// These are the general routines for supporting RLL decoders.
// TODO (BBMD): Update comment block to match call chain per mfm_decoder header
// TODO: make it use sector number information and checking CRC at data length to write data
// for sectors with bad headers. See if resyncing PLL at write boundaries improves performance when
// data bits are shifted at write boundaries.
//
// 01/12/26 BBMD Moved code common to both MFM and RLL operations out into
//    decoder_common, and created this file for RLL-related decoding

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
#include <limits.h>

#include "msg.h"
#include "crc_ecc.h"
#include "emu_tran_file.h"
#include "rll_decoder.h"
#include "deltas_read.h"

#define ARRAYSIZE(x)  (sizeof(x) / sizeof(x[0]))

// This routine calls the proper decoder for the drive format
// See called routines for parameters and return value
SECTOR_DECODE_STATUS rll_decode_track(DRIVE_PARAMS * drive_params, int cyl,
      int head, uint16_t deltas[], int *seek_difference,
      SECTOR_STATUS sector_status_list[])
{
   int rc = 0;
   int i;

   msg(MSG_DEBUG, "rll_decode_track: Start for C/H: %d / %d\n", cyl, head);
   for (i = 0; i < drive_params->num_sectors; i++) {
      sector_status_list[i].last_status = SECT_BAD_HEADER;
   }

   // Change in rll_process_bytes if this "if" block is changed
   if (drive_params->controller == CONTROLLER_WD_1006_RLL) {
      rc = wd_decode_rll_track(drive_params, cyl, head, deltas, seek_difference,
            sector_status_list);
   }
   update_stats(drive_params, cyl, head, sector_status_list);
   msg(MSG_DEBUG, "rll_decode_track: rc = %x\n", rc);
   return rc;
}

// Perform CRC check of data bytes.
// drive_params: Drive parameters
// bytes: bytes to process
// bytes_crc_len: Length of bytes including CRC
// state: Where we are in the decoding process
// *crc: Zero if no CRC error
// *ecc_span: Number of bits corrected with ECC to fix CRC error
// *init_status: Set to SECT_AMBIGUOUS_CRC if zero CRC may be due to zero data
// perform_ecc: Non zero if ECC corrections should be performed
//
SECTOR_DECODE_STATUS rll_crc_bytes(DRIVE_PARAMS *drive_params,
   uint8_t bytes[], int bytes_crc_len, int state, uint64_t *crc_ret,
   int *ecc_span, SECTOR_DECODE_STATUS *init_status, int perform_ecc)
{
   uint64_t crc;
   // CRC to use to process these bytes
   CRC_INFO crc_info;
   // Start byte for CRC decoding
   int start;
   SECTOR_DECODE_STATUS status = SECT_NO_STATUS;
   CHECK_TYPE check_type;

   if (state == PROCESS_HEADER) {
      start = controller_info[drive_params->controller].header_crc_ignore;
      crc_info = drive_params->header_crc;
      check_type = controller_info[drive_params->controller].header_check;
      msg(MSG_DEBUG, "rll_crc_bytes: start = %d, crc_info.length = %d, check_type = %d\n", start, crc_info.length, check_type);
   } else {
      start = controller_info[drive_params->controller].data_crc_ignore;
      crc_info = drive_params->data_crc;
      check_type = controller_info[drive_params->controller].data_check;
   }

   if (check_type == CHECK_CHKSUM) {
      crc = checksum64(&bytes[start], bytes_crc_len-crc_info.length/8-start, &crc_info);
      if (crc_info.length == 8) {
        crc = crc & 0xff;
        if (crc == bytes[bytes_crc_len-1]) {
           crc = 0;
        }
      } else if (crc_info.length == 16) {
         crc = crc & 0xff;
         if (crc == bytes[bytes_crc_len-2] &&
               crc == (bytes[bytes_crc_len-1] ^ 0xff)) {
            crc = 0;
         } else {
            msg(MSG_DEBUG, "sum %02llx: %02x, %02x\n", crc,
               bytes[bytes_crc_len-2], bytes[bytes_crc_len-1]);
            crc = 1; // Non zero indicates error
         }
      } else if (crc_info.length == 32) {
         crc = crc & 0xffff;
         uint16_t chksum1, chksum2;
         chksum1 = ((uint16_t) bytes[bytes_crc_len-4] << 8) | bytes[bytes_crc_len-3];
         chksum2 = ((uint16_t) bytes[bytes_crc_len-2] << 8) | bytes[bytes_crc_len-1];
         if (crc == chksum1 && crc == (chksum2 ^ 0xffff)) {
            crc = 0;
         } else {
            msg(MSG_DEBUG, "sum %04llx: %04x, %04x\n", crc, chksum1, chksum2);
            crc = 1; // Non zero indicates error
         }
      } else {
         msg(MSG_FATAL, "Invalid checksum length %d\n",crc_info.length);
         exit(1);
      }
   } else if (check_type == CHECK_NONE) {
      crc = 0;
   // TODO: Probably should make these use CRC length field and put in crc_ecc.c
   } else if (check_type == CHECK_XOR8) {
      uint8_t x1 = 0;
      int i;
      for (i = start; i < bytes_crc_len; i++) {
         x1 ^= bytes[i];
      }
      crc = x1 ^ 0xff ;
   } else if (check_type == CHECK_XOR16) {
      uint8_t x1 = 0, x2 = 0;
      int i;
      for (i = start; i < bytes_crc_len - crc_info.length/8; i += 2) {
         x1 ^= bytes[i];
      }
      for (i = start+1; i < bytes_crc_len - crc_info.length/8; i += 2) {
         x2 ^= bytes[i];
      }
      crc = (bytes[i] != x2) || (bytes[i+1] != x1) ;
   } else if (check_type == CHECK_CRC) {
      int i;

      crc = crc64(&bytes[start], bytes_crc_len-start, &crc_info);
      // If all the data and CRC is zero and CRC returns zero
      // mark it as ambiguous crc since any polynomial will match
      if (crc == 0) {
         for (i = start; i < bytes_crc_len; i++) {
            if (bytes[i] != 0) {
               break;
            }
         }
         if (i == bytes_crc_len) {
            *init_status |= SECT_AMBIGUOUS_CRC;
         }
      }
   } else if (check_type == CHECK_PARITY) {
      crc = eparity64(&bytes[start], bytes_crc_len - start, &drive_params->header_crc) != 1;
   } else {
      msg(MSG_FATAL, "Unknown check type %d\n", check_type);
      exit(1);
   }
   // Zero CRC is no error
   if (crc == 0 && !(*init_status & SECT_AMBIGUOUS_CRC)) {
      if (state == PROCESS_HEADER) {
         status |= SECT_ZERO_HEADER_CRC;
      } else {
         status |= SECT_ZERO_DATA_CRC;
      }
   }

   if (crc != 0) {
      // If ECC correction enabled then perform correction up to length
      // specified
      if (crc_info.ecc_max_span != 0 && perform_ecc) {
         *ecc_span = ecc64(&bytes[start], bytes_crc_len-start, crc, &crc_info);
         // TODO: This includes SECT_SPARE_BAD ECC corrections in the
         // final value printed. We don't have the info to fix here
         if (*ecc_span != 0) {
            drive_params->stats.max_ecc_span = MAX(*ecc_span,
                  drive_params->stats.max_ecc_span);
            crc = 0; // No longer have CRC error
         }
      }
   }

   *crc_ret = crc;
   return status;
}

// After we have found a valid header/data mark this routine is
// used to process the bytes. It checks the CRC and does ECC if needed then
// calls wd_process_data to finish the processing.
//
// drive_params: Drive parameters
// bytes: bytes to process
// bytes_crc_len: Length of bytes including CRC
// state: Where we are in the decoding process
// cyl,head: Physical Track data from
// sector_index: Sequential sector counter
// seek_difference: Return of difference between expected cyl and header
// sector_status_list: Return of status of decoded sector
// return: Status of sector decoded
// TODO: Would be good if data doesn't decode as data to try as header
// If data mark missed will process next sector header as data so one
// sector of data that should be recoverable will be lost.
SECTOR_DECODE_STATUS rll_process_bytes(DRIVE_PARAMS *drive_params,
   uint8_t bytes[], int bytes_crc_len, int total_bytes,
   STATE_TYPE *state, int cyl, int head,
   int *sector_index, int *seek_difference,
   SECTOR_STATUS sector_status_list[], SECTOR_DECODE_STATUS init_status) {

   uint64_t crc;
   int ecc_span = 0;
   SECTOR_DECODE_STATUS status = SECT_NO_STATUS;
   char *name;

   status = rll_crc_bytes(drive_params, bytes, bytes_crc_len, *state, &crc,
      &ecc_span, &init_status, 1);
   if (*state == PROCESS_HEADER) {
#if DUMP_HEADER
      static int dump_fd = 0;
      static int first = 1;
      if (first) {
         printf("Dumping for %d bytes crc len %d\n",bytes_crc_len,
           drive_params->header_crc.length/8);
         first = 0;
      }
      if  (dump_fd == 0) {
         dump_fd = open("dumpheader",O_WRONLY | O_CREAT | O_TRUNC, 0666);
      }
      write(dump_fd, bytes, bytes_crc_len);
#endif
      //if ((msg_get_err_mask() & MSG_DEBUG_DATA) && crc == 0) {
      /*if (msg_get_err_mask() & MSG_DEBUG_DATA) {
         mfm_dump_bytes(bytes, bytes_crc_len, cyl, head, *sector_index,
               MSG_DEBUG_DATA);
      }*/
      name = "header";
   } else {
#if DUMP_DATA
      static int dump_fd = 0;
      static int first = 1;
      if (first) {
         printf("Dumping for %d bytes crc len %d\n",bytes_crc_len,
           drive_params->data_crc.length/8);
         first = 0;
      }
      if  (dump_fd == 0) {
         dump_fd = open("dumpdata",O_WRONLY | O_CREAT | O_TRUNC, 0666);
      }
      write(dump_fd, bytes, bytes_crc_len);
#endif
      /*if (msg_get_err_mask() & MSG_DEBUG_DATA) {
         mfm_dump_bytes(bytes, bytes_crc_len, cyl, head, *sector_index,
               MSG_DEBUG_DATA);
      }*/
      name = "data";
   }
   if (crc != 0 || ecc_span != 0) {
      msg(MSG_DEBUG,"Bad CRC %s cyl %d head %d sector index %d\n",
            name, cyl, head, *sector_index);
   }

   // If no error process. Only process with errors if data. Without
   // valid header we don't know what sector we are decoding.
   if (*state != PROCESS_HEADER || crc == 0 || ecc_span != 0) {

      // If this is changed change in rll_decode_track also
      if (drive_params->controller == CONTROLLER_WD_1006_RLL) {
         status |= wd_process_rll_data(state, bytes, total_bytes, crc, cyl,
               head, sector_index,
               drive_params, seek_difference, sector_status_list, ecc_span,
               init_status);
      } else {
         msg(MSG_FATAL, "Unexpected controller %d\n",
               drive_params->controller);
         exit(1);
      }
    } else {
      // Wern't able to process header to mark invalid if we haven't seen all
      // expected headers. False header detection can occur in the junk at
      // the end of the track.
      if (*sector_index < drive_params->num_sectors) {
         status |= SECT_BAD_HEADER;
      }
      // Search for header in case we are out of sync. If we found
      // data next we can't process it anyway.
      *state = MARK_ID;
   }
   msg(MSG_DEBUG, "rll_process_bytes: Returning status of: %x\n", status);
   return status;
}

// Perform 3 head bit correction
//
// drive_params: Drive parameters
// head: Head value from header
// exp_head: Expected head
// return: Corrected head value
int rll_fix_head(DRIVE_PARAMS *drive_params, int exp_head, int head) {
   // WD 1003 controllers wrote 3 bit head code so head 8 is written as 0.
   // If requested and head seems correct fix read head value.
//printf("3 bit %d, head %d exp %d\n",drive_params->head_3bit, head, exp_head);
   if (drive_params->head_3bit && head == (exp_head & 0x7)) {
      return exp_head;
   } else {
      return head;
   }
}

// This checks that the sector header values are reasonable and match the
// track and head we thought we were reading. If the cylinder doesn't match
// we return the difference between the actual and expected cylinder so
// the caller can try seeking again if needed.
//
// It also updates the sector_status_list with the information gotten from
// the sector header.
//
// exp_cyl, exp_head: Track data was from
// sector_index: Counter for sectors starting at 0. With errors may not match
//   sector number
//  sector_size: Sector size from header
// seek_difference: Return of cylinder difference
// sector_status: Status of this sector
// drive_params: Parameters for drive
void rll_check_header_values(int exp_cyl, int exp_head,
      int *sector_index, int sector_size, int *seek_difference,
      SECTOR_STATUS *sector_status, DRIVE_PARAMS *drive_params,
      SECTOR_STATUS sector_status_list[]) {

   if (sector_status->ignore) {
      return;
   }

   if (drive_params->ignore_header_mismatch) {
      sector_status->logical_sector = *sector_index;
      (*sector_index)++;
      return;
   }

   if (sector_status->cyl >= 0 && sector_status->cyl < ARRAYSIZE(cyl_found)) {
      cyl_found[sector_status->cyl] = 1;
   }
   // If ignore seek error we will still declare an error if greater than 250
   // to make analyze work better.
   if (!sector_status->is_lba &&
       (sector_status->head != exp_head ||
        (sector_status->cyl != exp_cyl && !drive_params->ignore_seek_errors) ||
         abs(sector_status->cyl - exp_cyl) > 250)) {
      // Possibly a seek error, mark it if header isn't declared bad. If
      // drive uses bad CRC with initial value 0 non header data can pass
      // CRC hopefully will have BAD_HEADER set.
      // TODO: Should we not do any of these checks with bad header?
      if (sector_status->cyl != exp_cyl && !
          (sector_status->status & SECT_BAD_HEADER)) {

         msg(MSG_ERR,"Mismatch cyl %d,%d head %d,%d index %d\n",
            exp_cyl, sector_status->cyl, exp_head, sector_status->head,
            *sector_index);
         sector_status->status |= ANALYZE_WRONG_FORMAT;
         sector_status->status |= SECT_WRONG_CYL;
         if (seek_difference != NULL) {
            *seek_difference = exp_cyl - sector_status->cyl;
         }
      }
      sector_status->status |= SECT_BAD_HEADER;
   }
   // Verify LBA address is somewhat sensible. If not say wrong format for analyze
   if (sector_status->is_lba && sector_status->lba_addr >
      2*((exp_cyl + 1) * drive_params->num_head * drive_params->num_sectors)) {
         sector_status->status |= ANALYZE_WRONG_FORMAT;
   }
   // If we have expected sector ordering information check the sector numbers
   // TODO: make this handle more complex sector numbering where they vary
   // between tracks
   if (drive_params->sector_numbers != NULL) {
      int orig_sector_index = *sector_index;
      for (; *sector_index < drive_params->num_sectors; (*sector_index)++) {
         if (sector_status->sector == drive_params->sector_numbers[*sector_index]) {
            break;
         }
      }
      if (*sector_index > orig_sector_index+1) {
         msg(MSG_ERR, "Cyl %d head %d Missed sector between %d(%d) and %d(%d)\n",
               sector_status->cyl, sector_status->head,
               drive_params->sector_numbers[orig_sector_index], orig_sector_index,
               drive_params->sector_numbers[*sector_index], *sector_index);
      }
      if (*sector_index >= drive_params->num_sectors) {
         msg(MSG_ERR_SERIOUS,"Cyl %d head %d Sector %d not found in expected sector list after %d(%d)\n",
               sector_status->cyl, sector_status->head,
               sector_status->sector,
               drive_params->sector_numbers[orig_sector_index],
               orig_sector_index);
         sector_status->status |= SECT_BAD_HEADER;
         sector_status->status |= ANALYZE_WRONG_FORMAT;
         *sector_index = orig_sector_index;
      }
      sector_status->logical_sector = *sector_index;
   } else {
      sector_status->logical_sector = *sector_index;
      (*sector_index)++;
   }
   if (sector_size != drive_params->sector_size) {
      msg(MSG_ERR,"Expected sector size %d header says %d cyl %d head %d sector %d\n",
            drive_params->sector_size, sector_size, sector_status->cyl,
            sector_status->head, sector_status->sector);
      sector_status->status |= ANALYZE_WRONG_FORMAT;
   }
   //  Code copies from rll_write_sector
   //  so sectors marked bad will be properly handled. Something cleaner
   //  would be good.
   int sect_rel0 = sector_status->sector - drive_params->first_sector_number;
   if ((sect_rel0 >= drive_params->num_sectors || sect_rel0 < 0) &&
         !(sector_status->status & SECT_BAD_SECTOR_NUMBER)) {
      msg(MSG_ERR_SERIOUS, "Logical sector %d out of range 0-%d sector %d cyl %d head %d phys sector %d\n",
         sect_rel0, drive_params->num_sectors-1, sector_status->sector,
         sector_status->cyl,sector_status->head, *sector_index);
      sector_status->status |= SECT_BAD_HEADER;
      sector_status->status |= ANALYZE_WRONG_FORMAT;
   } else if (sector_status->head > drive_params->num_head) {
      msg(MSG_ERR_SERIOUS,"Head out of range %d max %d cyl %d sector %d\n",
         sector_status->head, drive_params->num_head,
         sector_status->cyl, sector_status->sector);
      sector_status->status |= SECT_BAD_HEADER;
      sector_status->status |= ANALYZE_WRONG_FORMAT;
   }
   if (sect_rel0 < MAX_SECTORS) {
      int sector_to_update = sect_rel0;

      // If < 0 mark bad in last sector for analyze.
      if (sector_to_update < 0) {
         sector_to_update = MAX_SECTORS-1;
      }
      // If we haven't written the sector update the sector status info. If
      // we have written we don't need to update here. Updating could change
      // sector data indicating good sector written to bad
      if (sector_status_list[sector_to_update].status & SECT_NOT_WRITTEN) {
         int last_status = sector_status_list[sector_to_update].last_status;
         sector_status_list[sector_to_update] = *sector_status;
            // Keep existing last status
         sector_status_list[sector_to_update].last_status = last_status;
               // Set to bad data as default. If data found good this will
               // be changed. Keep not written flag.
         sector_status_list[sector_to_update].status |= SECT_BAD_DATA | SECT_NOT_WRITTEN;
      }
   }
}

// Write the sector data to file. We only write the best data so if the
// caller retries read with error we won't overwrite good data if this
// read has an error for this sector but the previous didn't.
// Bytes is data to write.
// drive_params: specifies the length and other information.
// sector_status: is the status of the sector writing
// sector_status_list: is the status of the data that would be written to the
//    file. Status is still updated if drive_params->ext_fd is -1 to prevent
//    writing.
// all_bytes: Includes data header bytes, used for emulator file writing
// all_bytes_len: Length of all_bytes
// return: -1 if error found, 0 if OK.
int rll_write_sector(uint8_t bytes[], DRIVE_PARAMS * drive_params,
      SECTOR_STATUS *sector_status, SECTOR_STATUS sector_status_list[],
      uint8_t all_bytes[], int all_bytes_len)
{
   int rc;
   STATS *stats = &drive_params->stats;
   int update;
   off_t offset;

   if (sector_status->ignore) {
      return 0;
   }

   // Some disks number sectors starting from 1. We need them starting
   // from 0.
   int sect_rel0 = sector_status->sector - drive_params->first_sector_number;

   // Collect statistics
   stats->max_sect = MAX(sector_status->sector, stats->max_sect);
   stats->min_sect = MIN(sector_status->sector, stats->min_sect);
   stats->max_head = MAX(sector_status->head, stats->max_head);
   stats->min_head = MIN(sector_status->head, stats->min_head);
   stats->max_cyl = MAX(sector_status->cyl, stats->max_cyl);
   stats->min_cyl = MIN(sector_status->cyl, stats->min_cyl);

   // Check for sector and head in range to prevent bad writes.
   // We don't check cyl against max since if it exceeds it things
   // will still work properly.
   if (sect_rel0 >= drive_params->num_sectors || sect_rel0 < 0) {
      msg(MSG_ERR_SERIOUS, "Logical sector %d out of range 0-%d sector %d cyl %d head %d\n",
            sect_rel0, drive_params->num_sectors-1, sector_status->sector,
            sector_status->cyl,sector_status->head);
      return -1;
   }
   if (sector_status->head > drive_params->num_head) {
      msg(MSG_ERR_SERIOUS,"Head out of range %d max %d cyl %d sector %d\n",
            sector_status->head, drive_params->num_head,
            sector_status->cyl, sector_status->sector);
      return -1;
   }

   sector_status_list[sect_rel0].last_status = sector_status->status;
   // If not written then write data. Otherwise only write if likely to be
   // better than sector previously written. Better is if we didn't get a CRC
   // error, or the ECC correction span is less than the last one.
   // We assume the header data is correct so we don't check if the header
   // ECC correction is better. If the header wasn't right we wrote the
   // data to the wrong spot in the file.
   update = 0;
   // If the previous header was bad update
   if (sector_status_list[sect_rel0].status & SECT_BAD_HEADER) {
      update = 1;
   }
   // If we haven't written the sector yet write it even if bad
   if (sector_status_list[sect_rel0].status & SECT_NOT_WRITTEN) {
      sector_status_list[sect_rel0].status &= ~SECT_NOT_WRITTEN;
      update = 1;
   }
   // If current read isn't bad
   if ( !(sector_status->status & SECT_BAD_DATA)) {
      // If last was bad then update
      if (sector_status_list[sect_rel0].status & SECT_BAD_DATA) {
         update = 1;
      }
      // If previous had ECC correction and current correction is less then update
      if ((sector_status_list[sect_rel0].ecc_span_corrected_data > 0 &&
             (sector_status->ecc_span_corrected_data == 0 ||
             sector_status->ecc_span_corrected_data <
             sector_status_list[sect_rel0].ecc_span_corrected_data)) ||
           (drive_params->ignore_seek_errors &&
             sector_status->cyl != sector_status_list[sect_rel0].cyl )) {
         update = 1;
      }
   }
   // If LBA number bad don't update
   if (sector_status->status & SECT_BAD_LBA_NUMBER) {
      // Update status that would normally be updated when sector written
      sector_status_list[sect_rel0] = *sector_status;
      update = 0;
   }
   // Always update errors in emu data in case it ends up being used as
   // the best data to write
   //
   // TODO (BBMD): Implement the RLL version of this function.
   // This will take some time to look at, as the MFM data version is a
   // bit more consistent in encoding (since RLL differs in both encoded
   // data translations plus size differences of encoded bits based on the
   // source data bit patterns)
   //
   // This can be pushed back until we implement the emulation of RLL itself.
   //
   /*   update_emu_track_sector(drive_params, sector_status, sect_rel0,
      all_bytes, all_bytes_len, update);   */

   // Only write best sector when in ignore_seek_error mode. Since can get
   // same sector from reads that are supposed to be from different cylinders
   // the normal check can't determine which is best.
   if (update && (drive_params->ignore_seek_errors)) {
      CRC_INFO crc_info = {0x12345678, 0x140a0445000101ll, 56, 0};

      uint64_t crc = crc64(bytes, drive_params->sector_size, &crc_info);
      // If good read or previous good read see if data different. Good is CRC
      // matches with or without ECC correction
      if (!(sector_status->status & SECT_BAD_DATA)) {
         if (sector_good[sector_status->cyl][sector_status->head][sect_rel0] != SECTOR_GOOD_BAD) {
            if (crc != sector_crc[sector_status->cyl][sector_status->head][sect_rel0]) {
               // If neither current read or previous were corrected by ECC
               if (sector_status->ecc_span_corrected_data == 0 &&
                   (sector_good[sector_status->cyl][sector_status->head][sect_rel0] & SECTOR_GOOD_ECC_MASK) == 0) {
                  msg(MSG_INFO, "Found two reads of sector with different content cyl %d head %d sect %d\n",sector_status->cyl,
                      sector_status->head,sect_rel0);
                  // Keep first. Found disk with old format data at end. This
                  // will report these sectors as bad since they won't
                  // be written which updates the sector status list
                  update = 0;
               } else {
                  msg(MSG_INFO, "Miscorrected ECC cyl %d head %d sect %d\n",sector_status->cyl,
                     sector_status->head,sect_rel0);
               }
            }
         }
         // Update CRC so we can see if we are getting false ECC corrections
         sector_crc[sector_status->cyl][sector_status->head][sect_rel0] = crc;
      }
      // ECC correction, Span is only set if we don't have CRC error after correction
      uint8_t new_good;
      if (sector_status->status & SECT_BAD_DATA) {
         new_good = SECTOR_GOOD_BAD;
      } else if (sector_status->ecc_span_corrected_data > 0) {
         new_good = sector_status->ecc_span_corrected_data;
      } else {
         new_good = SECTOR_GOOD_GOOD;
      }
      // Need to replace same at least once to write data with CRC error.
      if (sector_good[sector_status->cyl][sector_status->head][sect_rel0] < new_good) {
         //printf("Not replacing better sector %d %d\n",sector_good[sector_status->cyl][sector_status->head][sect_rel0], new_good);
         update = 0;
      } else {
         sector_good[sector_status->cyl][sector_status->head][sect_rel0] = new_good;
      }
   }
   if (update) {
      if (drive_params->ext_fd >= 0) {
         if (sector_status->is_lba) {
            offset = (off_t) sector_status->lba_addr * drive_params->sector_size;
         } else {
            offset = (sect_rel0) * drive_params->sector_size +
               sector_status->head * (drive_params->sector_size *
                     drive_params->num_sectors) +
                     (off_t) sector_status->cyl * (drive_params->sector_size *
                           drive_params->num_sectors *
                           drive_params->num_head);
         }
         if (lseek(drive_params->ext_fd, offset, SEEK_SET) < 0) {
            msg(MSG_FATAL, "Seek failed decoded data: %s\n", strerror(errno));
            exit(1);
         };
         if ((rc = write(drive_params->ext_fd, bytes, drive_params->sector_size)) !=
               drive_params->sector_size) {
            msg(MSG_FATAL, "Write failed, rc %d: %s", rc, strerror(errno));
            exit(1);
         }
      }
      sector_status_list[sect_rel0] = *sector_status;

   }
   sector_status_list[sect_rel0].last_status = sector_status->status;

   return 0;
}
