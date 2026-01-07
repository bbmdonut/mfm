// As with the wd_mfm_decoder, this file decodes WD-formatted disks. This
// time, however, we're dealing with disks which were encoded in RLL format.
// This format may very well be used by other disks, with variations, but
// they will be identified and added over time.
//
// It seems to keep with the concept of 0xa1/0xfe (0xf8) id marks, so that
// logic should remain mostly the same. The core difference is in interpreting
// the deltas. While MFM is pretty standard across vendors, RLL has multiple
// possibilities.
//
// WD RLL decoding table (from Wikipedia):
//
// Source Data Bits          RLL-Encoded Transitions
//      11                         1000
//      10                         0100
//      000                        100100
//      010                        000100
//      011                        001000
//      0011                       00001000
//      0010                       00100100

// 03/31/25 BBMD Add initial RLL decoding process

// Copyright 2022 David Gesswein.
// This file is part of MFM disk utilities.
//
// MFM disk utilities is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// MFM disk utilities is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with MFM disk utilities.  If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "msg.h"
#include "crc_ecc.h"
#include "emu_tran_file.h"
#include "decoder_common.h"
#include "rll_decoder.h"
#include "deltas_read.h"

// Side of data to skip after header or data area.
#define HEADER_IGNORE_BYTES 10
// For data this is the write splice area where you get corrupted data that
// may look like a sector start byte.
#define DATA_IGNORE_BYTES 10

// This reverses the bit ordering in a byte. The controller writes
// the header data LSB first not the normal MSB first.
static unsigned char rev_lookup[16] = {
   0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
   0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };
#define REV_BYTE(n)( (rev_lookup[n&0xf] << 4) | rev_lookup[n>>4])

#define MARK_STORED -999

typedef struct rll_conversion_entry {
   // Number of source flux bits needed to match
   int flux_bit_count;
   // Pattern to match
   int flux_pattern;
   // Final output bit count
   int decoded_bit_count;
   // Final output bits
   int decoded_bits;
} RLL_CONVERSION_ENTRY;

RLL_CONVERSION_ENTRY rll_lookup_table[] = {
   // WD
   {4, 0b1000, 2, 0b11},
   {4, 0b0100, 2, 0b10},
   {6, 0b100100, 3, 0b000},
   {6, 0b000100, 3, 0b010},
   {6, 0b001000, 3, 0b011},
   {8, 0b00001000, 4, 0b0011},
   {8, 0b00100100, 4, 0b0010},
};

// Translate a flux pattern into actual data bits
//
// This takes the top 4/6/8 flux bits, and tries to map
// them to corresponding data bits, per the WD RLL lookup table.
// If nothing matches, returns 0. Otherwise, returns 1, along
// with the translated bits and the number of source bits that
// were matched.
int wd_rll_flux_translate(uint64_t raw, int raw_bits, unsigned int *trans_result, int *shift_val)
{
   int shift_bits = 0;
   // Intermediate value
   uint64_t shifted_flux_mask = 0;
   uint64_t shifted_flux_test = 0;

   for (int i = 0; i < ARRAYSIZE(rll_lookup_table); i++)
   {
      shift_bits = raw_bits - rll_lookup_table[i].flux_bit_count;
      shifted_flux_mask = ((1 << rll_lookup_table[i].flux_bit_count) - 1) << shift_bits;
      shifted_flux_test = rll_lookup_table[i].flux_pattern << shift_bits;

      if (shift_bits >= 0) {
         if ((raw & shifted_flux_mask) == shifted_flux_test) {
            // Found a match!
            *trans_result = rll_lookup_table[i].decoded_bits;
            *shift_val = rll_lookup_table[i].decoded_bit_count;
            return 1;
         }
      }
   }

   return 0;
}

// state: Current state in the decoding
// bytes: bytes to process
// crc: The crc of the bytes
// exp_cyl, exp_head: Track we think we are on
// sector_index: A sequential sector counter that may not match the sector
//    numbers
// drive_params: Drive parameters
// seek_difference: Return the difference between expected cylinder and
//   cylinder in header
// sector_status_list: The status of each sector (read errors etc)
// ecc_span: The maximum ECC span to correct (0 for no correction)
//
// Handle spared/alternate tracks for extracted data
SECTOR_DECODE_STATUS wd_process_rll_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status)
{
   static int sector_size;
   // Non zero if sector is a bad block, has alternate track assigned,
   // or is an alternate track
   static int bad_block, alt_assigned, is_alternate, alt_assigned_handled;
   static SECTOR_STATUS sector_status;
   // 0 after first sector marked spare/bad found. Only used for Adaptec
   static int first_spare_bad_sector = 1;

   if (*state == PROCESS_HEADER) {
      // Clear these since not used by all formats
      alt_assigned = 0;
      alt_assigned_handled = 0;
      is_alternate = 0;
      bad_block = 0;

      memset(&sector_status, 0, sizeof(sector_status));
      sector_status.status |= init_status | SECT_HEADER_FOUND;
      sector_status.ecc_span_corrected_header = ecc_span;
      if (ecc_span != 0) {
         sector_status.status |= SECT_ECC_RECOVERED;
      }

      if (drive_params->controller == CONTROLLER_WD_1006_RLL) {
         int sector_size_lookup[4] = {256, 512, 1024, 128};
         int cyl_high_lookup[16] = {0,1,2,3,-1,-1,-1,-1,4,5,6,7,-1,-1,-1,-1};
         int cyl_high;
         static int last_sector_group = 0;

         cyl_high = cyl_high_lookup[(bytes[1] & 0xf) ^ 0xe];
         sector_status.cyl = 0;
         if (cyl_high != -1) {
            sector_status.cyl = cyl_high << 8;
         }
         sector_status.cyl |= bytes[2];

         sector_status.head = rll_fix_head(drive_params, exp_head, bytes[3] & 0xf);
         sector_size = sector_size_lookup[(bytes[3] & 0x60) >> 5];
         bad_block = (bytes[3] & 0x80) >> 7;

         sector_status.sector = bytes[4];

         if (cyl_high == -1) {
            msg(MSG_INFO, "Invalid header id byte %02x on cyl %d,%d head %d,%d sector %d\n",
                  bytes[1], exp_cyl, sector_status.cyl,
                  exp_head, sector_status.head, sector_status.sector);
            sector_status.status |= SECT_BAD_HEADER;
         }
      } else {
         msg(MSG_FATAL,"Unknown controller type %d\n",drive_params->controller);
         exit(1);
      }

      if (sector_status.is_lba) {
         msg(MSG_DEBUG,
            "Got LBA %d exp %d,%d cyl %d head %d sector %d,%d size %d bad block %d\n",
               sector_status.lba_addr, exp_cyl, exp_head, sector_status.cyl,
               sector_status.head, sector_status.sector, *sector_index,
               sector_size, bad_block);
      } else {
         msg(MSG_DEBUG,
            "Got exp %d,%d cyl %d head %d sector %d,%d size %d bad block %d\n",
               exp_cyl, exp_head, sector_status.cyl, sector_status.head,
               sector_status.sector, *sector_index, sector_size, bad_block);
      }

      if (bad_block) {
         sector_status.status |= SECT_SPARE_BAD;
         msg(MSG_INFO,"Bad block set on cyl %d, head %d, sector %d\n",
               sector_status.cyl, sector_status.head, sector_status.sector);
      }
      if (is_alternate) {
         msg(MSG_DEBUG,"Alternate track set on cyl %d, head %d, sector %d\n",
               sector_status.cyl, sector_status.head, sector_status.sector);
      }

      rll_check_header_values(exp_cyl, exp_head, sector_index, sector_size,
            seek_difference, &sector_status, drive_params, sector_status_list);

      *state = MARK_DATA;
   } else { // Data
      // Value and where to look for header mark byte
      int id_byte_expected = 0xf8;
      int id_byte_index = 1;
      int id_byte_mask = 0xff;

      sector_status.status |= init_status;

      if (id_byte_index != -1 &&
            (bytes[id_byte_index] & id_byte_mask) != id_byte_expected &&
            crc == 0) {
         msg(MSG_INFO,"Invalid data id byte %02x expected %02x on cyl %d head %d sector %d\n",
               bytes[id_byte_index], id_byte_expected,
               sector_status.cyl, sector_status.head, sector_status.sector);
         sector_status.status |= SECT_BAD_DATA;
      }

      if (crc != 0) {
         sector_status.status |= SECT_BAD_DATA;
      }
      if (ecc_span != 0) {
         sector_status.status |= SECT_ECC_RECOVERED;
      }
      sector_status.ecc_span_corrected_data = ecc_span;
      // TODO: If bad sector number the stats such as count of spare/bad
      // sectors is not updated. We need to know the sector # to update
      // our statistics array. This happens with RQDX3
      if (!(sector_status.status & (SECT_BAD_HEADER | SECT_BAD_SECTOR_NUMBER))) {
         int dheader_bytes = controller_info[drive_params->controller].data_header_bytes;

         // Bytes[1] is because 0xa1 can't be updated from bytes since
         // won't get encoded as special sync pattern
         if (rll_write_sector(&bytes[dheader_bytes], drive_params, &sector_status,
               sector_status_list, &bytes[1], total_bytes-1) == -1) {
            sector_status.status |= SECT_BAD_HEADER;
         }
      }
      // Spare sectors normally are filled with same value if not used. This
      // may show if sector is used but not detected.
      if (sector_status.status & SECT_ANALYZE_SPARE && !(sector_status.status &
          SECT_BAD_DATA) && id_byte_index >= 0) {
         int spare_same = 1;
         for (int i = id_byte_index+2;
               i < id_byte_index + drive_params->sector_size + 1; i++ ) {
            if (bytes[i] != bytes[id_byte_index+1]) {
               spare_same = 0;
            }
         }
         if (!spare_same) {
            msg(MSG_INFO,"Spare sectors not all same value cyl %d head %d sector %d\n",
              sector_status.cyl, sector_status.head, sector_status.sector);
         }
      }
      if (alt_assigned && !alt_assigned_handled) {
         msg(MSG_INFO,"Assigned alternate track not corrected on cyl %d, head %d, sector %d\n",
               sector_status.cyl, sector_status.head, sector_status.sector);
      }
      // OMTI_20L only has one sector header so stay in MARK_DATA and inc
      // sector count

      *state = MARK_ID;
   }

   return sector_status.status;
}

// Decode a track's worth of deltas.
//
//
// drive_params: Drive parameters
// cyl,head: Physical Track data from
// deltas: RLL delta data to decode
// seek_difference: Return of difference between expected cyl and header
// sector_status_list: Return of status of decoded sector
// return: Or together of the status of each sector decoded
SECTOR_DECODE_STATUS wd_decode_rll_track(DRIVE_PARAMS *drive_params, int cyl,
      int head, uint16_t deltas[], int *seek_difference,
      SECTOR_STATUS sector_status_list[])
{
   // This is the raw RLL data decoded
   uint64_t raw_word = 0;
   // Index of the next raw_flux bit to insert
   int raw_bit_cntr = 0;
   // The decoded bits
   unsigned int decoded_word = 0;
   // Counter to know when we have a bytes' worth
   int decoded_bit_cntr = -1;
   // loop counter
   int i;
   // Number of bits we should shift around in order
   // to get one raw flux bit pattern for testing/removal
   int shift_value = 0;
   // These are variables for the PLL filter. avg_bit_sep_time is the
   // "VCO" frequency
   float avg_bit_sep_time;     // 200 MHz clocks
   float nominal_bit_sep_time; // 200 MHz clocks
   // Clock time is the clock edge time from the VCO.
   float clock_time = 0;
   // How many bits the last delta corresponded to
   int int_bit_pos;
   float filter_state = 0;
   // Time in track for debugging
   int track_time = 0;
   // Counter for debugging
   int tot_raw_bit_cntr = 0;
   // Where we are in decoding a sector, Start looking for header ID mark
   STATE_TYPE state = MARK_ID;
   // Status of decoding returned
   SECTOR_DECODE_STATUS all_sector_status = SECT_NO_STATUS;
   // How many zeros we need to see before we will look for the 0xa1 byte.
   // When write turns on and off can cause codes that look like the 0xa1
   // so this avoids them. Some drives seem to have small number of
   // zeros after sector marked bad in header.
#define MARK_NUM_ZEROS 2
   int zero_count = 0;
   // Number of deltas available so far to process
   int num_deltas;
   // And number from last time
   int last_deltas = 0;
   // If we get too large a delta we need to process it in less than 32 bit
   // word number of bits. This holds remaining number to process
   int remaining_delta = 0;
   // Maximum delta to process in one pass
   int max_delta = 0;
   // Intermediate value
   int tmp_raw_word = 0;
   // Collect bytes to further process here
   uint8_t bytes[MAX_SECTOR_SIZE + 50];
   // How many we need before passing them to the next routine
   int bytes_needed = 0;
   int header_bytes_needed = 0;
   // Length to perform CRC over
   int bytes_crc_len = 0;
   int header_bytes_crc_len = 0;
   // how many we have so far
   int byte_cntr = 0;
   // Sequential counter for counting sectors
   int sector_index = 0;
   // Count all the raw bits for emulation file
   int all_raw_bits_count = 0;
   // Bit count of last of header found
   int header_raw_bit_count = 0;
   // Bit delta between last header and previous header
   int header_raw_bit_delta = 0;
   // First address mark time in ns
   int first_addr_mark_ns = 0;
   // Decoded bits for one valid RLL pattern
   unsigned int translated_result = 0;

   unsigned int header_byte_cntr = 0;
   unsigned int data_byte_cntr = 0;

   num_deltas = deltas_get_count(0);
   raw_word = 0;
   nominal_bit_sep_time = 200e6 /
       controller_info[drive_params->controller].clk_rate_hz;
   max_delta = nominal_bit_sep_time * 22;
   avg_bit_sep_time = nominal_bit_sep_time;
   i = 1;
   while (num_deltas >= 0) {
      // We process what we have then check for more.
      for (; i < num_deltas;) {
         int delta_process;
         // If no remaining delta process next else finish remaining
         if (remaining_delta == 0) {
            delta_process = deltas[i++];
            remaining_delta = delta_process;
         }  else {
            delta_process = remaining_delta;
         }
         // Don't overflow our 32 bit word
         if (delta_process > max_delta) {
            delta_process = max_delta;
         }
         track_time += delta_process;
         // This is simulating a PLL/VCO clock sampling the data.
         clock_time += delta_process;
         remaining_delta -= delta_process;
         // Move the clock in current frequency steps and count how many bits
         // the delta time corresponds to
         for (int_bit_pos = 1; clock_time > avg_bit_sep_time / 2;
               clock_time -= avg_bit_sep_time, int_bit_pos++) {
         }
         int_bit_pos--; // Account for bit differences in delta parsing
         // And then filter based on the time difference between the delta and
         // the clock. Don't update PLL if this is a long burst without
         // transitions
         if (remaining_delta == 0) {
            avg_bit_sep_time = nominal_bit_sep_time + filter(clock_time, &filter_state);
         }
         // Shift based on number of bit times, then put in the 1 from the
         // delta. If we had a delta greater than the size of raw word we
         // will lose the unprocessed bits in raw_word. This is unlikely
         // to matter since this is invalid data, so the disk had a long
         // drop out, so many more bits are lost.
         if (int_bit_pos >= sizeof(raw_word)*8) {
            raw_word = 1;
         } else {
            raw_word = (raw_word << int_bit_pos) | 1;
         }
         tot_raw_bit_cntr += int_bit_pos;
         raw_bit_cntr += int_bit_pos;

         // See if we have our golden marker(s)
         if (((state == MARK_ID) || (state == MARK_DATA)) &&
            ((raw_word & 0b11111111111111111) == 0b10000000100100001)) {
               //  1000 0000 1001 0000 (8090)
               // This is the "0xA1" marker for RLL encoding. Start collecting bytes after this.

               raw_bit_cntr = 1;
               tot_raw_bit_cntr = 1;
               int_bit_pos = 0;
               decoded_word = 0;
               decoded_bit_cntr = 0;
               state = (state == MARK_ID) ? PROCESS_HEADER : PROCESS_DATA;
               bytes[byte_cntr++] = 0xa1;
         } else if ((state == PROCESS_HEADER) || (state == PROCESS_DATA)) {
            while (raw_bit_cntr >= 16) {
               if (wd_rll_flux_translate(raw_word, raw_bit_cntr, &translated_result, &shift_value))
               {
                  // We managed to translate a flux pattern. Add it to our
                  // decoded word.
                  decoded_word = (decoded_word << shift_value) | translated_result;
                  decoded_bit_cntr += shift_value;
                  // We return the number of decoded bits, so we multiply by 2
                  // to get the raw bits required (since the RLL encoding format
                  // is a 2-to-1 mapping)
                  raw_bit_cntr -= (shift_value * 2);
                  tot_raw_bit_cntr -= (shift_value * 2);
                  raw_word = ((raw_word) & (uint64_t)((1 << raw_bit_cntr) - 1));
               } else {
                  // There was no translation for the flux pattern sent.
                  // This most likely means the delta data is not valid
                  // for some reason.
                  // If we stay in this state, we'll be stuck forever.
                  // To help continue processing - though the data may be
                  // potentially corrupted after this - we'll drop the MSBit.
                  raw_bit_cntr -= 1;
                  tot_raw_bit_cntr -= 1;
                  decoded_bit_cntr = 0;
               }
            }
            while (decoded_bit_cntr >= 8)
            {
               tmp_raw_word = (decoded_word >> (decoded_bit_cntr - 8)) & 0xff;
               bytes[byte_cntr++] = tmp_raw_word;
               if (state == PROCESS_HEADER) {
                  header_byte_cntr++;
               } else if (state == PROCESS_DATA) {
                  data_byte_cntr++;
               }
               decoded_bit_cntr -= 8;

               // Entire header loaded. Update our status based on if header is valid.
               if ((state == PROCESS_HEADER) && (header_byte_cntr == 6))
               {
                  uint64_t crc;
                  int ecc_span;
                  SECTOR_DECODE_STATUS init_status = 0;
                  SECTOR_DECODE_STATUS test_status = 0;

                  // We have the entirety of the header, with CRC value. Test it for validity.

                  header_bytes_crc_len = controller_info[drive_params->controller].header_bytes +
                     drive_params->header_crc.length / 8;
                  header_bytes_needed = header_bytes_crc_len + HEADER_IGNORE_BYTES;
                  bytes_crc_len = header_bytes_crc_len;
                  bytes_needed = header_bytes_needed;

                  test_status = rll_crc_bytes(drive_params, bytes, header_bytes_crc_len, PROCESS_HEADER, &crc, &ecc_span, &init_status, 0);
                  if ((crc == 0) && !(init_status & SECT_AMBIGUOUS_CRC) && (drive_params->header_crc.poly != 0))
                  {
                     all_sector_status |= rll_process_bytes(drive_params, bytes,
                        bytes_crc_len, bytes_needed, &state, cyl, head,
                        &sector_index, seek_difference, sector_status_list, 0
                     );
                     state = MARK_DATA;
                     header_byte_cntr = 0;
                     data_byte_cntr = 0;
                     byte_cntr = 0;
                  }
               } else if ((state == PROCESS_DATA) && (data_byte_cntr == 520)) {

                  bytes_crc_len = controller_info[drive_params->controller].data_header_bytes +
                           controller_info[drive_params->controller].data_trailer_bytes +
                           drive_params->sector_size +
                           drive_params->data_crc.length / 8;
                  bytes_needed = DATA_IGNORE_BYTES + bytes_crc_len;

                  all_sector_status |= rll_process_bytes(drive_params, bytes,
                     bytes_crc_len, bytes_needed, &state, cyl, head,
                     &sector_index, seek_difference, sector_status_list, 0
                  );
                  state = MARK_ID;
                  header_byte_cntr = 0;
                  data_byte_cntr = 0;
                  byte_cntr = 0;
               }
            }
         }
      }
      // Finished what we had, any more?
      // If we didn't get too many last time sleep so delta reader can run.
      // Thread priorities might be better.
      if (num_deltas - last_deltas <= 2000) {
         usleep(500);
      }
      last_deltas = num_deltas;
      num_deltas = deltas_get_count(i);
   }

   // If we didn't find anything to decode return header error
   if (all_sector_status == SECT_NO_STATUS) {
      all_sector_status = SECT_BAD_HEADER;
   }

   return all_sector_status;
}
