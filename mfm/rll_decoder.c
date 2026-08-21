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

   for (i = 0; i < drive_params->num_sectors; i++) {
      sector_status_list[i].last_status = SECT_BAD_HEADER;
   }

   // Change in rll_process_bytes if this "if" block is changed
   if (drive_params->controller == CONTROLLER_WD_1006_RLL) {
      rc = wd_decode_rll_track(drive_params, cyl, head, deltas, seek_difference,
            sector_status_list);
   }
   dc_update_stats(drive_params, cyl, head, sector_status_list);
   return rc;
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

   status = dc_crc_bytes(drive_params, bytes, bytes_crc_len, *state, &crc,
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
