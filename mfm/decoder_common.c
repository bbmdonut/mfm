#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "crc_ecc.h"
#include "emu_tran_file.h"
#include "decoder_common.h"
#include "track_definitions.h"
#include "msg.h"
#include "mfm_decoder.h"

// This is used with --ignore_seek_errors to show what sectors were good/bad
// for the entire disk. It also makes sure a good sector won't be overwritten with a
// bad sector. A disk I was reading you saw multiple cylinders when
// reading a track so track at a time error information wasn't accurate.
//    Lower value better data.
//    sector_good & ECC MASK not zero number of bits corrected.
uint8_t sector_good[MAX_CYL][MAX_HEAD][MAX_SECTORS];

// CRC of sector data. This detects if good or ECC corrected data differs between
// reads
uint64_t sector_crc[MAX_CYL][MAX_HEAD][MAX_SECTORS];

// Hold for sector status and cylinder and head it was for. We save the
// data so when the cylinder and head changes we can print the final status.
// The same track may be reread.
SECTOR_STATUS last_sector_list[MAX_SECTORS];
int last_cyl;
int last_head;

int cyl_found[4096];

// Last LBA address processed for detecting bad sectors
int last_lba_addr;

// Best_track is the best track read as one read. Best_fixed_track is
// the best track by putting together multiple reads
uint32_t best_track_words[MAX_TRACK_WORDS];
int best_track_weight;
int best_track_num_words;
uint32_t best_fixed_track_words[MAX_TRACK_WORDS];
int best_fixed_track_weight;
int best_fixed_track_num_words;
uint32_t current_track_words[MAX_TRACK_WORDS];
int current_track_words_ndx;

// Type II PLL. Here so it will inline. Converted from continuous time
// by bilinear transformation. Coefficients adjusted to work best with
// my data. Could use some more work.
inline float dc_filter(float v, float *delay)
{
   float in, out;

   in = v + *delay;
   out = in * 0.034446428576716f + *delay * -0.034124999994713f;
   *delay = in;
   return out;
}


// Compare two integers for qsort
int dc_cmpint(const void *i1, const void *i2) {
   if (*(int *) i1 < *(int *)i2) {
       return -1;
   } else if (*(int *) i1 == *(int *) i2) {
      return 0;
   } else {
      return 1;
   }
}

// Print any errors and ECC correction from the sector_status_list.
//
// drive_params: Parameters for drive. Stats updated.
// sector_status_list: List of sector statuses
// cyl, head: Track status if for
void dc_print_sector_list_status(DRIVE_PARAMS *drive_params,
      SECTOR_STATUS *sector_status_list, int cyl, int head) {
   int cntr;
   int ecc_corrections = 0;
   int hard_errors = 0;
   int data_errors = 0;
   int lba_addrs[MAX_SECTORS];
   int first_sector_number = drive_params->first_sector_number;
   int num_sectors = drive_params->num_sectors;

   // Find if anything needs printing
   for (cntr = 0; cntr < num_sectors; cntr++) {
      if (!(sector_status_list[cntr].status & SECT_SPARE_BAD)) {
         if (UNRECOVERED_ERROR(sector_status_list[cntr].status)) {
            hard_errors = 1;
         }
         if (sector_status_list[cntr].status & SECT_BAD_DATA) {
            data_errors = 1;
         }
         if (sector_status_list[cntr].status & SECT_ECC_RECOVERED) {
            ecc_corrections = 1;
         }
      }
   }
   if (controller_info[drive_params->controller].analyze_type == CINFO_LBA) {
      int lba_missing = 0;

      if (data_errors) {
         msg(MSG_ERR_SUMMARY, "Bad sectors on cylinder %d head %d LBA:",cyl,head);
      }
      // Make a list of all LBA addresses found. If SECT_BAD_HEADER set then
      // LBA address is not valid. We then sort the list and check that the
      // numbers are consecutive.
      for (cntr = 0; cntr < num_sectors; cntr++) {
         if (sector_status_list[cntr].status & (SECT_BAD_HEADER |
              SECT_BAD_LBA_NUMBER)) {
            lba_missing++;
         } else {
            lba_addrs[cntr - lba_missing] = sector_status_list[cntr].lba_addr;
            if (sector_status_list[cntr].status & SECT_BAD_DATA &&
               !(sector_status_list[cntr].status & SECT_SPARE_BAD)) {
               msg(MSG_ERR_SUMMARY, " %d", sector_status_list[cntr].lba_addr);
            }
         }
      }
      if (data_errors) {
         msg(MSG_ERR_SUMMARY,"\n");
      }
      if (ecc_corrections) {
         msg(MSG_ERR_SUMMARY, "ECC Corrections on cylinder %d head %d LBA:",cyl,head);
         for (cntr = 0; cntr < num_sectors; cntr++) {
            if (sector_status_list[cntr].status & SECT_ECC_RECOVERED &&
                 !(sector_status_list[cntr].status & SECT_SPARE_BAD)) {
               msg(MSG_ERR_SUMMARY, " %d(", sector_status_list[cntr].lba_addr);
               if (sector_status_list[cntr].ecc_span_corrected_header) {
                  msg(MSG_ERR_SUMMARY, "%dH%s",
                     sector_status_list[cntr].ecc_span_corrected_header,
                     sector_status_list[cntr].ecc_span_corrected_data != 0 ? "," : "");
               }
               if (sector_status_list[cntr].ecc_span_corrected_data) {
                  msg(MSG_ERR_SUMMARY, "%d", sector_status_list[cntr].ecc_span_corrected_data);
               }
               msg(MSG_ERR_SUMMARY,")");
            }
         }
         msg(MSG_ERR_SUMMARY, "\n");
      }

      qsort(lba_addrs, num_sectors - lba_missing, sizeof(lba_addrs[0]), dc_cmpint);
      for (cntr = 0; cntr < num_sectors - lba_missing; cntr++) {
         if (last_lba_addr + 1 != lba_addrs[cntr]) {
            msg(MSG_ERR, "Missing LBA address %d", last_lba_addr + 1);
            if (last_lba_addr + 2 != lba_addrs[cntr]) {
               msg(MSG_ERR, " to %d", lba_addrs[cntr] - 1);
            }
            msg(MSG_ERR, "\n");
         }
         last_lba_addr = lba_addrs[cntr];
      }
   } else {
      // Print CHS errors
      if (hard_errors) {
         int last_test_cyl = cyl;
         msg(MSG_ERR_SUMMARY, "Bad sectors on cylinder %d", cyl);
         // Sector status list is indexed by header sector number relative to lowest
         // sector number
         for (cntr = 0; cntr < num_sectors; cntr++) {
            if (!(sector_status_list[cntr].status & SECT_BAD_HEADER)) {
               if (sector_status_list[cntr].cyl != cyl &&
                   sector_status_list[cntr].cyl != last_test_cyl) {
                  msg(MSG_ERR_SUMMARY, "/%d",sector_status_list[cntr].cyl);
                  last_test_cyl = sector_status_list[cntr].cyl;
               }
            }
         }
         msg(MSG_ERR_SUMMARY, " head %d:",head);
         for (cntr = 0; cntr < num_sectors; cntr++) {
            if (!(sector_status_list[cntr].status & SECT_SPARE_BAD)) {
               if (sector_status_list[cntr].status & SECT_BAD_HEADER) {
                  msg(MSG_ERR_SUMMARY, " %dH", cntr + first_sector_number);
               }
               if (sector_status_list[cntr].status & SECT_BAD_DATA) {
                  msg(MSG_ERR_SUMMARY, " %d", cntr + first_sector_number);
               }
            }
         }
         msg(MSG_ERR_SUMMARY,"\n");
      }
      if (ecc_corrections) {
         msg(MSG_ERR_SUMMARY, "ECC Corrections on cylinder %d", cyl);
         for (cntr = 0; cntr < num_sectors; cntr++) {
            if (!(sector_status_list[cntr].status & SECT_BAD_HEADER)) {
               if (sector_status_list[cntr].cyl != cyl &&
                   sector_status_list[cntr].cyl != last_cyl) {
                  msg(MSG_ERR_SUMMARY, "/%d",sector_status_list[cntr].cyl);
                  last_cyl = sector_status_list[cntr].cyl;
               }
            }
         }
         msg(MSG_ERR_SUMMARY, " head %d:",head);
         for (cntr = 0; cntr < num_sectors; cntr++) {
            if (sector_status_list[cntr].status & SECT_ECC_RECOVERED &&
                 !(sector_status_list[cntr].status & SECT_SPARE_BAD)) {
               msg(MSG_ERR_SUMMARY, " %d(", cntr + first_sector_number);
               if (sector_status_list[cntr].ecc_span_corrected_header) {
                  msg(MSG_ERR_SUMMARY, "%dH%s",
                     sector_status_list[cntr].ecc_span_corrected_header,
                     sector_status_list[cntr].ecc_span_corrected_data != 0 ? "," : "");
               }
               if (sector_status_list[cntr].ecc_span_corrected_data) {
                  msg(MSG_ERR_SUMMARY, "%d", sector_status_list[cntr].ecc_span_corrected_data);
               }
               msg(MSG_ERR_SUMMARY,")");
            }
         }
         msg(MSG_ERR_SUMMARY, "\n");
      }
   }

}

// Update statistics for the read so we can print summary at the end.
// Since we may be called multiple times for the same track if errors are
// retried only update the statistics and print errors when the track
// changes.
//
// drive_params: Parameters for drive. Stats updated.
// cyl, head: cylinder and head from header
// sector_status_list: status of sectors
void dc_update_stats(DRIVE_PARAMS *drive_params, int cyl, int head,
      SECTOR_STATUS sector_status_list[])
{
   STATS *stats = &drive_params->stats;
   int i;
   int write_cyl = last_cyl;

   msg(MSG_DEBUG, "update_stats: Start for C/H: %d/%d.\n", cyl, head);
   // If track changed and list has been set (last_cyl != -1) then process
   if (last_cyl != -1 && (cyl != last_cyl || head != last_head)) {
// This was start of trying to enable creating emu file with --ignore_seek_errors
// Solved issue by using microstepper so this never finished
// Also needed change below
//                if (sector_status_list[i].status & SECT_ECC_RECOVERED) {
//                   best_weight += 10;
// and to allow ignore seek with emu generation
#if 0
      if (drive_params->ignore_seek_errors) {
         int cyl_list[drive_params->num_sectors];
         int cmpint (const void * a, const void * b) {
             return ( *(int*)a - *(int*)b );
         }
         int ndx = 0;
         for (i = 0; i < drive_params->num_sectors; i++) {
            if (!(last_sector_list[i].status & SECT_BAD_HEADER)) {
               cyl_list[ndx++] = last_sector_list[i].cyl;
            }
         }
         qsort(cyl_list, ndx, sizeof(cyl_list[0]), cmpint);
         int count = 1;
         int last_entry = cyl_list[0];
         for (i = 1; i < ndx; i++) {
            if (cyl_list[i] == last_entry) {
               count++;
               // If over half same cylinder use it.
            }
            if (cyl_list[i] != last_entry || i == ndx - 1) {
               if (count > drive_params->num_sectors / 2 + 1 && last_entry != last_cyl) {
                  printf("Writing read cyl %d to actual cyl %d head %d, count %d\n",
                     last_cyl, last_entry, last_head, count);
                  write_cyl = last_entry;
               }
               count = 1;
               last_entry = cyl_list[i];
            }
         }
      }
#endif
      dc_update_emu_track_words(drive_params, sector_status_list, 1, 1,
          write_cyl, last_head);
      for (i = 0; i < drive_params->num_sectors; i++) {
         if (last_sector_list[i].status & SECT_ECC_RECOVERED &&
             !(last_sector_list[i].status & SECT_SPARE_BAD)) {
            stats->num_ecc_recovered++;
            msg(MSG_DEBUG, "update_stats[i]: stats->num_ecc_recovered++\n", i);
         }
         if (last_sector_list[i].status & SECT_SPARE_BAD) {
            stats->num_spare_bad++;
            msg(MSG_DEBUG, "update_stats[i]: stats->num_spare_bad++\n", i);
         } else if (last_sector_list[i].status & SECT_BAD_HEADER) {
            stats->num_bad_header++;
            msg(MSG_DEBUG, "update_stats[i]: stats->num_bad_header++\n", i);
         } else if (last_sector_list[i].status & SECT_BAD_DATA) {
            stats->num_bad_data++;
            msg(MSG_DEBUG, "update_stats[i]: stats->num_bad_data++\n", i);
         } else {
            stats->num_good_sectors++;
            msg(MSG_DEBUG, "update_stats[i]: stats->num_good_sectors++\n", i);
         }
      }
      dc_print_sector_list_status(drive_params, last_sector_list,
         last_cyl, last_head);
   } else {
      dc_update_emu_track_words(drive_params, sector_status_list, 0, last_cyl == -1, cyl, head);
   }
   // Save the sector information so we can use it when track changes
   if (sector_status_list != NULL) {
      memcpy(last_sector_list, sector_status_list, sizeof(last_sector_list));
   }
   last_cyl = cyl;
   last_head = head;
   msg(MSG_DEBUG, "update_stats: End.\n");
}

// Note that last call where data will be written had data for next track.
void dc_update_emu_track_words(DRIVE_PARAMS * drive_params,
      SECTOR_STATUS sector_status_list[], int write_track, int new_track,
      int cyl, int head)
{
   int i;
   int last_weight = 0;
   int best_weight = 0;

   if (drive_params->emulation_filename == NULL || !drive_params->emulation_output) {
      return;
   }
   // Determine error value for best track and best_fixed track so we can
   // determine which to use.
   if (sector_status_list != NULL) {
      for (i = 0; i < drive_params->num_sectors; i++) {
         if (!(sector_status_list[i].last_status & SECT_WRONG_CYL)) {
            // Last status is the one for the last track read
            if (sector_status_list[i].last_status & SECT_BAD_DATA) {
               last_weight += 1;
            } else if (!(sector_status_list[i].last_status & SECT_BAD_HEADER)) {
               if (sector_status_list[i].last_status & SECT_ECC_RECOVERED) {
                  last_weight += 9;
               } else {
                  last_weight += 10;
               }
            }
         }
         if (!(sector_status_list[i].status & SECT_WRONG_CYL)) {
            // This is the best status for all the reads of the track
            if (sector_status_list[i].status & SECT_BAD_DATA) {
               best_weight += 1;
            } else if (!(sector_status_list[i].status & SECT_BAD_HEADER)) {
               if (sector_status_list[i].status & SECT_ECC_RECOVERED) {
                  best_weight += 9;
               } else {
                  best_weight += 10;
               }
            }
         }
      }
   }
   if (write_track) {
      // If it is at least as good use the track that was from one read
      // since it is more likely to be ok
      if (best_track_weight >= best_fixed_track_weight) {
         emu_file_write_track_bits(drive_params->emu_fd, best_track_words,
               best_track_num_words, cyl, head,
               drive_params->emu_track_data_bytes);
      } else {
        //printf("Using fixed %d,%d,%d %d %d\n",best_weight, last_weight,
        //     best_track_weight, cyl, head);
         emu_file_write_track_bits(drive_params->emu_fd, best_fixed_track_words,
               best_fixed_track_num_words, cyl, head,
               drive_params->emu_track_data_bytes);
      }
   }
   // Keep best track. Should be last track the way mfm_read works.
   if (last_weight > best_track_weight || new_track) {
      best_track_weight = last_weight;
      memcpy(best_track_words, current_track_words, current_track_words_ndx *
            sizeof(best_track_words[0]));
      best_track_num_words = current_track_words_ndx;
   }
   // Take the first track for our best fixed track
   if (new_track && sector_status_list != NULL) {
      memcpy(best_fixed_track_words, current_track_words, current_track_words_ndx *
            sizeof(best_track_words[0]));
      best_fixed_track_num_words = current_track_words_ndx;
   }
   best_fixed_track_weight = best_weight;
   // Clear for next time
   current_track_words_ndx = 0;
}

void dc_print_status_flags(SECTOR_DECODE_STATUS status)
{
   if ((status & SECT_ANALYZE_SPARE) == SECT_ANALYZE_SPARE)
      printf("SECT_ANALYZE_SPARE ");
   if ((status & ANALYZE_WRONG_FORMAT) == ANALYZE_WRONG_FORMAT)
      printf("ANALYZE_WRONG_FORMAT ");
   if ((status & SECT_AMBIGUOUS_CRC) == SECT_AMBIGUOUS_CRC)
      printf("SECT_AMBIGUOUS_CRC ");
   if ((status & SECT_ANALYZE_ERROR) == SECT_ANALYZE_ERROR)
      printf("SECT_ANALYZE_ERROR ");
   if ((status & SECT_BAD_LBA_NUMBER) == SECT_BAD_LBA_NUMBER)
      printf("SECT_BAD_LBA_NUMBER ");
   if ((status & SECT_BAD_SECTOR_NUMBER) == SECT_BAD_SECTOR_NUMBER)
      printf("SECT_BAD_SECTOR_NUMBER ");
   if ((status & SECT_SPARE_BAD) == SECT_SPARE_BAD)
      printf("SECT_SPARE_BAD ");
   if ((status & SECT_ZERO_HEADER_CRC) == SECT_ZERO_HEADER_CRC)
      printf("SECT_ZERO_HEADER_CRC ");
   if ((status & SECT_ZERO_DATA_CRC) == SECT_ZERO_DATA_CRC)
      printf("SECT_ZERO_DATA_CRC ");
   if ((status & SECT_HEADER_FOUND) == SECT_HEADER_FOUND)
      printf("SECT_HEADER_FOUND ");
   if ((status & SECT_ECC_RECOVERED) == SECT_ECC_RECOVERED)
      printf("SECT_ECC_RECOVERED ");
   if ((status & SECT_WRONG_CYL) == SECT_WRONG_CYL)
      printf("SECT_WRONG_CYL ");
   if ((status & SECT_NOT_WRITTEN) == SECT_NOT_WRITTEN)
      printf("SECT_NOT_WRITTEN ");
   if ((status & SECT_BAD_HEADER) == SECT_BAD_HEADER)
      printf("SECT_BAD_HEADER ");
   if ((status & SECT_BAD_DATA) == SECT_BAD_DATA)
      printf("SECT_BAD_DATA ");

   printf("\n");
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
SECTOR_DECODE_STATUS dc_crc_bytes(DRIVE_PARAMS *drive_params,
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
void dc_check_header_values(int exp_cyl, int exp_head,
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
int dc_write_sector(uint8_t bytes[], DRIVE_PARAMS * drive_params,
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
   // Keep this in place for MFM, however, as that side is still needed.

   if (!drive_params->is_rll)
   {
      mfm_update_emu_track_sector(drive_params, sector_status, sect_rel0,
         all_bytes, all_bytes_len, update);   
   }

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
