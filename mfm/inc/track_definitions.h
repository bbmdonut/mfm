#ifndef TRACK_DEFS_H__
#define TRACK_DEFS_H__

//
// track_definitions.h
//
//  Created on: Mar 31, 2025
//      Author: BBMD (Based on code from DJG, others)
//
// 03/31/25 BBMD Moved the definitions for track layouts into a common header,
//    in order to add RLL formats when ready
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

// This defines fields that aren't whole multiples of bytes.
// Filling starts with the most significant bit
typedef struct bit_l {
      // Start bit, bits are numbered with most significant bit of the
      // first byte 0
   int bitl_start;
      // Field length in bits
   int bitl_length;
} BIT_L;

// This defines the fields in sector header and data areas
typedef struct field_l {
      // Length of field in bytes. If defined by bit fields set to 0
   int len_bytes;
      // Type of field,
      // FIELD_FILL fills the specified number of bytes with value
      // FIELD_A1 writes the A1 header/data mark code. Only 1 byte valid.
      // FIELD_C0 writes the C0 header/data mark code. Only 1 byte valid.
      // FIELD_CYL, HEAD, SECTOR, LBA write the current cylinder, head, sector,
      //   or logical block address. Valid for byte of bit fields.
      // FIELD_HDR_CRC and FIELD_DATA_CRC write the check word. The
      //    CONTROLLER data defines the type of check word.
      // FIELD_MARK_CRC_START and END are used to mark the start and end
      // byte for CRC (check data) calculation. The default
      // includes the all the data from sector start flag byte (a1 etc) to
      // the CRC.
   enum {FIELD_FILL, FIELD_A1, FIELD_C0, FIELD_42, FIELD_85, FIELD_0A, FIELD_10,
      FIELD_CYL, FIELD_HEAD, FIELD_SECTOR,
      FIELD_LBA, FIELD_HDR_CRC, FIELD_DATA_CRC, FIELD_SECTOR_DATA,
      FIELD_MARK_CRC_START, FIELD_MARK_CRC_END,
      FIELD_BAD_SECTOR,
         // These are for controllers that need special handling
      FIELD_HEAD_SEAGATE_ST11M, FIELD_CYL_SEAGATE_ST11M,
         // Mark end of sector to increment sector counter
      FIELD_NEXT_SECTOR, FIELD_SECTOR_METADATA,
         // Like FIELD_FILL but only takes effect if last sector on track
      FIELD_FILL_LAST_SECTOR
      } type;
      // Value for field.
   uint8_t value;
      // OP_SET writes the data to the field, OP_XOR exclusive or's it
      // with the current contents, OP_REVERSE reverses the bits then does
      // an OP_SET.
   enum {OP_SET, OP_XOR, OP_REVERSE, OP_REVERSE_XOR} op;
      // If bit_list is null is is the byte from start of field.
      // If bit_list is not null this is the length of the field in bits.
   int byte_offset_bit_len;
      // The list of bits in field if not null.
   BIT_L *bit_list;
} FIELD_L;

// This defines all the data in the track. Each operation starts at the
// end of the previous one.
typedef struct trk_l {
      // The count for the field. For TRK_FIll and TRK_FIELD its the number
      // of bytes,
      // for TRK_SUB its the number of times the specificied list should be
      // repeated.
   int count;
      // TRK_FILL fills the specified length of bytes.
   enum {TRK_FILL, TRK_SUB, TRK_FIELD} type;
      // Only used for TRK_FILL.
   uint8_t value;
      // Pointer to a TRK_L for TRK_SUB or FIELD_L for TRK_FIELD
   void *list;
} TRK_L;

// *** NOTE: These define the track starting from start_time_ns ****
//
// For more information on the track formats see the *decoder.c files.
//
// TODO, use these tables to drive reading the data also instead of the
// current hard coded data.
// Will likely need a separate op for reading.
//
// Format for AT&T 3B1 computer
DEF_EXTERN TRK_L trk_3B1[]
#ifdef DEF_DATA
 =
{ { 45, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {15, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 512
              {1, FIELD_FILL, 0x20, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {516, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {38, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {275, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;
// TANDY 16B. From example XENIX.emu in emu.zip. Minor change to 3b1 format. Probably minor padding changes don't matter.
DEF_EXTERN TRK_L trk_tandy_16b[]
#ifdef DEF_DATA
 =
{ { 34, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 512
              {1, FIELD_FILL, 0x20, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {516, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {33, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {388, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// Corvus Omnidrive
DEF_EXTERN TRK_L trk_corvus_omni[]
#ifdef DEF_DATA
 =
{ { 48, TRK_FILL, 0x4e, NULL },
  { 18, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {11, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 512
              {1, FIELD_FILL, 0x20, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {516, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {17, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {388, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// Format from http://www.bitsavers.org/pdf/intel/iSBC/134910-001_iSBC_214_Peripheral_Controller_Subsystem_Hardware_Reference_Manual_Aug_85.pdf

// Format from http://www.bitsavers.org/pdf/intel/iSBC/134910-001_iSBC_214_Peripheral_Controller_Subsystem_Hardware_Reference_Manual_Aug_85.pdf
// Four sectors sizes for Intel iSBC214 controller
DEF_EXTERN TRK_L trk_ISBC214_128b[]
#ifdef DEF_DATA
 =
{ { 15, TRK_FILL, 0x4e, NULL },
  { 54, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 128
              {1, FIELD_FILL, 0x60, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {134, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {128, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 130, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {15, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {251, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_ISBC214_256b[]
#ifdef DEF_DATA
 =
{ { 15, TRK_FILL, 0x4e, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 256
              {1, FIELD_FILL, 0x00, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {262, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {15, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {291, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;
DEF_EXTERN TRK_L trk_ISBC214_512B[]
#ifdef DEF_DATA
 =
{ { 38, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 512
              {1, FIELD_FILL, 0x20, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {38, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {265, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_ISBC214_1024b[]
#ifdef DEF_DATA
 =
{ { 54, TRK_FILL, 0x4e, NULL },
  { 9, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 1024
              {1, FIELD_FILL, 0x40, OP_SET, 3, NULL},
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {1030, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {1024, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 1026, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {54, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {257, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// Format from http://www.bitsavers.org/pdf/intel/iSBC/144780-002_iSBC_215_Generic_Winchester_Disk_Controller_Hardware_Reference_Manual_Dec84.pdf. Gaps not
// specified in manual so iSBC_214 values used
// Four sectors sizes for Intel iSBC215 controller
// **** NOTE, these don't currently generate image readable by the controller.
// **** More investigation needed if sufficient interest
DEF_EXTERN TRK_L trk_ISBC215_128b[]
#ifdef DEF_DATA
 =
{ { 15, TRK_FILL, 0x4e, NULL },
  { 54, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x19, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // Sector size 128. All tracks data, alternate not supported
              {1, FIELD_FILL, 0x00, OP_SET, 2, NULL},
              // Upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {134, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xd9, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {128, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 130, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {13, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {251, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_ISBC215_256b[]
#ifdef DEF_DATA
 =
{ { 15, TRK_FILL, 0x4e, NULL },
  { 31, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x19, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // Sector size 256. All tracks data, alternate not supported
              {1, FIELD_FILL, 0x10, OP_SET, 2, NULL},
              // Upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {262, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xd9, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {17, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {452, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;
DEF_EXTERN TRK_L trk_ISBC215_512B[]
#ifdef DEF_DATA
 =
{ { 38, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x19, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // Sector size 512. All tracks data, alternate not supported
              {1, FIELD_FILL, 0x20, OP_SET, 2, NULL},
              // Upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xd9, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {36, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {265, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_ISBC215_1024b[]
#ifdef DEF_DATA
 =
// This is from inspecting data from drive. First data after index
// does not seem to be deterministic
{ { 15, TRK_FILL, 0x00, NULL },
  { 9, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {15, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x19, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // Sector size 1024. All tracks data, alternate not supported
              {1, FIELD_FILL, 0x30, OP_SET, 2, NULL},
              // Upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {5, TRK_FILL, 0x4e, NULL},
        {12, TRK_FILL, 0x00, NULL},
        {1030, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xd9, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {1024, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 1026, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {65, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {143, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_tektronix_6130[]
#ifdef DEF_DATA
 =
{ { 38, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 2 bits of cylinder to bits 1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Sector size 512
              {1, FIELD_FILL, 0x20, OP_SET, 3, NULL},
              // Add head to lower bits
              {0, FIELD_HEAD, 0x00, OP_XOR, 4,
                 (BIT_L []) {
                    { -2, 1}, // -2 says discard this bit. Goes in different byte
                    { 29, 3},
                    { -1, -1},
                 }
              },
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {0, FIELD_HEAD, 0x00, OP_XOR, 4,
                 (BIT_L []) {
                    { 33, 1},
                    { -2, 3},	// -2 says discard these bits
                    { -1, -1},
                 }
              },
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {15, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {38, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {265, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;


DEF_EXTERN TRK_L trk_convergent_aws[]
#ifdef DEF_DATA
 =
{ { 15, TRK_FILL, 0x00, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 2, NULL},
              // Put head in bits 7-4 of byte 2
              {1, FIELD_HEAD, 0x00, OP_XOR, 4,
                 (BIT_L []) {
                    { 16, 4},
                    { -1, -1},
                 }
              },
              // Cyl in upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {19, TRK_FILL, 0x00, NULL},
        {260, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {19, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {195, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_ELEKTROKIKA_85[]
#ifdef DEF_DATA
 =
{ { 2, TRK_FILL, 0x00, NULL },
  { 16, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {532, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x80, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {16, FIELD_FILL, 0x00, OP_SET, 514, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 530, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {40, TRK_FILL, 0x55, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {672, TRK_FILL, 0x55, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// From hand written notes 20201020_225628.jpg slightly adjusted to
// match disk image
DEF_EXTERN TRK_L trk_sm_1810[]
#ifdef DEF_DATA
 =
{ { 15, TRK_FILL, 0x4e, NULL },
  { 16, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // Sector size 512. All tracks data, alternate not supported
              {1, FIELD_FILL, 0x20, OP_SET, 2, NULL},
              // Upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {16, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {4, TRK_FILL, 0x00, NULL},
        {24, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {1059, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// From http://www.bitsavers.org/pdf/dsd/5215_5217/040040-01_5215_UG_Apr84.pdf
DEF_EXTERN TRK_L trk_DSD_5217_512B[]
#ifdef DEF_DATA
 =
{ { 7, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // Sector size 512. All tracks data, alternate not supported
              {1, FIELD_FILL, 0x20, OP_SET, 2, NULL},
              // Upper 4 bits in low 4 bits of byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 20, 4},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        // 3 after header and 12 before data field
        {3, TRK_FILL, 0x4e, NULL},
        {6, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfb, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {44, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {330, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;


// From http://www.bitsavers.org/pdf/sms/asic/OMTI_5050_Programmable_Data_Sequencer_Jun86.pdf
// Appendix A
DEF_EXTERN TRK_L trk_omti_5510[]
#ifdef DEF_DATA
 =
{ { 11, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 2, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 4, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {14, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {14, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {717, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_omti_5200_18sector_512B[]
#ifdef DEF_DATA
 =
{ { 11, TRK_FILL, 0x4e, NULL },
  { 18, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 2, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 4, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 5, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {14, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {14, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {147, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// From http://www.mirrorservice.org/sites/www.bitsavers.org/pdf/sms/asic/OMTI_5050_Programmable_Data_Sequencer_Jun86.pdf
// Appendix A
DEF_EXTERN TRK_L trk_mvme320[]
#ifdef DEF_DATA
 =
{ { 20, TRK_FILL, 0x4e, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {9, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 2, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 4, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 5, NULL},
              {1, FIELD_FILL, 0x01, OP_SET, 6, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 7, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {4, TRK_FILL, 0x4e, NULL},
        {12, TRK_FILL, 0x00, NULL},
        {262, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfb, OP_SET, 1, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {350, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// From looking at data read from a drive
DEF_EXTERN TRK_L trk_symbolics_3640[]
#ifdef DEF_DATA
 =
{ { 8, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {47, TRK_FILL, 0x00, NULL},
        {11, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x5a, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x96, OP_SET, 2, NULL},
              {1, FIELD_FILL, 0x0e, OP_SET, 3, NULL},
              {1, FIELD_FILL, 0x0e, OP_SET, 4, NULL},
              {1, FIELD_FILL, 0x9e, OP_SET, 5, NULL},
              {1, FIELD_FILL, 0x01, OP_SET, 6, NULL},
              {0, FIELD_SECTOR, 0x00, OP_REVERSE, 3,
                 (BIT_L []) {
                    { 56, 3},
                    { -1, -1},
                 }
              },
              {0, FIELD_HEAD, 0x00, OP_REVERSE, 4,
                 (BIT_L []) {
                    { 62, 4},
                    { -1, -1},
                 }
              },
              {0, FIELD_CYL, 0x00, OP_REVERSE, 12,
                 (BIT_L []) {
                    { 68, 12},
                    { -1, -1},
                 }
              },
              {1, FIELD_HDR_CRC, 0x00, OP_SET, 10, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {25, TRK_FILL, 0x00, NULL},
        {1165, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x0f, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1160, FIELD_SECTOR_DATA, 0x00, OP_REVERSE, 1, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 1161, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {49, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {42, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;


// From looking at data read from a drive and manual. Commented out values
// are what the manual specified which didn't work and didn't match captured
// data. The controller could read data written with this format but got
// errors after it wrote to a track.
DEF_EXTERN TRK_L trk_northstar[]
#ifdef DEF_DATA
 =
//{ { 44, TRK_FILL, 0xff, NULL },
{ { 69, TRK_FILL, 0xff, NULL },
  { 3, TRK_FILL, 0x55, NULL },
  //{ 40, TRK_FILL, 0xff, NULL },
  { 8, TRK_FILL, 0xff, NULL },
  { 16, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {67, TRK_FILL, 0x00, NULL},
// This is somewhat inconsistant. The Symbolics 3640 needs the 1 as part of the
// header, Northstar code assumes it is not.
        { 1, TRK_FILL, 0x01, NULL},
        { 525, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_SECTOR, 0x00, OP_SET, 0, NULL},
              {0, FIELD_CYL, 0x00, OP_SET, 12,
                 (BIT_L []) {
                    { 0, 4},
                    { 8, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 2, NULL},
              {4, FIELD_FILL, 0x00, OP_SET, 3, NULL},
              {1, FIELD_FILL, 0xff, OP_SET, 8, NULL},
              {1, FIELD_HDR_CRC, 0x00, OP_SET, 7, NULL},
              {1, FIELD_HDR_CRC, 0x00, OP_XOR, 8, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 9, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 9, NULL},
              {0, FIELD_MARK_CRC_END, 0, OP_SET, 520, NULL},
              {2, FIELD_FILL, 0xff, OP_SET, 523, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 521, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_XOR, 523, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        //{45, TRK_FILL, 0x00, NULL},
        {49, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   //{121, TRK_FILL, 0xff, NULL},
   {64, TRK_FILL, 0xff, NULL},
   {-1, 0, 0, NULL}
}
#endif
;

// Disk sample had cylinders marked spare starting at 1011. Unclear where ended
// due to disk read errors. 1021 head 7 had cylinder field set
// to 2045. 1022 and 1023 are unformatted. Formatting a new emulator file
// cylinders starting at 1011 weren't marked spare. Otherwise the same
// None of these are implemented by this format definition. Seems to work anyway
DEF_EXTERN TRK_L trk_nd100_3041[]
#ifdef DEF_DATA
 =
{ { 9, TRK_SUB, 0x00,
     (TRK_L [])
     {
        { 62, TRK_FILL, 0x00, NULL},
        { 1, TRK_FILL, 0x01, NULL},  // 1 marking header start
        { 32, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x00, OP_SET, 0, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 1, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 3, NULL},
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 16, 8},
                    { 24, 3},
                    { -1, -1},
                 }
              },
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 4, NULL},
              {26, FIELD_FILL, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           },
        },
        { 1, TRK_FILL, 0x01, NULL},  // 1 marking data start
        { 1026, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1024, FIELD_SECTOR_DATA, 0x00, OP_SET, 0, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 1024, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {-1, 0, 0, NULL},
     }
   },
   {320, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL}
}
#endif
;

DEF_EXTERN TRK_L trk_seagate_ST11M[]
#ifdef DEF_DATA
 =
{ { 19, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {10, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {1, FIELD_HEAD_SEAGATE_ST11M, 0x00, OP_SET, 2, NULL},
                 // On first cylinder byte 2 is 0xff. This is set by
                 // FIELD_HEAD_SEAGATE_ST11M. The XOR prevents clearing
                 // the upper 2 bits
              {0, FIELD_CYL_SEAGATE_ST11M, 0x00, OP_XOR, 10,
                 (BIT_L []) {
                    { 16, 2},
                    { 24, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 5, NULL}, // Spare flags
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {20, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {622, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_cromemco_stdc[]
#ifdef DEF_DATA
 =
{ { 40, TRK_FILL, 0x00, NULL },
  { 1, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {5, TRK_FIELD, 0x00,  // Track header
           (FIELD_L []) {
              {1, FIELD_FILL, 0x04, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xaa, OP_SET, 1, NULL},
              {0, FIELD_CYL, 0x00, OP_SET, 16,  //6
                 (BIT_L []) {
                    { 24, 8}, // High byte
                    { 16, 8}, // Low byte
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 4, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {75, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   { 1, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {10258, TRK_FIELD, 0x00,  // All bytes in CRC must be in TRK_FIELD
           (FIELD_L []) {
              {1, FIELD_FILL, 0x04, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {3, FIELD_FILL, 0xaa, OP_SET, 2, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 5, NULL},
              {0, FIELD_CYL, 0x00, OP_SET, 16,  //6
                 (BIT_L []) {
                    { 56, 8}, // High byte
                    { 48, 8}, // Low byte
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 8, NULL},
              {10240, FIELD_SECTOR_DATA, 0x00, OP_SET, 9, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 10249, NULL},
              {2, FIELD_FILL, 0xaa, OP_SET, 10250, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 10252, NULL},
              {0, FIELD_CYL, 0x00, OP_SET, 16,  //10253
                 (BIT_L []) {
                    { 82032, 8},
                    { 82024, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 10255, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 10256, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {4, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {36, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// 512B 17 sectors per track from unknown DTC controller
DEF_EXTERN TRK_L trk_dtc_pc_512B[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {8, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // This adds upper 3 bits of cylinder to bits 4-6 of
              // byte 3 and the rest in byte 2.
              {0, FIELD_CYL, 0x00, OP_SET, 11,
                 (BIT_L []) {
                    { 25, 3}, // Byte 3 bits 6-4 gets upper 3 bits
                    { 16, 8}, // byte 2 gets lower 8 bits
                    { -1, -1},
                 }
              },
              // Add head to lower bits, Need XOR since SET clears all bits
              // in byte.
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {3, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x00, NULL},
        {517, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {3, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {37, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {304, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// 512B 18 sectors per track from DTC 520 controller manual
DEF_EXTERN TRK_L trk_dtc_520_512B[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 18, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {8, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // This adds upper 3 bits of cylinder to bits 4-6 of
              // byte 3 and the rest in byte 2.
              {0, FIELD_CYL, 0x00, OP_SET, 11,
                 (BIT_L []) {
                    { 25, 3}, // Byte 3 bits 6-4 gets upper 3 bits
                    { 16, 8}, // byte 2 gets lower 8 bits
                    { -1, -1},
                 }
              },
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {3, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {517, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {3, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {14, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {158, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_dtc_520_256b[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 33, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {8, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // This adds upper 3 bits of cylinder to bits 4-6 of
              // byte 3 and the rest in byte 2.
              {0, FIELD_CYL, 0x00, OP_SET, 11,
                 (BIT_L []) {
                    { 25, 3}, // Byte 3 bits 6-4 gets upper 3 bits
                    { 16, 8}, // byte 2 gets lower 8 bits
                    { -1, -1},
                 }
              },
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {3, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {261, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {3, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {10, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {205, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// **** NOTE, these don't currently generate image readable by the controller.
// **** More investigation needed if sufficient interest
// No information on format is available
DEF_EXTERN TRK_L trk_saga_fox[]
#ifdef DEF_DATA
 =
{ { 87, TRK_FILL, 0x00, NULL },
  { 34, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {17, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x0f, OP_SET, 0, NULL},
              // This adds upper 3 bits of cylinder to bits 4-6 of
              // byte 3 and the rest in byte 2.
              {0, FIELD_CYL, 0x00, OP_REVERSE, 16,
                 (BIT_L []) {
                    { 8, 8}, // High byte get written to low after reverse
                    { 16, 8},
                    { -1, -1},
                 }
              },
              // Add head to lower bits
              {1, FIELD_HEAD, 0x00, OP_REVERSE, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_REVERSE, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {19, TRK_FILL, 0x00, NULL},
        {259, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x0f, OP_SET, 0, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_REVERSE, 1, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 257, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {-1, 0, 0, NULL},
     }
   },
   {63, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// Some info in
// http://bitsavers.org/pdf/xerox/8010_dandelion/DandelionHardwareRefRev2.2.pdf
// Starting on page 75 and 88
// The number of fill bytes should be reasonably close. The actual fill byte
// values may not match the real format since the values seen were not
// consistant.
DEF_EXTERN TRK_L trk_xerox_8010[]
#ifdef DEF_DATA
 =
{ { 60, TRK_FILL, 0x4e, NULL },
  { 16, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {14, TRK_FILL, 0x00, NULL},
        {8, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x41, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 2, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 4, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 5, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {21, TRK_FILL, 0x00, NULL},
        {28, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x43, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {24, FIELD_SECTOR_METADATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 26, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {23, TRK_FILL, 0x00, NULL},
        {516, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0x43, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {790, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_perq_t2[]
#ifdef DEF_DATA
 =
{ { 158, TRK_FILL, 0x00, NULL },
  { 16, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {22, TRK_FIELD, 0x00,
           (FIELD_L []) {
              // sector mark
              {1, FIELD_C0, 0xC0, OP_SET, 0, NULL},
              {13, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              // Sync
              {1, FIELD_FILL, 0x0f, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              // Upper 4 bits in upper 4 bits of byte 1, lower 8 bits in
              // byte 0
              {0, FIELD_CYL, 0x00, OP_REVERSE_XOR, 12,
                 // This is reversed to match the bit reversing
                 (BIT_L []) {
                    {120, 8},
                    {132, 4},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_REVERSE_XOR, 16, NULL},
              {1, FIELD_SECTOR, 0x00, OP_REVERSE, 17, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 18, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 19, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 21, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {35, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {14, FIELD_FILL, 0x00, OP_SET, 0, NULL},
              // Sync
              {1, FIELD_FILL, 0x0f, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              {16, FIELD_SECTOR_METADATA, 0x00, OP_REVERSE, 15, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 31, NULL},
              {2, FIELD_FILL, 0x00, OP_SET, 33, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {575, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {14, FIELD_FILL, 0x00, OP_SET, 0, NULL},
              // Sync
              {1, FIELD_FILL, 0x0f, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_REVERSE, 15, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 527, NULL},
              {46, FIELD_FILL, 0x00, OP_SET, 529, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {-1, 0, 0, NULL},
     }
   },
   {148, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_shugart_1610[]
#ifdef DEF_DATA
 =
{ { 26, TRK_FILL, 0x4e, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 0, NULL},
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // Upper 3 bits in bits 6-4 of byte 3, lower 8 bits in
              // byte 2. All tracks marked good track
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 // List is starting from the high bit. Low bit to write to, length
                 (BIT_L []) {
                    { 28, 3},
                    { 16, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 0, NULL},
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x00, NULL},
        {45, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {209, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_shugart_1400[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {8, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              // Upper 3 bits in bits 6-4 of byte 3, lower 8 bits in
              // byte 2. All tracks marked good track
              {1, FIELD_CYL, 0x00, OP_SET, 2, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {3, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {15, TRK_FILL, 0x00, NULL},
        {261, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 2, NULL},
              {256, FIELD_FILL, 0xff, OP_SET, 2, NULL},
              // Data is inverted. Fill with 1's then XOR in data
              {256, FIELD_SECTOR_DATA, 0x00, OP_XOR, 2, NULL},
              {3, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {354, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// Mixture of datasheet and formatter gap lengths adjusted with actual
// file. Not sure if really correct. The pad at the end seems a little
// low.
DEF_EXTERN TRK_L trk_myarc_hfdc[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {13, TRK_FILL, 0x00, NULL},
        {8, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 3,1,0 of
              // the 0xfe byte and the rest in the next byte. The cylinder
              // bits are xored with the 0xfe. Xor with 0 just sets the bits
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 12, 1},
                    { 14, 10},
                    { -1, -1},
                 }
              },
              // Put head in lower bits
              {1, FIELD_HEAD, 0x00, OP_XOR, 3, NULL},
              // Put upper 3 bits of cyl in bits 6-4
              {0, FIELD_CYL, 0x00, OP_XOR, 11,
                 (BIT_L []) {
                    { 25, 3},
                    { 32, 8}, // Dump the rest of the bits where it will be overwritten
                    { -1, -1},
                 }
              },
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              // 4 byte ECC, Sector size 256
              {1, FIELD_FILL, 0x01, OP_SET, 5, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x4e, NULL},
        {13, TRK_FILL, 0x00, NULL},
        {262, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {3, TRK_FILL, 0x00, NULL},
        {15, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {258, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_acorn_a310_podule[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {16, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x4e, NULL},
        {16, TRK_FILL, 0x00, NULL},
        {262, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {162, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_fujitsu_k_10r[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL },
  { 34, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {8, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {2, TRK_FILL, 0x4e, NULL},
        {12, TRK_FILL, 0x00, NULL},
        {262, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 258, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {11, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {134, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_CTM9016[]
#ifdef DEF_DATA
 =
{ { 257, TRK_FILL, 0x4e, NULL },
  { 8, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {24, TRK_FILL, 0x00, NULL},
        {7, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {2, FIELD_CYL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {27, TRK_FILL, 0x00, NULL},
        {1030, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {1024, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 1026, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {4, TRK_FILL, 0x00, NULL},
        {98, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {641, TRK_FILL, 0x4e, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_corvus_h[]
#ifdef DEF_DATA
 =
{ { 7, TRK_FILL, 0x00, NULL },
  { 20, TRK_SUB, 0x00,
     (TRK_L [])
     {
        // Real disk seems to have some non zero bytes here but they aren't
        // always the same or a simple pattern so I ignored them
        {49, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x02, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              // Put head in bits 7-5 of byte 1
              {1, FIELD_HEAD, 0x00, OP_XOR, 3,
                 (BIT_L []) {
                    { 8, 3},
                    { -1, -1},
                 }
              },
              // Put sector in bits 4-0 of byte 1
              {0, FIELD_SECTOR, 0x00, OP_XOR, 5,
                 (BIT_L []) {
                    { 11, 5},
                    { -1, -1},
                 }
              },
              {2, FIELD_FILL, 0x00, OP_SET, 2, NULL},
              // Cyl in upper 8 bits in byte 2, lower 8 bits in
              // byte 3
              {0, FIELD_CYL, 0x00, OP_XOR, 16,
                 (BIT_L []) {
                    { 24, 8},
                    { 16, 8},
                    { -1, -1},
                 }
              },
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 4, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 516, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {-1, 0, 0, NULL},
     }
   },
   {113, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_Xebec_104527_256B[]
#ifdef DEF_DATA
 =
{ { 20, TRK_FILL, 0x00, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {8, TRK_FILL, 0x00, NULL},
        {28, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {13, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x01, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              {2, FIELD_FILL, 0x00, OP_SET, 15, NULL},
              {1, FIELD_FILL, 0xc2, OP_SET, 17, NULL},
              // Upper 4 bits in low 4 bits of byte 5, lower 8 bits in
              // byte 6
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 148, 4},
                    { 152, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 20, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 21, NULL},
              {1, FIELD_FILL, 0x80, OP_SET, 22, NULL},
              {1, FIELD_FILL_LAST_SECTOR, 0x10, OP_XOR, 22, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 23, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 24, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x00, NULL},
        {263, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x01, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0xc9, OP_SET, 2, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 3, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 259, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {6, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {94, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_Xebec_104527_512B[]
#ifdef DEF_DATA
 =
{ { 25, TRK_FILL, 0x00, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {16, TRK_FILL, 0x00, NULL},
        {28, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {13, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x01, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              {2, FIELD_FILL, 0x00, OP_SET, 15, NULL},
              {1, FIELD_FILL, 0xc2, OP_SET, 17, NULL},
              // Upper 4 bits in low 4 bits of byte 5, lower 8 bits in
              // byte 6
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 148, 4},
                    { 152, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 20, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 21, NULL},
              {1, FIELD_FILL, 0x80, OP_SET, 22, NULL},
              {1, FIELD_FILL_LAST_SECTOR, 0x10, OP_XOR, 22, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 23, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 24, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x00, NULL},
        {519, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x01, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0xc9, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 3, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 515, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {28, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {57, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_Xebec_104527_C0_256B[]
#ifdef DEF_DATA
 =
{ { 20, TRK_FILL, 0x00, NULL },
  { 32, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {8, TRK_FILL, 0x00, NULL},
        {28, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {13, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x01, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              {2, FIELD_FILL, 0x00, OP_SET, 15, NULL},
              {1, FIELD_FILL, 0xc2, OP_SET, 17, NULL},
              // Upper 4 bits in low 4 bits of byte 5, lower 8 bits in
              // byte 6
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 148, 4},
                    { 152, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 20, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 21, NULL},
              {1, FIELD_FILL, 0x80, OP_SET, 22, NULL},
              {1, FIELD_FILL_LAST_SECTOR, 0x10, OP_XOR, 22, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 23, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 24, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x00, NULL},
        {263, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x01, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 2, NULL},
              {256, FIELD_SECTOR_DATA, 0x00, OP_SET, 3, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 259, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {6, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {94, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

DEF_EXTERN TRK_L trk_EC1841[]
#ifdef DEF_DATA
 =
{ { 30, TRK_FILL, 0x00, NULL },
  { 17, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {17, TRK_FILL, 0x00, NULL},
        {28, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_42, 0x42, OP_SET, 1, NULL},
              {1, FIELD_85, 0x85, OP_SET, 2, NULL},
              {1, FIELD_0A, 0x0a, OP_SET, 3, NULL},
              // This doesn't exactly match real pattern but real pattern
              // doesn't allign the 0x01 start with our pattern
              {1, FIELD_10, 0x10, OP_SET, 4, NULL},
              {9, FIELD_FILL, 0x00, OP_SET, 5, NULL},
              {1, FIELD_FILL, 0x01, OP_SET, 14, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 15, NULL},
              {2, FIELD_FILL, 0x00, OP_SET, 15, NULL},
              {1, FIELD_FILL, 0xc2, OP_SET, 17, NULL},
              // Upper 4 bits in low 4 bits of byte 5, lower 8 bits in
              // byte 6
              {0, FIELD_CYL, 0x00, OP_XOR, 12,
                 (BIT_L []) {
                    { 148, 4},
                    { 152, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 20, NULL},
              // Don't support alternate tracks
              {1, FIELD_SECTOR, 0x00, OP_SET, 21, NULL},
              {1, FIELD_FILL, 0x80, OP_SET, 22, NULL},
              {1, FIELD_FILL_LAST_SECTOR, 0x10, OP_XOR, 22, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 23, NULL},
              {4, FIELD_HDR_CRC, 0x00, OP_SET, 24, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {17, TRK_FILL, 0x00, NULL},
        {519, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x01, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 1, NULL},
              {1, FIELD_FILL, 0x00, OP_SET, 2, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 3, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 515, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {24, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {103, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;

// 3B2 MFM Disks. Controller is a NEC uPD7261AD,
// configured for soft sectors and CRCs.
//
// DTLH = 0xF2 (CRC polynomial inits with all 1's, 0x4E for ID/DATA pads)
// CRC polynomial: (x^16 + x^12 + x^5 + 1)
// GPL1 = 16
// GPL2 = 13
// GPL3 = 15

DEF_EXTERN TRK_L trk_att_3b2[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL},        // GPL 1
  { 18, TRK_SUB, 0x00,
    (TRK_L [])
    {
        {13, TRK_FILL, 0x00, NULL },  // PLO SYNC (GPL 2)
        {7, TRK_FIELD, 0x00,
          (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              // Upper byte of cylinder is xored with 0xff
              // Lower byte is unmodified
              {1, FIELD_FILL, 0xff, OP_SET, 1, NULL},
              {0, FIELD_CYL, 0x00, OP_XOR, 16,
                 (BIT_L []) {
                    { 8, 16},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
          },
        },
	{3, TRK_FILL, 0x4e, NULL},    // ID PAD
        {13, TRK_FILL, 0x00, NULL},   // PLO SYNC (GPL 2)
        {516, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {18, TRK_FILL, 0x4e, NULL},   // Data pad (3) + Inter-Record gap (GPL3)
        {-1, 0, 0, NULL},
      }
  },
  {142, TRK_FILL, 0x4e, NULL},
  {-1, 0, 0, NULL},
}
#endif
;
DEF_EXTERN TRK_L trk_att_3b2_17sector[]
#ifdef DEF_DATA
 =
{ { 16, TRK_FILL, 0x4e, NULL},        // GPL 1
  { 17, TRK_SUB, 0x00,
    (TRK_L [])
    {
        {13, TRK_FILL, 0x00, NULL },  // PLO SYNC (GPL 2)
        {7, TRK_FIELD, 0x00,
          (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              // Upper byte of cylinder is xored with 0xff
              // Lower byte is unmodified
              {1, FIELD_FILL, 0xff, OP_SET, 1, NULL},
              {0, FIELD_CYL, 0x00, OP_XOR, 16,
                 (BIT_L []) {
                    { 8, 16},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {2, FIELD_HDR_CRC, 0x00, OP_SET, 5, NULL},
              {-1, 0, 0, 0, 0, NULL}
          },
        },
	{3, TRK_FILL, 0x4e, NULL},    // ID PAD
        {13, TRK_FILL, 0x00, NULL},   // PLO SYNC (GPL 2)
        {516, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {2, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {18, TRK_FILL, 0x4e, NULL},   // Data pad (3) + Inter-Record gap (GPL3)
        {-1, 0, 0, NULL},
      }
  },
  {712, TRK_FILL, 0x4e, NULL},
  {-1, 0, 0, NULL},
}
#endif
;


// 512B 18 sectors per track from Adaptec controller manual
// http://www.bitsavers.org/pdf/adaptec/ACB-4000/400003-00A_ACB-4000A_Users_Manual_Oct85.pdf

DEF_EXTERN TRK_L trk_Adaptec_4000_18sector_512B[]
#ifdef DEF_DATA
 =
{ { 10, TRK_FILL, 0x4e, NULL }, // GAP 1
  { 18, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {12, TRK_FILL, 0x00, NULL},
        {10, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xfe, OP_SET, 1, NULL},
              // This adds upper 3 bits of cylinder to bits 4-6 of
              // byte 3 and the rest in byte 2.
              {0, FIELD_LBA, 0x00, OP_SET, 24,
                 (BIT_L []) {
                    { 16, 8}, // Byte 2 bits 6-4 gets upper 8 bits
                    { 24, 8}, // byte 3 gets middle 8 bits
                    { 32, 8}, // byte 4 gets low 8 bits
                    { -1, -1},
                 }
              },
              // This is flag byte. Should have values for disk format
              // in header for cylinder 0.
              {1, FIELD_FILL, 0x00, OP_SET, 5, NULL},

              {4, FIELD_HDR_CRC, 0x00, OP_SET, 6, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
// 3+12 Gap2
        {3, TRK_FILL, 0x00, NULL},
        {12, TRK_FILL, 0x00, NULL},
        {518, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_A1, 0xa1, OP_SET, 0, NULL},
              {1, FIELD_FILL, 0xf8, OP_SET, 1, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 2, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 514, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
// 2+9 Gap3
        {2, TRK_FILL, 0x00, NULL},
        {9, TRK_FILL, 0x4e, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {220, TRK_FILL, 0x4e, NULL}, // GAP4, 2 longer due to emulator timing
   {-1, 0, 0, NULL},
}
#endif
;

// Code currently can't generate same physical sector ordering.
// Disk sample has 16 17 0 1 ... This will generate 0 1 2 ...
DEF_EXTERN TRK_L trk_IMS_A820[]
#ifdef DEF_DATA
 =
{ { 18, TRK_SUB, 0x00,
     (TRK_L [])
     {
        {27, TRK_FILL, 0x00, NULL},
        {521, TRK_FIELD, 0x00,
           (FIELD_L []) {
              {1, FIELD_FILL, 0x01, OP_SET, 0, NULL},
              {0, FIELD_MARK_CRC_START, 0, OP_SET, 1, NULL},
              {0, FIELD_CYL, 0x00, OP_SET, 16,
                 (BIT_L []) {
                    { 16, 8}, // High byte get written to low after reverse
                    { 8, 8},
                    { -1, -1},
                 }
              },
              {1, FIELD_HEAD, 0x00, OP_SET, 3, NULL},
              {1, FIELD_SECTOR, 0x00, OP_SET, 4, NULL},
              {512, FIELD_SECTOR_DATA, 0x00, OP_SET, 5, NULL},
              {4, FIELD_DATA_CRC, 0x00, OP_SET, 517, NULL},
              {0, FIELD_NEXT_SECTOR, 0x00, OP_SET, 0, NULL},
              {-1, 0, 0, 0, 0, NULL}
           }
        },
        {30, TRK_FILL, 0x00, NULL},
        {-1, 0, 0, NULL},
     }
   },
   {14, TRK_FILL, 0x00, NULL},
   {-1, 0, 0, NULL},
}
#endif
;


#endif // TRACK_DEFS_H__
