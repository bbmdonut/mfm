#ifndef MFM_DECODER_H_
#define MFM_DECODER_H_
//
// 05/15/26 DJG Added SHUGART_CD9963 & HP9133XV controller
// 01/12/26 BBMD Refactored common bits for MFM/RLL out into separate source
//    files, updated remaining items to support refactoring as needed
// 09/10/25 DJG Fixed ext2emu marking bad sectors when interleave used
// 06/12/25 DJG/DV Add CONTROLLER_MICROBEE_WD1002_05
// 06/04/25 DJG Changed trk_Xebec_* to use 5 ID mark patterns to match image
//    mindset_st225_base.emu. 
//    https://bitsavers.org/pdf/xebec/Xebec_S1410/104478B_S1410A_Feb84.pdf
//    says ID pattern is 4 bytes not normal 1. Also adjusted timing midway
//    between two sample disk images.
// 03/13/25 DJG Added ext2emu support for Xebec_104527_512B
// 01/20/25 SH  Add ext2emu support for corvus_omni
// 01/13/25 DJG Fixes for xebec_skew processing. Skew not same on all tracks.
// 10/30/24 DJG Add new option to handle Xebec data skewed one sector from
//    header
// 10/20/24 DJG Added support for AT&T 3B2 17 sector per track format
// 07/02/24 DJG Fixed ECC length for CONTROLLER_IMS_A820 and added ext2emu support
// 06/26/24 DJG Added CONTROLLER_IMS_A820
// 06/12/24 DJG Added CONTROLLER_OMTI_5200_18SECTOR_512B
// 05/24/24 DJG Added Seagate ST11MB support. Make bitfields unique.
// 04/29/24 DJG Disabled TI_2223220 format. Duplicate of EC1841
// 05/19/24 DJG Added CONTROLLER_XEBEC_104527_C0_256B. Compare byte 0.
// 04/30/24 DJG Added CONTROLLER_INFORT_PC02_06 format
// 04/29/24 DJG Added TI_2223220 format
// 04/27/24 SM/DJG Added ext2emu support for AT&T 3B2
// 02/20/24 DJG Added CONTROLLER_ADAPTEC_4000_18Sector_512B (ACB-4000)
// 11/20/23 DJG Added CONTROLLER_NEC_4800
// 11/10/23 DJG Fixed missing first sector for CONTROLLER_OMTI_20L
// 11/04/23 DJG Added CONTROLLER_SOUYZ_NEON
// 10/30/23 DJG Added CONTROLLER_OMTI_20L
// 10/18/23 SWE Added David Junior II 210 and 301
// 10/13/23 DJG Added CONTROLLER_ND100_3041
// 09/01/23 DJG Added WD_MICROENGINE support
// 08/31/23 DJG Added DIMENSION_68000 support
// 07/08/23 DJG Added Fujitsu-K-10R and changed all poly 0x00a00805 to shorter
//    ECC correction length. See 01/17/23
// 04/26/23 DJG Really fixed EC1841 ext2emu
// 04/17/23 DJG Fixed EC1841 and Tektronix_6130 ext2emu
// 03/27/23 DJG Addex ext2emu support for EC1841
// 03/11/23 DJG Fix for EC1841 decoding
// 03/10/23 DJG Added ES7978 format.
// 01/17/23 DJG Found false ECC correction so reduced ECC correction length for 0x00a00805
//    added ext2emu support for Xebec_104527_256B
// 10/31/22 DJG Added ext2emu Corvus_H support
// 10/01/22 DJG Added CTM9016 format
// 06/01/22 TJT Add CALLAN with proper CRC info
// 03/17/22 DJG Update function prototype
// 12/18/21 DJG Fix Symbolics 3640 ext2emu creation
// 12/18/21 SWE Added David Junior II
// 10/29/21 DJG Added STRIDE_440
// 09/20/21 DJG Added TANDY_16B to give ext2emu support
// 09/03/21 DJG Added SUPERBRAIN
// 08/27/21 DJG Added DSD_5217_512B
// 05/27/21 DJG Added TEKTRONIX_6130
// 03/21/21 DJG Added initial value for OMTI 20D controller 256 byte sectors
// 02/15/21 DJG Added ext2emu support for CONVERGENT_AWS
// 02/01/21 DJG Adjusted trk_ELEKTROKIKA_85 to match documentation on format
//    found.
// 01/18/21 DJG Add ext2emu support for Elektronika_85
// 01/07/21 DJG Added RQDX2 format
// 12/11/20 DJG Found false ECC correction so reduced ECC correction length
// 11/13/20 DJG Added CONTROLLER_ACORN_A310_PODULE
// 10/26/20 DJG ext2emu support for MYARC_HFDC controller
// 10/24/20 DJG Added MYARC_HFDC controller and ext2emu support for SM_1810_512B
// 10/17/20 DJG increased maximum ECC correction length to 2 for 24 bit
//    polynomial and 7 or 8 for 32 bit polynomial.
// 10/16/20 DJG Added SHUGART_SA1400 controller.
// 10/08/20 DJG Added SHUGART_1610 and UNKNOWN2 controllers
// 09/21/21 DJG Added controller SM_1810_512B
// 12/31/19 DJG Added PERQ T2 ext2emu support. Not tested.
// 11/29/19 DJG Fix PERQ T2 format to ignore sector data trailing byte
// 10/25/19 DJG Added PERQ T2 format
// 10/05/19 DJG Fixes to detect when CONT_MODEL controller doesn't really
//    match format
// 07/19/19 DJG Added ext2emu support for Xerox 8010. Fixed data for
//    trk_omti_5510
// 06/19/19 DJG Removed DTC_256B since only difference from DTC_520_256B was
//    error. Added SM1040 format. Fixed Xerox_8010 bitrate. Added recovery
//    mode flag
// 02/09/19 DJG Added CONTROLLER_SAGA_FOX, adjusted trk_ISBC215_1024b to match
//    example file
// 01/20/18 DJG Increased maximum sector to support iSBC 214/215 128B 54 sectors/track
//    Added support for ext2emu for iSBC 214/215. Controller names changed.
// 12/16/18 DJG Added NIXDORF_8870
// 11/03/18 DJG Renamed variable
// 10/12/18 DJG Added CONTROLLER_IBM_3174
// 09/10/18 DJG Added CONTROLLER_DILOG_DQ604
// 08/05/18 DJG Added IBM_5288. Fixed Convergent AWS SA1000 format
// 07/02/18 DJG Added Convergent AWS SA1000 format and new data for finding
//   correct location to look for headers
// 06/03/18 DJG Added Tandy 8 Meg SA1004, fourth DTC variant, and ROHM_PBX
// 04/25/18 DJG Added Xerox 8010 and Altos support
// 03/31/18 DJG Added ext2emu support for DTC.
// 03/09/18 DJG Added CONTROLLER_DILOG_DQ614 and fields for reading more
//    cylinders and heads than analyze determines.
// 12/17/17 DJG Aded EDAX_PV9900
// 11/23/17 DJG Changed Wang 2275_B to CONT_MODEL so it won't get confused
//    with WD_1006
// 09/30/17 DJG Added support for Wang 2275
// 08/11/17 DJG Added support for Convergent AWS
// 05/19/17 DJG New sector state to indicate it hasn't been written.
// 04/21/17 DJG Allow --begin_time to override default values from analyze
// 03/08/17 DJG Fixed Intel iSBC 215 and added support for all sector lengths
// 02/12/17 DJG Added support for Data General MV/2000. Fix mfm_util
//    for Mightframe
// 02/09/17 DJG Added support for AT&T 3B2
// 02/07/17 DJG Added support for Altos 586 and adjusted start time for
//    Cromemco to prevent trying to read past end of track.
// 01/18/17 DJG Added 532 sector length for Sun Remarketing OMTI controller
//    for Lisa computer and --ignore_seek_errors option
// 01/06/17 DJG Don't consider SECT_SPARE_BAD unrecoverable error
// 12/11/16 DJG Added Intel iSBC_215 controller. Fix for Adaptec format
//    bad block handling. Handle sector contents which make CRC detection
//    ambiguous.
// 11/20/16 DJG Add logic to allow emulator track length to be increased if
//    data won't fit. Found with Vector4. Most likly drive rotation speed
//    and pad bits distributed between sectors instead of grouped at end.
//    Change to allow copying extra data before the begining of the
//    sector needed for syncronization to make fixing of emu data work better.
//    Changes to pass length of data for correcting emu file data
//    instead of trying to recalculate.
// 11/14/16 DJG Added Telenex Autoscope, Xebec S1420 and Vector4 formats
// 11/02/16 DJG Added metadata length field. Only Xerox 6085 uses
// 10/31/16 DJG Added extra header needed by Cromemco to make ext2emu
//    files work. Changes to handle Adaptec and sectors marked bad better
// 10/22/16 DJG Added unknown format found on ST-212 disk
// 10/16/16 DJG Renamed OLIVETTI to DTC. Added MOTOROLA_VME10 and SOLOSYSTEMS
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
#include "track_definitions.h"
#include "TC78H670FTG.h"

// Smallest sector size should be first in list
DEF_EXTERN int mfm_all_sector_size[]
#ifdef DEF_DATA
 = {128, 256, 512, 524, 532, 1024, 1160, 1164, 2048, 4096, 10240, -1}
  // -1 marks end of array
#endif
;

// Number of sectors to search for LBA format
DEF_EXTERN int mfm_lba_num_sectors[]
#ifdef DEF_DATA
 = {17, 18, 32, 33, -1}
  // -1 marks end of array
#endif
;

SECTOR_DECODE_STATUS mfm_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS wd_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS tagged_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS xebec_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS corvus_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS northstar_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);
SECTOR_DECODE_STATUS perq_decode_track(DRIVE_PARAMS *drive_parms, int cyl,
   int head, uint16_t deltas[], int *seek_difference,
   SECTOR_STATUS bad_sector_list[]);

void mfm_check_header_values(int exp_cyl, int exp_head, int *sector_index,
   int sector_size, int *seek_difference, SECTOR_STATUS *sector_status,
   DRIVE_PARAMS *drive_params, SECTOR_STATUS sector_status_list[]);
void mfm_decode_setup(DRIVE_PARAMS *drive_params, int write);
void mfm_decode_done(DRIVE_PARAMS *drive_params);
int mfm_write_sector(uint8_t bytes[], DRIVE_PARAMS *drive_params,
   SECTOR_STATUS *sector_status, SECTOR_STATUS bad_sector_list[],
   uint8_t all_bytes[], int all_bytes_len);
int mfm_write_metadata(uint8_t bytes[], DRIVE_PARAMS *drive_params,
   SECTOR_STATUS *sector_status);
void init_sector_status_list(SECTOR_STATUS *sector_status_list, int num_sectors);
void mfm_dump_bytes(uint8_t bytes[], int len, int cyl, int head,
      int sector_index, int msg_level);

SECTOR_DECODE_STATUS mfm_crc_bytes(DRIVE_PARAMS *drive_params, uint8_t bytes[],
    int bytes_crc_len, int state, uint64_t *crc_ret, int *ecc_span,
    SECTOR_DECODE_STATUS *init_status, int perform_ecc);

SECTOR_DECODE_STATUS mfm_process_bytes(DRIVE_PARAMS *drive_params, uint8_t bytes[],
      int bytes_crc_len, int total_bytes, STATE_TYPE *state, int cyl,
      int head, int *sector_index,
      int *seek_difference, SECTOR_STATUS sector_status_list[],
      SECTOR_DECODE_STATUS init_status);

SECTOR_DECODE_STATUS wd_process_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);
SECTOR_DECODE_STATUS tagged_process_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);
SECTOR_DECODE_STATUS xebec_process_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);
SECTOR_DECODE_STATUS corvus_process_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);
SECTOR_DECODE_STATUS northstar_process_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);
SECTOR_DECODE_STATUS perq_process_data(STATE_TYPE *state, uint8_t bytes[],
      int total_bytes,
      uint64_t crc, int exp_cyl, int exp_head, int *sector_index,
      DRIVE_PARAMS *drive_params, int *seek_difference,
      SECTOR_STATUS sector_status_list[], int ecc_span,
      SECTOR_DECODE_STATUS init_status);

int mfm_save_raw_word(DRIVE_PARAMS *drive_params, int all_raw_bits_count,
   int int_bit_pos, int raw_word);
// Use data stored by mfm_mark_location instead of data passed in to
//   mfm_mark_header_location or mfm_mark_data_location
#define MARK_STORED -999
void mfm_mark_header_location(int bit_count, int bit_offset, int tot_bit_count);
void mfm_mark_data_location(int bit_count, int bit_offset, int tot_bit_count);
void mfm_mark_location(int bit_count, int bit_offset, int tot_bit_count);
void mfm_mark_end_data(int bit_count, DRIVE_PARAMS *drive_params, int cyl, int head);
int mfm_get_data_bit_count();

void mfm_handle_alt_track_ch(DRIVE_PARAMS *drive_params, unsigned int bad_cyl,
      unsigned int bad_head, unsigned int good_cyl, unsigned int good_head);
void mfm_handle_alt_LBA(DRIVE_PARAMS *drive_params, unsigned int bad_LBA,
     int good_LBA, int size, int print);
int mfm_fix_head(DRIVE_PARAMS *drive_params, int exp_head, int head);

void mfm_end_track(DRIVE_PARAMS *drive_params,
   unsigned int cyl, unsigned int head);
void mfm_clear_remap_list(void);
void mfm_remap_track_sectors(unsigned int from_sector, unsigned int to_sector);
void mfm_remap_track(DRIVE_PARAMS *drive_params,
   unsigned int cyl, unsigned int head);

#endif /* MFM_DECODER_H_ */
