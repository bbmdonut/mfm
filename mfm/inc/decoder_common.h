#ifndef DECODER_COMMON_H_
#define DECODER_COMMON_H_

//
// decoder_common.h
//
//  Created on: Mar 31, 2025
//      Author: BBMD (contains original code from DJG)
//
// 05/15/26 DJG Added SHUGART_CD9963 & HP9133XV controller
// 01/12/26 BBMD Added in order to export common functions for both MFM
//    and RLL operations, to minimize duplication of code
// 09/10/25 DJG Fixed ext2emu marking bad sectors when interleave used
// 06/12/25 DJG/DV Add CONTROLLER_MICROBEE_WD1002_05
// 06/04/25 DJG Changed trk_Xebec_* to use 5 ID mark patterns to match image
//    mindset_st225_base.emu.
//    https://bitsavers.org/pdf/xebec/Xebec_S1410/104478B_S1410A_Feb84.pdf
//    says ID pattern is 4 bytes not normal 1. Also adjusted timing midway
//    between two sample disk images.
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

#include <sys/types.h>
#include <stdint.h>

#ifndef DEF_DATA
#define DEF_EXTERN extern
#else
#define DEF_EXTERN
#endif

#include "track_definitions.h"
#include "Arduino.h"
#include "TC78H670FTG.h"

#define SECTOR_GOOD_BAD 0x80
#define SECTOR_GOOD_GOOD 0x0
#define SECTOR_GOOD_ECC_MASK 0x7f

// Various convenience macros
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#define MAX(x,y) (x > y ? x : y)
#define MIN(x,y) (x < y ? x : y)
#define BIT_MASK(x) (1 << (x))

// Number of nanoseconds of each PRU clock
#define CLOCKS_TO_NS 5

// MFM is up to 32 for 256 byte sectors. This allows growth for RLL/ESDI
// ISBC_214 can have 54 128 byte sectors
#define MAX_SECTORS 70
#define MAX_HEAD 16
#define MAX_CYL 4096
#define MAX_SECTOR_SIZE 20480 // Data size in bytes
// Max size of raw words for a track. This is big enough to
// hold future growth up to 30 Mbit/sec at 3600 RPM
#define MAX_TRACK_WORDS 16000

// The possible states from reading each sector.

// These are ORed into the state
// Flag for analyze format we must see spare sector.
#define SECT_ANALYZE_SPARE 0x4000
// This is set if information is found such as sector our of the expected
// range. If MODEL controller mostly matches but say has one less sector than
// the disk has it was being selected instead of going on to ANALYZE
#define ANALYZE_WRONG_FORMAT 0x2000
// If this is set the data being CRC'd is zero so the zero CRC
// result is ambiguous since any polynomial will match
#define SECT_AMBIGUOUS_CRC 0x1000
// If set treat as error for analyze but otherwise ignore it
#define SECT_ANALYZE_ERROR 0x800
// Set if the sector number is bad when BAD_HEADER is not set.
// Some formats use bad sector numbers to flag bad blocks
#define SECT_BAD_LBA_NUMBER 0x400
#define SECT_BAD_SECTOR_NUMBER 0x200
// This is used to mark sectors that are spare sectors or are marked
// bad and don't contain user data.
// It suppresses counting as errors other errors seen.
#define SECT_SPARE_BAD      0x100
#define SECT_ZERO_HEADER_CRC 0x80
#define SECT_ZERO_DATA_CRC  0x40
#define SECT_HEADER_FOUND   0x20
#define SECT_ECC_RECOVERED  0x10
#define SECT_WRONG_CYL      0x08
// Sector hasn't been written yet
#define SECT_NOT_WRITTEN    0x04
// Only one of these three will be set. BAD_HEADER is initially set
// until we find a good header, then BAD_DATA is set until we find good data
#define SECT_BAD_HEADER     0x02
#define SECT_BAD_DATA       0x01
#define SECT_NO_STATUS      0x00
#define UNRECOVERED_ERROR(x) ((x & (SECT_BAD_HEADER | SECT_BAD_DATA)) && !(x & SECT_SPARE_BAD))

#define CONTROLLER_TI_2223220 9999

typedef uint32_t SECTOR_DECODE_STATUS;

// These are various statistics from reading the drive that are used to
// print a summary when finished.
typedef struct {
   int max_sect;
   int min_sect;
   int max_head;
   int min_head;
   int max_cyl;
   int min_cyl;
   int num_good_sectors;
   int num_bad_header;
   int num_bad_data;
   int num_spare_bad;
   int num_ecc_recovered;
   int num_retries;
   int max_ecc_span;
   int max_track_words;
   int emu_data_truncated;
} STATS;

typedef char MARK_BAD_INFO[MAX_CYL][MAX_HEAD][MAX_SECTORS];

typedef struct alt_struct ALT_INFO;

struct alt_struct {
   ALT_INFO *next;
   int bad_offset;
   int good_offset;
   int length;
};

// The state of a sector
typedef struct {
   // The span of any ECC correction. 0 if no correction
   int ecc_span_corrected_data;
   int ecc_span_corrected_header;
   // The difference between the expected and actual cylinder found
   int cyl_difference;
   // The values from the sector header
   int cyl, head, sector;
   // Non zero if drive is LBA
   int is_lba;
   // The logical block address for LBA drives.
   int lba_addr;
   // A sequential count of sectors starting from 0
   // Logical sector will only be accurate if no header read errors
   // on preceding sectors
   int logical_sector;
   // The sector state
   SECTOR_DECODE_STATUS status;
   SECTOR_DECODE_STATUS last_status;
   int ignore; // Non zero ignore this sector. Its a non used spare sector
} SECTOR_STATUS;

// This is the main structure defining the drive characteristics
typedef struct {
   // The number of cylinders, heads, and sectors per track
   int num_cyl;
   int num_head;
   int num_sectors;
   // Don't do retry when >= than specified head or cylinder. This is used
   // when drive mixes formats. Code currently can only decode one so
   // retries don't help
   int noretry_cyl;
   int noretry_head;
   // The number of the first sector. Some disks start at 0 others 1
   int first_sector_number;
   // Size of data area of sector in bytes
   int sector_size;
   // Size of metadata area of sector in bytes
   int metadata_bytes;
   // CRC/ECC used for header and data area
   CRC_INFO header_crc, data_crc;
   // Track format
   // Update controller_info list below if enum changed.
   // ORDER IN THE TWO LISTS MUST MATCH
   // TODO, replace this with pointer to CONTROLLER entry
   enum {CONTROLLER_NONE,
      CONTROLLER_NEWBURYDATA,
      CONTROLLER_ALTOS,
      CONTROLLER_SUPERBRAIN,
      CONTROLLER_WD_1006,
      CONTROLLER_RQDX2,
      CONTROLLER_ISBC_214_128B,
      CONTROLLER_ISBC_214_256B,
      CONTROLLER_ISBC_214_512B,
      CONTROLLER_ISBC_214_1024B,
      CONTROLLER_MICROBEE_WD1002_05,
      CONTROLLER_TEKTRONIX_6130,
      CONTROLLER_NIXDORF_8870,
      CONTROLLER_TANDY_8MEG,
      CONTROLLER_WD_3B1,
      CONTROLLER_TANDY_16B,
      CONTROLLER_CORVUS_OMNI,
      CONTROLLER_MOTOROLA_VME10,
      CONTROLLER_DTC,
      CONTROLLER_DTC_520_256B,
      CONTROLLER_DTC_520_512B,
      CONTROLLER_MACBOTTOM,
      CONTROLLER_FUJITSU_K_10R,
      CONTROLLER_CTM9016,
      CONTROLLER_ACORN_A310_PODULE,
      CONTROLLER_ELEKTRONIKA_85,
      CONTROLLER_ALTOS_586,
      CONTROLLER_ATT_3B2,
      CONTROLLER_ATT_3B2_17sector,
      CONTROLLER_CONVERGENT_AWS,
      CONTROLLER_CONVERGENT_AWS_SA1000,
      CONTROLLER_WANG_2275,
      CONTROLLER_WANG_2275_B,
      CONTROLLER_CALLAN,
      CONTROLLER_IBM_5288,
      CONTROLLER_EDAX_PV9900,
      CONTROLLER_SHUGART_1610,
      CONTROLLER_SHUGART_SA1400,
      CONTROLLER_ES7978,
      CONTROLLER_WD_MICROENGINE,
      CONTROLLER_NEC_4800,
      CONTROLLER_SOUYZ_NEON,
      CONTROLLER_INFORT_PC02_06,
      CONTROLLER_HP9133XV,
      CONTROLLER_SHUGART_CD9963,      
      CONTROLLER_SM_1810_512B,
      CONTROLLER_DSD_5217_512B,
      CONTROLLER_OMTI_5510,
      CONTROLLER_OMTI_5200_18SECTOR_512B,
      CONTROLLER_XEROX_6085,
      CONTROLLER_TELENEX_AUTOSCOPE,
      CONTROLLER_MORROW_MD11,
      CONTROLLER_UNKNOWN1,
      CONTROLLER_UNKNOWN2,
      CONTROLLER_DEC_RQDX3,
      CONTROLLER_MYARC_HFDC,
      CONTROLLER_IBM_3174,
      CONTROLLER_SEAGATE_ST11M,
      CONTROLLER_SEAGATE_ST11MB,
      CONTROLLER_ISBC_215_128B,
      CONTROLLER_ISBC_215_256B,
      CONTROLLER_ISBC_215_512B,
      CONTROLLER_ISBC_215_1024B,
      CONTROLLER_XEROX_8010,
      CONTROLLER_ROHM_PBX,
      CONTROLLER_DIMENSION_68000,
      CONTROLLER_DJ_II_210,
      CONTROLLER_DJ_II_301,
      CONTROLLER_DJ_II,
      CONTROLLER_ADAPTEC,
      CONTROLLER_ADAPTEC_4000_18SECTOR_512B,
      CONTROLLER_MVME320,
      CONTROLLER_SYMBOLICS_3620,
      CONTROLLER_SM1040,
      CONTROLLER_SYMBOLICS_3640,
      CONTROLLER_OMTI_20L,
      CONTROLLER_MIGHTYFRAME,
      CONTROLLER_DG_MV2000,
      CONTROLLER_SOLOSYSTEMS,
      CONTROLLER_DILOG_DQ614,
      CONTROLLER_DILOG_DQ604,
      CONTROLLER_XEBEC_104786,
      CONTROLLER_XEBEC_104527_256B,
      CONTROLLER_XEBEC_104527_512B,
      CONTROLLER_XEBEC_104527_C0_256B,
      //CONTROLLER_TI_2223220,
      CONTROLLER_XEBEC_S1420,
      CONTROLLER_EC1841,
      CONTROLLER_CORVUS_H,
      CONTROLLER_NORTHSTAR_ADVANTAGE,
      CONTROLLER_CROMEMCO,
      CONTROLLER_VECTOR4,
      CONTROLLER_VECTOR4_ST506,
      CONTROLLER_STRIDE_440,
      CONTROLLER_SAGA_FOX,
      CONTROLLER_ND100_3041,
      CONTROLLER_PERQ_T2,
      CONTROLLER_IMS_A820,
      CONTROLLER_WD_1006_RLL
   } controller;

   // The sector numbering used. This will vary from the physical order if
   // interleave is used. Only handles all sectors the same.
   uint8_t *sector_numbers;
   // DRIVE_STEP_FAST or DRIVE_STEP_SLOW from drive.h
   int step_speed;
   // Stats structure above
   STATS stats;
   // Files to write extracted sector data, emulator data file, and
   // raw transitions data to. Null if file should not be written.
   char *extract_filename;
   char *emulation_filename;
   char *transitions_filename;
   // 1 if emulation file is output
   int emulation_output;
   // One WD controller truncated the head number to 3 bits in the header. This
   // enables processing for that.
   int head_3bit;
   // Number of retries to do when an error is found reading the disk
   int retries;
   // Number of retries to do without seeking
   int no_seek_retries;
   // Use drive recovery line.
   int recovery;
   // Disables some header checks. Not currently used.
   int ignore_header_mismatch;
   // Drive number for setting select lines
   int drive;
   // Command line for driver parameters. Null if not set
   char *cmdline;
   // Input/output files
   int tran_fd;
   int emu_fd;
   int ext_fd;
   int ext_metadata_fd;
   TRAN_FILE_INFO *tran_file_info;
   EMU_FILE_INFO *emu_file_info;
   // Size of data for each emulator track. Only valid when writing
   // emulator file. TODO, use emu_file_info field instead
   int emu_track_data_bytes;
   // non zero if analyze option set
   int analyze;
   // non zero if in process of performing format analyze.
   int analyze_in_progress;
   // What cylinder and head to analyze
   int analyze_cyl;
   int analyze_head;
   // What options have been set. Used by command line parsing and validation
   uint32_t opt_mask;
   // Command line note parameter
   char *note;
   // Time after index to start read in nanoseconds
   uint32_t start_time_ns;
   // Non zero if start_time_ns has been set either from input file or command
   // line and shouldn't be overridden.
   int dont_change_start_time;
   // List of sector to mark bad in ext2emu. Sorted ascending
   MARK_BAD_INFO *mark_bad_list;
   // Linked list of alternate tracks for fixing extracted data file
   ALT_INFO *alt_llist;
   // Cylinder to start write precompensation at. For mfm_write
   int write_precomp_cyl;
   // Precompensation time in nanoseconds
   int early_precomp_ns;
   int late_precomp_ns;
   // If we detect special cases of the format durring running we
   // set them here. ADAPTEC_COUNT_BAD_BLOCKS is when bits 0 to at least
   // 5
   enum {FORMAT_NONE, FORMAT_ADAPTEC_COUNT_BAD_BLOCKS} format_adjust;
   // Non zero if seek errors should be ignored
   int ignore_seek_errors;
   // Non zero if sector data follows one sector after header. Only
   // valid for certain Xebec formats
   int xebec_skew;
   // Value set on command line
   int xebec_skew_cmdline;
   // Extra data needed. Data in this structure is big endian
   union {
      struct s_CD9963_sect0 {
         uint16_t sectSize;
         uint8_t  nHeads;
         uint8_t  nSect;
         uint16_t nCyl;
         uint16_t unknown1[5];
         uint16_t SCSInCyl;
         uint16_t unknown2;
         uint8_t  nZones1;
         uint8_t  nZones2;
         uint16_t zones[245];  // Only 15 used in sample
      } CD9963_sect0;
   } u;   
   // If zero, we are dealing with MFM encoding. Otherwise, it's RLL.
   int is_rll;
   // Used by the microstepping code for external stepper control
   // during data recovery
   PRODRIVERSettings *settings;
   // Number of cylinders to move the head on startup, before any other
   // drive operations occur; used primarily in microstepping recovery mode
   int step;
   // If non-zero, we will use an external stepper controller
   // for the heads. Otherwise, the drive's onboard controller is used.
   int ext_stepper;
} DRIVE_PARAMS;


// CHECK_NONE is used for header formats where some check that is specific
// to the format is used so can't be generalized. If so the check will need
// to be done in the header decode and ext2emu as special cases.
typedef enum {CHECK_CRC, CHECK_CHKSUM, CHECK_PARITY, CHECK_XOR8, CHECK_XOR16,
              CHECK_NONE} CHECK_TYPE;

typedef struct {
   char *name;
      // Sector size needs to be smallest value to prevent missing next header
      // for most formats. Some controller formats need the correct value.
   int analyze_sector_size;
      // Rate of MFM clock & data bit cells
   uint32_t clk_rate_hz;
      // Delay from index pulse to we should start capturing data in
      // nanoseconds. Needed to ensure we start reading with the first
      // physical sector
   uint32_t start_time_ns;

   int header_start_poly, header_end_poly;
   int data_start_poly, data_end_poly;

   int start_init, end_init;
   enum {CINFO_NONE, CINFO_CHS, CINFO_LBA} analyze_type;

      // Size of headers not including checksum
   int header_bytes, data_header_bytes;
      // These bytes at start of header and data header ignored in CRC calc
   int header_crc_ignore, data_crc_ignore;
   CHECK_TYPE header_check, data_check;

      // These bytes at the end of the data area are included in the CRC
      // but should not be written to the extract file.
   int data_trailer_bytes;
      // 1 if data area is separate from header. 0 if one CRC covers both
   int separate_data;
      // Layout of track.
   TRK_L *track_layout;
      // Sector size to use for converting extract to emulator file
   int write_sector_size;
      // And number of sectors per track
   int write_num_sectors;
      // And first sector number
   int write_first_sector_number;
      // Number of 32 bit words in track MFM data
   int track_words;

      // Non zero of drive has metadata we which to extract.
   int metadata_bytes;
      // Number of extra MFM 32 bit words to copy when moving data around
      // to fix read errors. Formats that need a bunch of zero before
      // a one should use this to copy the zeros.
   int copy_extra;

      // Check information
   CRC_INFO write_header_crc, write_data_crc;
      // Analize is use full search on this format. Model is use
      // the specific data only.
      // TODO: Analyze should search model for specific models before
      // doing generic first. We may want to switch this to bit mask if
      // some we will use as specific model then try search with different
      // polynomials.
   enum {CONT_ANALYZE, CONT_MODEL} analyze_search;

      // Minimum number of bits from last good header. Zero if not used
      // Both must be zero or non zero
   int header_min_delta_bits, data_min_delta_bits;
     // Minimum number of bits from index for first header
   int first_header_min_bits;
     // Special decoding flags
   int flag;
     // If 0, controller is MFM. Otherwise, it's RLL.
   int is_rll;
     // We found a spare sector. Used to separate ST11M and ST11MB
   #define FLAG_ANALYZE_SPARE_SECT 1
     // This format needs xebec_skew set
   #define FLAG_XEBEC_SKEW 2
     // This is a Xebec format. Used to warn xebec_skew might be needed.
   #define FLAG_XEBEC 4
} CONTROLLER;

// These are the formats we will search through.
DEF_EXTERN struct {
   uint64_t poly;
   int length;
   int ecc_span;
} mfm_all_poly[]
#ifdef DEF_DATA
   = {
  // Length 0 for parity (Symbolics 3640). Doesn't really use length
  // also used for CHECK_NONE
  {0, 0, 0},
  // Length 16 for Northstar header checksum
  {0, 16, 0},
  // Length 32 for Northstar data checksum
  {0, 32, 0},
  // Length 8 for Wang header checksum
  {0, 8, 0},
  // This seemed to have more false corrects than other 32 bit polynomials with
  // more errors than can be corrected. Had false correction at length 5 on disk
  // read so dropped back to 4. My attempt to test showed 7 gives 3-42 false
  // corrections per 100000. Some controllers do 11 bit correct with 32 bit
  // polynomial. Had false ECC corrections with length 3 on another disk so dropped it
  // to 2.
  {0x00a00805, 32, 2},
  // Don't move this without fixing the Northstar reference
  {0x1021, 16, 0},
  {0x8005, 16, 0},
  // CTM9016
  {0x0001, 16, 0},
  // The rest of the 32 bit polynomials with 8 bit correct get 5-19 false
  // corrects per 100000 when more errors than can be corrected. Reduced due
  // to false correct seen with 0x00a00805
  {0x140a0445, 32, 6},
  {0x140a0445000101ll, 56, 22}, // From WD42C22C datasheet, not tested
  {0x0104c981, 32, 6},
  // The Shugart SA1400 that uses this polynomial says it does 4 bit correct.
  // That seems to have excessive false corrects when more errors that can be
  // corrected so went with 2 bit correct which has 43-141 miscorrects
  // per 100000 for data with more errors than can be corrected.
  {0x24409, 24, 2},
  {0x3e4012, 24, 0}, // WANG 2275. Not a valid ECC code so max correct 0
  {0x88211, 24, 2}, // ROHM_PBX
  // Adaptec bad block on Maxtor XT-2190
  {0x41044185, 32, 6},
  // MVME320 controller
  {0x10210191, 32, 6},
  // Shugart 1610
  {0x10183031, 32, 6},
  // DSD 5217
  {0x00105187, 32, 6},
  // David Junior II DJ_II
  {0x5140c101, 32, 6},
  // Nixdorf
  {0x8222f0804bda23ll, 56, 22}
  // DQ604 Not added to search since more likely to cause false
  // positives that find real matches
  //{0x1, 8, 0}
  // From uPD7261 datasheet. Also has better polynomials so commented out
  //{0x1, 16, 0}
  // From 9410 CRC checker. Not seen on any drive so far
  //{0x4003, 16, 0}
  //{0xa057, 16, 0}
  //{0x0811, 16, 0}
}
#endif
;

DEF_EXTERN struct {
   int length; // -1 indicates valid for all polynomial size
   uint64_t value;
}  mfm_all_init[]
#ifdef DEF_DATA
 =
   {{-1, 0}, {-1, 0xffffffffffffffffll}, {32, 0x2605fb9c}, {32, 0xd4d7ca20},
     {32, 0x409e10aa},
     // 256 byte OMTI
     {32, 0xe2277da8},
     // This is 532 byte sector OMTI. Above are other OMTI. They likely are
     // compensating for something OMTI is doing to the CRC like below
     // TODO: Would be good to find out what. File sun_remarketing/kalok*
     {32, 0x84a36c27},

     // These are for iSBC_215. The final CRC is inverted but special
     // init value will also make it match
     // TODO Add xor to CRC to allow these to be removed
     // header
     {32, 0xed800493},
     // 128 byte sector
     {32, 0xec1f077f},
     // 256 byte sector
     {32, 0xde60050c},
     // 512 byte sector
     {32, 0x03affc1d},
     // 1024 byte sector
     {32, 0xbe87fbf4},
     // This is data area for Altos 586. Unknown why this initial value needed.
     {16, 0xe60c},
     // WANG 2275 with all header bytes in CRC
     {24, 0x223808},
     // This is for DILOG_DQ614, header and data
     {32, 0x58e07342},
     {32, 0xcf2105e0},
     // This is for Convergent AWS on Quantum Q2040 header and data
     {32, 0x920d65c0},
     {32, 0xef26129d},
     {16, 0x8026}, // IBM 3174
     {16, 0x551a}  // Altos
  }
#endif
;

DEF_EXTERN struct {
   uint64_t poly;
   int length;
   int ecc_span;
} rll_all_poly[]
#ifdef DEF_DATA
   = {
    // WD1006 address marks
      {0x1021, 16, 0},
    // WD1006 data marks
      {0x140a0445000101ll, 56, 22}, // From WD42C22C datasheet, not tested
   }
#endif
;

DEF_EXTERN struct {
   int length; // -1 indicates valid for all polynomial size
   uint64_t value;
}  rll_all_init[]
#ifdef DEF_DATA
 =
   {
     {-1, 0},
     {-1, 0xffffffffffffffffll},
  }
#endif
;

///////////////////////////////////////////////////////////
// Defining the controller types here, under a unified structure.

DEF_EXTERN CONTROLLER controller_info[]
// Keep sorted by header length for same controller family.
// MUST MATCH order of controller enum
#ifdef DEF_DATA
   = {
      {"CONTROLLER_NONE",        0, 10000000,      0,
         0,0, 0,0,
         0,0, CINFO_NONE,
         0, 0, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 0, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, 0,
         0, 0, 0, 0, 0
      },
      {"NewburyData",          256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Altos",              256, 8680000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0x551a,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Superbrain",  256, 10000000, 230000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 1, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
// Should be model after data filled in
         0, 33,
         {0,0x1021,16,0},{0,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"WD_1006",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"RQDX2",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 18, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_214_128B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC214_128b, 128, 54, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_214_256B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC214_256b, 256, 32, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_214_512B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC214_512B, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_214_1024B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC214_1024b, 1024, 9, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      // DV Microbee format (Intel_iSBC_214_512B with start_sector 1)
      {"Microbee_WD1002_05",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC214_512B, 512, 17, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL, // THIS NEEDS TO GO BACK TO 0xffffffff!
         0, 0, 0, 0
      },      
      // TODO: Analyze currently can't separate this from Intel_iSBC_214_512B
      // since only different for heads >= 8
      {"Tektronix_6130",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_tektronix_6130, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"NIXDORF_8870",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 2, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 16, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0,0x8222f0804bda23,56,22}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"TANDY_8MEG",              512, 8680000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"WD_3B1",          512, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_3B1, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"TANDY_16B",          512, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_tandy_16b, 512, 17, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"CORVUS_OMNI",        512, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_corvus_omni, 512, 18, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Motorola_VME10",  256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
         0, 0,
         {0,0xa00805,32,4},{0,0xa00805,32,4}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"DTC",             256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_dtc_pc_512B, 512, 17, 0, 5209,
         0, 0,
         {0,0x24409,24,2},{0,0x24409,24,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"DTC_520_256B",             256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_dtc_520_256b, 256, 33, 0, 5209,
         0, 0,
         {0,0x24409,24,2},{0,0x24409,24,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"DTC_520_512B",             512, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_dtc_520_512B, 512, 18, 0, 5209,
         0, 0,
         {0,0x24409,24,2},{0,0x24409,24,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"MacBottom",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Fujitsu-K-10R",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 1, 1, CHECK_CRC, CHECK_CRC,
         0, 1, trk_fujitsu_k_10r, 256, 34, 0, 5209,
         0, 0,
         {0x0,0x1021,16,0},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"CTM9016",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_CTM9016, 1024, 8, 0, 5209,
         0, 0,
         {0x0,0x1021,16,0},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Acorn_A310_podule",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 1, 1, CHECK_CRC, CHECK_CRC,
         0, 1, trk_acorn_a310_podule, 256, 32, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      // Also DEC professional 350 & 380
      {"Elektronika_85",      256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         16, 1, trk_ELEKTROKIKA_85, 512, 16, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Altos_586",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 1, 1, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"ATT_3B2",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_att_3b2, 512, 18, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"ATT_3B2_17sector",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_att_3b2_17sector, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"CONVERGENT_AWS",       256, 10000000, 0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_convergent_aws, 256, 32, 1, 5209,
         0, 0,
         {0x0,0x1021,16,0},{0x0,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"CONVERGENT_AWS_SA1000",       512, 8680000, 0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 16, 0, 5209,
         0, 0,
         {0x920d65c0,0x140a0445,32,6},{0xef26129d,0x140a0445,32,6}, CONT_MODEL,
         8700, 770, 350, 0
      },
      {"WANG_2275",              256, 10000000,      0,
         3, 4, 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 2, 0, CHECK_CHKSUM, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"WANG_2275_B",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"CALLAN",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"IBM_5288",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 4, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"EDAX_PV9900",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 1, 1, 1, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"SHUGART_1610",          512, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_shugart_1610, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x10183031,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"SHUGART_SA1400",            256, 8680000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_shugart_1400, 256, 32, 0, 5209,
         0, 0,
         {0x0,0x24409,24,2},{0x0,0x24409,24,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"ES7978",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 2, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"WD-Microengine",              256, 8680000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 16, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"NEC_4800",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 33, 0, 5209,
         0, 0,
         {0x0,0x1021,16,0},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Souyz-Neon",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 18, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"INFORT-PC02.06",      128, 10000000,      617000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 2, CHECK_CRC, CHECK_CRC,
         3, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0x0,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"HP9133XV",              256, 10000000,      0, 
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly), 
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_HP9133XV, 256, 31, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0
      },
      {"SHUGART_CD9963",          512, 10000000,      0, 
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly), 
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_shugart_1610, 512, 17, 0, 5209,
         512, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x10183031,32,6}, CONT_MODEL,
         0, 0, 0, 0
      },
      // OMTI_5200 uses initial value 0x409e10aa for data
      // Data CRC is really initial value 0 xor of final value of 0xffffffff.
      // Code doesn't do final xor so initial value is equivalent.
      {"SM_1810_512B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_sm_1810, 512, 16, 0, 5209,
         0, 0,
         {0xed800493,0xa00805,32,4},{0x03affc1d,0xa00805,32,4}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"DSD_5217_512B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_DSD_5217_512B, 512, 17, 0, 5209,
         0, 0,
         {0xffffffff,0x105187,32,6},{0xffffffff,0x105187,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      // For 20D controller 256 byte sectors polynomial is 0xe2277da8,0x104c981,32,6
      // Left as CONT_ANALYZE since 20D doesn't have CONT_MODEL format. ext2emu
      // won't work for 20D. TODO: Add 20D and convert this to CONT_MODEL.
      {"OMTI_5510",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_omti_5510, 512, 17, 0, 5209,
         0, 0,
         { 0x2605fb9c,0x104c981,32,6},{0xd4d7ca20,0x104c981,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"OMTI_5200_18sector_512B",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_omti_5200_18sector_512B, 512, 18, 0, 5209,
         0, 0,
         { 0x2605fb9c,0x104c981,32,6},{0xd4d7ca20,0x104c981,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Xerox_6085",           256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         20, 0,
         { 0x2605fb9c,0x104c981,32,6},{0xd4d7ca20,0x104c981,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Telenex_Autoscope",           256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         { 0x2605fb9c,0x104c981,32,6},{0xd4d7ca20,0x104c981,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Morrow_MD11",            1024, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 1024, 9, 0, 5209,
         0, 0,
         { 0x2605fb9c,0x104c981,32,6},{0xd4d7ca20,0x104c981,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Unknown1",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 1, 1, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         { 0x2605fb9c,0x104c981,32,6},{0xd4d7ca20,0x104c981,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Unknown2",            256, 8680000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 32, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffff,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"DEC_RQDX3",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      // ext2emu not supported since it used deleted data 0xf8 on some
      // sectors which we currently don't have a good way to handle
      {"MYARC_HFDC",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_myarc_hfdc, 256, 32, 0, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"IBM_3174",            256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 516, 17, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0x8026,0x1021,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Seagate_ST11M",        256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_seagate_ST11M, 512, 17, 0, 5209,
         0, 0,
         {0x0,0x41044185,32,6},{0x0,0x41044185,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Seagate_ST11MB",        256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0x0,0x41044185,32,6},{0x0,0x41044185,32,6}, CONT_MODEL,
         0, 0, 0, FLAG_ANALYZE_SPARE_SECT
      },
      {"Intel_iSBC_215_128B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC215_128b, 128, 54, 0, 5209,
         0, 0,
         {0xed800493,0xa00805,32,4},{0xbe87fbf4,0xa00805,32,4}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_215_256B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC215_256b, 256, 31, 0, 5209,
         0, 0,
         {0xed800493,0xa00805,32,4},{0xbe87fbf4,0xa00805,32,4}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_215_512B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC215_512B, 512, 17, 0, 5209,
         0, 0,
         {0xed800493,0xa00805,32,4},{0xbe87fbf4,0xa00805,32,4}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Intel_iSBC_215_1024B",      128, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_ISBC215_1024b, 1024, 9, 0, 5209,
         0, 0,
         {0xed800493,0xa00805,32,4},{0xbe87fbf4,0xa00805,32,4}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
// Quantum Q20#0 Xerox Star drive
// SA100# 8" drives with similar format won't work properly with this
// format and ext2emu due to different rotation speed.
      {"Xerox_8010",      128, 8500000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 2, 2, CHECK_CRC, CHECK_CRC,
         0, 1, trk_xerox_8010, 512, 16, 0, 5425,
         24, 0,
         {0xffff,0x8005,16,0},{0xffff,0x8005,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"ROHM_PBX",      256, 8680000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 1, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 32, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0}, {0, 0x88211, 24, 2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Dimension-68000",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0x0,0x41044185,32,6},{0x0,0x41044185,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"DJ_II_210",            256, 10000000,      0,
         0, 0, 4, ARRAYSIZE(mfm_all_poly),
         0, 0, CINFO_CHS,
         6, 2, 2, 2, CHECK_XOR8, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0x5140c101,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"DJ_II_301",            256, 10000000,      0,
         0, 0, 4, ARRAYSIZE(mfm_all_poly),
         0, 0, CINFO_CHS,
         6, 2, 2, 2, CHECK_XOR8, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
         0, 0,
         {0,0,8,0},{0,0x5140c101,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      // Header is either 6 or 10 bytes
      {"DJ_II",            256, 10000000,      0,
         0, 0, 4, ARRAYSIZE(mfm_all_poly),
         0, 0, CINFO_CHS,
         6, 2, 0, 2, CHECK_NONE, CHECK_CRC,
         0, 1, NULL, 256, 32, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0x5140c101,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Adaptec",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_LBA,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Adaptec_4000_18sector_512B",              256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_LBA,
         6, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_Adaptec_4000_18sector_512B, 512, 18, 0, 5209,
         0, 0,
         {0x0,0x41044185,32,6},{0x0,0x41044185,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"MVME320",        256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         7, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_mvme320, 256, 32, 1, 5209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffff,0x10210191,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Symbolics_3620",       256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         7, 3, 3, 3, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
// Should be model after data filled in
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"SM1040",       256, 10000000,      0,
         0, 1, 4, ARRAYSIZE(mfm_all_poly),
         0, 1, CINFO_CHS,
         10, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0x6e958e56,0x140a0445,32,6},{0xcf2105e0,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Symbolics_3640",       256, 10000000,      0,
         0, 1, 4, ARRAYSIZE(mfm_all_poly),
         0, 1, CINFO_CHS,
         11, 2, 7, 2, CHECK_PARITY, CHECK_CRC,
         0, 1, trk_symbolics_3640, 1160, 8, 0, 5209,
         0, 0,
         {0x0,0x0,0,0},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"OMTI_20L",            256, 10000000,      0,
         0, 0, 4, ARRAYSIZE(mfm_all_poly),
         0, 0, CINFO_CHS,
         20, 1, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 256, 38, 0, 5209,
         16, 0,
         {0x85271cf0,0x0104c981,32,6},{0x3b4292c3,0x0104c981,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
// This format is detected by special case code so it doesn't need to
// be sorted by number. It should not be part of a normal search
// since it will match wd_1006 for drives less than 8 heads
// CONT_MODEL is currently doing TODO: when model support
// added revisit this
      {"Mightyframe",          256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
// This format is detected by special case code so it doesn't need to
// be sorted by number. It should not be part of a normal search
// since it will match wd_1006 for drives less than 8 heads
// CONT_MODEL is currently doing TODO: when model support
// added revisit this
      {"DG_MV2000",          256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
// END of WD type controllers
      {"SOLOsystems",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         7, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"DILOG_DQ614",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         8, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"DILOG_DQ604",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         8, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0,0x1,8,0},{0,0x1,8,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
//    Changed begin time from 0 to 100500 to work with 1410A. The sample
//    I have of the 104786 says it should work with it also so changing default.
//    Its possible this will cause problems with other variants.
      {"Xebec_104786",         256, 10000000,      100500,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, FLAG_XEBEC
      },
      {"Xebec_104527_256B",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_Xebec_104527_256B, 256, 32, 0, 5209,
         0, 0,
         {0x0,0xa00805,32,2},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, FLAG_XEBEC
      },
      {"Xebec_104527_512B",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_Xebec_104527_512B, 512, 17, 0, 5209,
         0, 0,
         {0x0,0xa00805,32,2},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, FLAG_XEBEC
      },
      // This has data compare byte 0 vs 0xc9 for format above. Also
      // needs begin_time 225000
      {"Xebec_104527_C0_256B",         256, 10000000,      225000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_Xebec_104527_C0_256B, 256, 32, 0, 5209,
         0, 0,
         {0x0,0xa00805,32,2},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, FLAG_XEBEC_SKEW
      },
#if 0
      {"TI_2223220",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 17, 0, 5209,
         0, 0,
         {0x0,0xa00805,32,2},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
#endif
      {"Xebec_S1420",         256, 10000000,      0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 0, 0, 0, 5209,
         0, 0,
         {0,0,0,0},{0,0,0,0}, CONT_ANALYZE,
         0, 0, 0, FLAG_XEBEC
      },
      {"EC1841",         256, 10000000,      220000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_EC1841, 512, 17, 0, 5209,
         0, 10,
         {0x0,0xa00805,32,2},{0x0,0xa00805,32,2}, CONT_MODEL,
         0, 0, 0, FLAG_XEBEC_SKEW
      },
      {"Corvus_H",             512, 11000000,  312000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         3, 3, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 0, trk_corvus_h, 512, 20, 0, 5730,
         0, 0,
         // Only have one CRC. DATA_CRC needs to be non zero for analyze_model
         // to ignore this format. Also needed by ext2emu for mark_bad to work
         {0xffff,0x8005,16,0},{0xffff,0x8005,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"NorthStar_Advantage",  256, 10000000, 230000,
         1, 2, 2, 3,
         0, 1, CINFO_CHS,
         7, 0, 0, 0, CHECK_CHKSUM, CHECK_CHKSUM,
         0, 1, trk_northstar, 512, 16, 0, 5209,
// Should be model after data filled in
         0, 33,
         {0,0,16,0},{0,0,32,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Cromemco",             10240, 10000000,  6000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         9, 9, 0, 0, CHECK_CRC, CHECK_CRC,
         7, 0, trk_cromemco_stdc, 10240, 1, 0, 5209,
// Should be model after data filled in
         0, 0,
         {0,0x8005,16,0},{0,0x8005,16,0}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Vector4",             256, 10000000,  300000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 4, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 0, NULL, 256, 32, 0, 5209,
// Should be model after data filled in
         0, 20,
         {0x0,0x104c981,32,6},{0x0,0x104c981,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Vector4_ST506",             256, 10000000,  300000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 4, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 0, NULL, 256, 32, 0, 5209,
// Should be model after data filled in
         0, 20,
         {0x0,0x104c981,32,6},{0x0,0x104c981,32,6}, CONT_ANALYZE,
         0, 0, 0, 0, 0
      },
      {"Stride_440",             256, 10000000,  300000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         28, 0, 6, 0, CHECK_CRC, CHECK_CRC,
         1, 0, NULL, 8192, 1, 0, 5209,
         0, 20,
         {0x0,0x8005,16,0},{0x0,0x8005,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"Saga_Fox",             256, 10000000,  330000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         5, 1, 0, 0, CHECK_XOR16, CHECK_XOR16,
         0, 1, trk_saga_fox, 256, 33, 0, 5209,
// Should be model after data filled in
         0, 20,
         {0x0,0,16,0},{0x0,0x0,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"ND100_3041",             256, 10000000,  0,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 0, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, trk_nd100_3041, 1024, 9, 0, 5209,
         0, 20,
         {0x0,0x8005,16,0},{0x0,0x8005,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"PERQ_T2",              256, 10000000,    754000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 0, 0, 0, CHECK_CRC, CHECK_CRC,
         1, 1, trk_perq_t2, 512, 16, 0, 5209,
         16, 0,
         {0x0,0x8005,16,0},{0x0,0x8005,16,0}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"IMS_A820",              256, 10000000,    485000,
         4, ARRAYSIZE(mfm_all_poly), 4, ARRAYSIZE(mfm_all_poly),
         0, ARRAYSIZE(mfm_all_init), CINFO_CHS,
         4, 4, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 0, trk_IMS_A820, 512, 18, 0, 5209,
         0, 0,
         {0xd1e92a5b,0x140a0445,32,6},{0xd1e92a5b,0x140a0445,32,6}, CONT_MODEL,
         0, 0, 0, 0, 0
      },
      {"WD1006V-SR2",          256, 15000000,      0,
         4, ARRAYSIZE(rll_all_poly), 4, ARRAYSIZE(rll_all_poly),
         0, ARRAYSIZE(rll_all_init), CINFO_CHS,
         5, 2, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 1, NULL, 512, 26, 0, 15209,
         0, 0,
         {0xffff,0x1021,16,0},{0xffffffffffffff,0x140a0445000101ll,56,22}, CONT_MODEL,
         0, 0, 0, 0, 1
      },
      {NULL, 0, 0, 0,
         0, 0, 0, 0,
         0,0, CINFO_NONE,
         0, 0, 0, 0, CHECK_CRC, CHECK_CRC,
         0, 0, NULL, 0, 0, 0, 0,
         0, 0,
         {0,0,0,0},{0,0,0,0}, 0,
         0, 0, 0, 0, 0
      },
   }
#endif
;

// Best_track is the best track read as one read. Best_fixed_track is
// the best track by putting together multiple reads
extern uint32_t best_track_words[MAX_TRACK_WORDS];
extern int best_track_weight;
extern int best_track_num_words;
extern uint32_t best_fixed_track_words[MAX_TRACK_WORDS];
extern int best_fixed_track_weight;
extern int best_fixed_track_num_words;
extern uint32_t current_track_words[MAX_TRACK_WORDS];
extern int current_track_words_ndx;

extern int cyl_found[4096];

extern uint8_t sector_good[MAX_CYL][MAX_HEAD][MAX_SECTORS];

extern uint64_t sector_crc[MAX_CYL][MAX_HEAD][MAX_SECTORS];

// Define states for processing the data. MARK_ID is looking for the 0xa1 byte
// before a header and MARK_DATA is same for the data portion of the sector.
// MARK_DATA1 is looking for special Symbolics 3640 mark code.
// MARK_DATA2 is looking for special ROHM PBX mark code.
// DATA_SYNC2 is looking for SUPERBRAIN
// PROCESS_HEADER is processing the header bytes and PROCESS_DATA processing
// the data bytes. HEADER_SYNC and DATA_SYNC are looking for the one bit to sync to
// in CONTROLLER_XEBEC_104786. Not all decoders use all states.
typedef enum { MARK_ID, MARK_DATA, MARK_DATA1, MARK_DATA2, HEADER_SYNC, HEADER_SYNC2, DATA_SYNC, DATA_SYNC2, PROCESS_HEADER, PROCESS_HEADER2, PROCESS_DATA
} STATE_TYPE;

int dc_cmpint(const void *i1, const void *i2);
float dc_filter(float v, float *delay);

// Hold for sector status and cylinder and head it was for. We save the
// data so when the cylinder and head changes we can print the final status.
// The same track may be reread.
extern SECTOR_STATUS last_sector_list[MAX_SECTORS];
extern int last_cyl;
extern int last_head;

// Last LBA address processed for detecting bad sectors
extern int last_lba_addr;

extern CONTROLLER controller_info[];

void dc_update_stats(DRIVE_PARAMS *drive_params, int cyl, int head,
      SECTOR_STATUS sector_status_list[]);

void dc_update_emu_track_words(DRIVE_PARAMS * drive_params,
      SECTOR_STATUS sector_status_list[], int write_track, int new_track,
      int cyl, int head);

void dc_print_status_flags(SECTOR_DECODE_STATUS status);

SECTOR_DECODE_STATUS dc_crc_bytes(DRIVE_PARAMS *drive_params,
   uint8_t bytes[], int bytes_crc_len, int state, uint64_t *crc_ret,
   int *ecc_span, SECTOR_DECODE_STATUS *init_status, int perform_ecc);

void dc_check_header_values(int exp_cyl, int exp_head,
      int *sector_index, int sector_size, int *seek_difference,
      SECTOR_STATUS *sector_status, DRIVE_PARAMS *drive_params,
      SECTOR_STATUS sector_status_list[]);

int dc_write_sector(uint8_t bytes[], DRIVE_PARAMS * drive_params,
      SECTOR_STATUS *sector_status, SECTOR_STATUS sector_status_list[],
      uint8_t all_bytes[], int all_bytes_len);

// #undef DEF_EXTERN
#endif // DECODER_COMMON_H_
