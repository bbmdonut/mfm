#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "crc_ecc.h"
#include "emu_tran_file.h"
#include "decoder_common.h"
#include "track_definitions.h"
#include "msg.h"

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
float filter(float v, float *delay)
{
   float in, out;

   in = v + *delay;
   out = in * 0.034446428576716f + *delay * -0.034124999994713f;
   *delay = in;
   return out;
}


// Compare two integers for qsort
int cmpint(const void *i1, const void *i2) {
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
void print_sector_list_status(DRIVE_PARAMS *drive_params,
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

      qsort(lba_addrs, num_sectors - lba_missing, sizeof(lba_addrs[0]), cmpint);
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
void update_stats(DRIVE_PARAMS *drive_params, int cyl, int head,
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
      update_emu_track_words(drive_params, sector_status_list, 1, 1,
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
      print_sector_list_status(drive_params, last_sector_list,
         last_cyl, last_head);
   } else {
      update_emu_track_words(drive_params, sector_status_list, 0, last_cyl == -1, cyl, head);
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
void update_emu_track_words(DRIVE_PARAMS * drive_params,
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

void print_status_flags(SECTOR_DECODE_STATUS status)
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
