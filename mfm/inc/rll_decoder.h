#ifndef RLL_DECODER_H_
#define RLL_DECODER_H_

//
// rll_decoder.h
//
//  Created on: Mar 31, 2025
//      Author: BBMD (Based on code from DJG, others)
//
// 01/12/26 BBMD Initial implementation of common RLL decoding functionality,
//    based in part on DJG and others' original MFM code
//
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
//

#include "decoder_common.h"
#include "TC78H670FTG.h"

#ifndef DEF_DATA
#define DEF_EXTERN extern
#else
#define DEF_EXTERN
#endif

SECTOR_DECODE_STATUS wd_process_rll_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);
SECTOR_DECODE_STATUS rll_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS wd_decode_rll_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
extern void update_emu_track_words(DRIVE_PARAMS * drive_params,
      SECTOR_STATUS sector_status_list[], int write_track, int new_track,
      int cyl, int head);
SECTOR_DECODE_STATUS rll_process_bytes(DRIVE_PARAMS *drive_params, uint8_t bytes[],
      int bytes_crc_len, int total_bytes, STATE_TYPE *state, int cyl,
      int head, int *sector_index,
      int *seek_difference, SECTOR_STATUS sector_status_list[],
      SECTOR_DECODE_STATUS init_status);
int rll_fix_head(DRIVE_PARAMS *drive_params, int exp_head, int head);

#endif // RLL_DECODER_H__
