// This module is routines for reading the disk drive.
//
// This is for directly controlling the head stepper with a TC78H670FTG
// bipolar stepper controller.
// https://www.sparkfun.com/products/16836
//
// The read circuit for drives I have been working with still works when the
// drive is flashing error codes of step errors on powerup.
// The stepper was frozen. With oiling the outer bearing and poking at the
// track 0 interupter the stepper was freed but failed initial step test.
// One drive I readjusted the track 0 interupter to get it to initialize but
// heads were out of position by a couple tracks with some heads having high
// number of unreadable sectors.
//
// This code ignores ready from the drive and does not try to use the normal
// step function.
//
// TODO: The disable of ready detection in the prucode breaks the DAC mode.
//
// The Miniscribe 3650 was used in this testing.
// It appears to use half step by using dual phase and single phase alternating
// to drive the stepper to get the 400 steps/revolution marked on the motor.
// The motor is 12V .5A. To operate properly I used
// #define CYL_STEPS 16
// #define STEP_RESOLUTION PRODRIVER_STEP_RESOLUTION_FIXED_1_32;
//
// Second read required --retries 200,0 to get to good data from head stop.
//
// This code assumes the heads will be at or before the first cylinder.
//
// Wiring
// J7-3  EN            All models (gpio2[01]:65:P8_18)
// J7-7  STBY          All models (gpio2[04]:68:P8_10)
// J7-5  ERR           All models (gpio2[05]:69:P8_09)
// J7-6  MODE0         All models (gpio2[23]:87:P8_29)
// J7-11 MODE1         All models (gpio2[03]:67:P8_08)
// J7-4  MODE2         All models (gpio2[22]:86:P8_27)
// J7-9  MODE3         All models (gpio2[02]:66:P8_07)
//
// Seagates ST-251 and others use 5 phase stepper. For driving them I used
// Autonics MD5-HD14 I got cheap used.
// Wiring
// J7-8 - CW
// J7-12 - CCW
// I used an NPN transistor to buffer with collector to 5V, emitter driving
//    Autonics and base connected to pins above with 3.6k resistor. Input is
//    optoisolator and they say needs minimum of 4V drive.
// Set switches
// TEST - OFF
// 1/2 CLK - OFF (Separate CW, CCW step pulses)
// C/D - OFF (Reduced motor current when stopped)
// RUN - Appropriate for motor. I used 1
// STOP - Appropriate for motor. I used 0
// MS1 - A - 50 microstep per step
// MS2 - A - 50 microstep per step
// Drive must be half stepping already since it only takes 25 steps to move a
//   cylinder
//
// Stepper connector had 10 pins. I jumpered and connected to Autonics stepper input
// 1-6, Black
// 2-7, Green
// 3-8, Orange
// 4-9, Red
// 5-10, Blue
// See ST-251 below also and
// http://www.pdp8online.com/mfm/microstep/index.shtml
// So far only works with --ignore_seek_error flag and
// READ_ALL_STEPS set. It varies with drives how bad but it seems to see
// multiple cylinders always. Suspect their is something wrong on how I have
// the stepper motor connected but since the one method works not worth more
// time troubleshooting at this time.

// TE Connectivity housing 102387-4 with contacts 6-87756-6 can be used for J7.

// I also wired up a 74LVC00 to be a 5V to 3.3V level shifter and hooked that
// to J7-11 to sense the track 0 sensor. Ended up not using track0 input so
// you don't need to hook it up.

// NOTE: As of the RLL update, this pin has been reallocated
// for other core microstepping processes (see above).

// The Miniscribe 3650 is doing something strange with the sensor since none
// of the pins changed between covered and uncovered so this signal is not
// currently used. Sensor works when the drive is using to recalibrate back to
// track 0.  Input can be read with
// #define TRACK0_PIN (2*32+3)
// pinMode(TRACK0_PIN, INPUT); // input without pullup
//   printf("%d\n",digitalRead(TRACK0_PIN));
//
// To move the head you can use --step # or -S #. Head will be moved by #
// cylinders. Positive is towards higher cylinder.


// drive_setup sets up the drive for reading.
// drive_read_disk reads the disk drive.
// drive_read_track read a track of deltas from a drive. It steps the
//    head if necessary.
//
// The drive must be at track 0 on startup or drive_seek_track0 called.
// 01/12/26 BBMD Initial import to main code branch to support
//   microstepping recovery in newer builds/RLL formats; also added changes
//   made by DJG to wiring pinouts in order to make the stepper control
//   wiring universal across revisions for the Sparkfun controller, along with
//   a pinout correction for ST-225 and compatible stepper motors
// 03/09/18 DJG Added logic to not retry when requested to read more
//   cylinders or heads than analyze determined
// 11/05/16 DJG Made retry seek progression more like it was before change below
// 10/16/16 DJG Added control over seek on retry
// 10/02/16 DJG Rob Jarratt change to clean up confusing code.
//
// Copyright 2021 David Gesswein.
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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include "msg.h"
#include "crc_ecc.h"
#include "emu_tran_file.h"
#include "mfm_decoder.h"
#include "rll_decoder.h"
#include "cmd.h"
#include "cmd_write.h"
#include "deltas_read.h"
#include "pru_setup.h"
#include "drive.h"
#include "board.h"

// TODO (BBMD): Make more of these #defines configurable from the command line

// For creating emulator file you may want to set this to 1 since emulator
// file not always good if data recovered from multiple reads. For creating
// extracted data file 0 is better.
#define SINGLE_READ_GOOD 0

// 1 if you don't want the drive trying to seek to the correct track when it
// finds the wrong cylinder
#define DISABLE_OFFTRACK_SEEK 0


// Read disk using all microsteps and multiple reads. This is for a ST-251 disk
// which always returned data from multiple cylinders like platters were
// not concentric with heads. Or could be my microstepper was causing problems.
// This recovered my good disk and decided not to mess with my good disk to
// diagnose further.
// Value of define is 0=off or number which is number of reads to do for each
// track.  Retries not used
// Use with --ignore_seek_errors
#define READ_ALL_STEPS 0

//#define TANDON_TM503
#define MINISCRIBE_3650
//#define SEAGATE_ST251

// Also for Seagate ST-225
#ifdef MINISCRIBE_3650
// Stepper wiring 3650
// A+ Yellow
// A- White
// B+ Red
// B- Blue
//
// Stepper wiring ST-225/ST-238R
// A+ Yellow
// A- White
// B+ Blue
// B- Red
//
// This is number of microsteps to move one cylinder
#define CYL_STEPS 16
// Half microstep since drive was already driving it in half step mode so
// with this drive one full step moves two cylinders
#define STEP_RESOLUTION PRODRIVER_STEP_RESOLUTION_FIXED_1_32;
#define MAX_STEP_OFFSET (5*CYL_STEPS)
// Should be >= 10 ms per full step
#define DELAY 0.2
#define CONTROL_MODE PRODRIVER_MODE_CLOCKIN
#endif

#ifdef TANDON_TM503
// Stepper wiring Tandon TM503
// A+ Blue
// A- Red
// B+ White
// B- Yellow
// Stepper wiring
// A+ Yellow
// A- White
// B+ Red
// B- Blue
//
// This is number of microsteps to move one cylinder
#define CYL_STEPS 32
// Half microstep since drive was already driving it in half step mode so
// with this drive one full step moves two cylinders
#define STEP_RESOLUTION PRODRIVER_STEP_RESOLUTION_FIXED_1_32;
#define MAX_STEP_OFFSET (5*CYL_STEPS)
// Should be >= 10 ms per full step
#define DELAY 0.2
#define CONTROL_MODE PRODRIVER_MODE_CLOCKIN
#endif

#ifdef SEAGATE_ST251
// Stepper wiring Seagate ST51
// Blue
// Red
// Orange
// Green
// Black
//
// Stepper shaft rotates counterclockwise for higher cylinder when viewed
// from bottom of drive.
//
// This is number of microsteps to move one cylinder
#define CYL_STEPS 25
// Half microstep since drive was already driving it in half step mode so
// with this drive one full step moves two cylinders
#define STEP_RESOLUTION PRODRIVER_STEP_RESOLUTION_FIXED_1_32;
#define MAX_STEP_OFFSET (5*CYL_STEPS)
// Should be >= 10 ms per full step
#define DELAY 0.2
#define CONTROL_MODE PRODRIVER_MODE_AUTONICS
#endif


int ext_stepper_update_offset(int step, int cur_offset) {
   cur_offset += step;
   if (cur_offset > MAX_STEP_OFFSET) {
      cur_offset = MAX_STEP_OFFSET;
   } else if (cur_offset < -MAX_STEP_OFFSET) {
      cur_offset = -MAX_STEP_OFFSET;
   }
   return cur_offset;
}

int ext_stepper_step_head(int new_offset, int cur_offset) {
   msg(MSG_INFO, "Step head Stepping by %d %d %d\n",new_offset - cur_offset, new_offset, cur_offset);
   if (new_offset > cur_offset) {
      pd_step(new_offset - cur_offset, 1, DELAY);
   } else {
      pd_step(cur_offset - new_offset, 0, DELAY);
   }
   cur_offset = new_offset;
   return cur_offset;
}

#if READ_ALL_STEPS
void ext_stepper_drive_read_disk(DRIVE_PARAMS *drive_params, void *deltas, int max_deltas)
{
   int cyl, head;
   //SECTOR_DECODE_STATUS sector_status = 0;
   // The status of each sector
   SECTOR_STATUS sector_status_list[MAX_SECTORS];
   // Cylinder difference when a seek error occurs
   int seek_difference;

   // Settings is structure returned by prodriver so this is used by
   // pd_begin to setup chip.
   drive_params->settings->stepResolutionMode = STEP_RESOLUTION;
   drive_params->settings->controlMode = CONTROL_MODE;
   pd_begin();
   if (drive_params->step != 0) {
      printf("Moving head by %d...\n", abs(drive_params->step) * CYL_STEPS);
      pd_step(abs(drive_params->step) * CYL_STEPS, drive_params->step > 0, DELAY);
   }

   // Start up delta reader
   deltas_start_thread(drive_params);

   mfm_decode_setup(drive_params, 1);

   for (cyl = 0; cyl < drive_params->num_cyl + 10; cyl++) {
      for (int step = 0; step < CYL_STEPS; step++) {
         for (head = 0; head < drive_params->num_head; head++) {
            for (int reads = 0; reads < READ_ALL_STEPS; reads++) {
               ext_stepper_drive_read_track(drive_params, cyl, head, deltas, max_deltas, 0);
               init_sector_status_list(sector_status_list, drive_params->num_sectors);
               if (drive_params->is_rll)
               {
                  rll_decode_track(drive_params, cyl, head, deltas, &seek_difference, sector_status_list);
               } else {
                  mfm_decode_track(drive_params, cyl, head, deltas, &seek_difference, sector_status_list);
               }
            }
         }
         ext_stepper_step_head(1, 0);
      }
   }

   mfm_decode_done(drive_params);

   deltas_stop_thread();
}

#else

// Simple old * FILTER + new * (1-FILTER) filter
#define FILTER .9
// Read the disk.
//  drive params specifies the information needed to decode the drive and what
//    files should be written from the data read
//
// drive_params: Drive parameters
// deltas: Memory array containing the raw MFM delta time transition data
void ext_stepper_drive_read_disk(DRIVE_PARAMS *drive_params, void *deltas, int max_deltas)
{
   // Loop variable
   int cyl, head;
   int cntr;
   // Retry counter, counts up
   int err_cnt;
   // Read status
   SECTOR_DECODE_STATUS sector_status = 0;
   // The status of each sector
   SECTOR_STATUS sector_status_list[MAX_SECTORS];
   // We use this to summarize the list above has any errors
   int sect_err;
   // For retry we do various length seeks. This is the maximum length we will do
   // Cylinder difference when a seek error occurs
   int seek_difference;

   // Timing stuff for testing
#ifdef CLOCK_MONOTONIC_RAW
#define CLOCK CLOCK_MONOTONIC_RAW
#else
#define CLOCK CLOCK_MONOTONIC
#endif
   struct timespec tv_start;
   double start, last = 0;
   double min = 9e9, max = 0, tot = 0;
   int count = 0;
   int last_good = 0;

   // Step offset for each head
   float head_offset[MAX_HEAD];
   // zero step to lower voltage
   int head_dir[MAX_HEAD];
   // 1 if first time trying to find proper cylinder
   int head_first = 1;
   int cyl_first = 1;
   int cur_offset = 0;
   int new_offset;

   for (cntr = 0; cntr < MAX_HEAD; cntr++) {
      head_offset[cntr] = 0;
      head_dir[cntr] = 1;
   }

   // Settings is structure returned by prodriver so this is used by
   // pd_begin to setup chip.
   drive_params->settings->stepResolutionMode = STEP_RESOLUTION;
   drive_params->settings->controlMode = CONTROL_MODE;
   pd_begin();
   if (drive_params->step != 0) {
      pd_step(abs(drive_params->step) * CYL_STEPS, drive_params->step > 0, DELAY);
   }

   // Start up delta reader
   deltas_start_thread(drive_params);

   mfm_decode_setup(drive_params, 1);

   for (cyl = 0; cyl < drive_params->num_cyl; cyl++) {
      if (cyl % 5 == 0)
         msg(MSG_PROGRESS, "At cyl %d\r", cyl);
      for (head = 0; head < drive_params->num_head; head++) {
         int recovered = 0;
         int retries;

         err_cnt = 0;
         // Clear sector status here so we can see if different sectors
         // successfully read on different reads.
         init_sector_status_list(sector_status_list, drive_params->num_sectors);
         if (cyl >= drive_params->noretry_cyl ||
             head >= drive_params->noretry_head) {
            retries = 0;
         } else {
            retries = drive_params->retries;
         }
         new_offset = round(head_offset[head]);
         do {
            cur_offset = ext_stepper_step_head(new_offset, cur_offset);
            msg(MSG_INFO, "Setting offset to %f cyl %d head %d last good %d\n",(float) cur_offset/CYL_STEPS, cyl, head, last_good);

            clock_gettime(CLOCK, &tv_start);
            start = tv_start.tv_sec + tv_start.tv_nsec / 1e9;
            if (last != 0) {
               if (start-last > max)
                  max = start-last;
               if (start-last < min)
                  min = start-last;
               tot += start-last;
               count++;
            }
            last = start;

            ext_stepper_drive_read_track(drive_params, cyl, head, deltas, max_deltas, 0);

            init_sector_status_list(sector_status_list, drive_params->num_sectors);

            if (drive_params->is_rll)
            {
               // Attempt to do an RLL decode on the current track
               sector_status = rll_decode_track(drive_params, cyl, head,
                  deltas, &seek_difference, sector_status_list);
            } else {
               sector_status = mfm_decode_track(drive_params, cyl, head,
                  deltas, &seek_difference, sector_status_list);
            }

            // See if sector list shows any with errors. The sector list
            // contains the information on the best read for each sector so
            // even if the last read had errors we may have recovered all the
            // data without errors.
            sect_err = 0;
            last_good = 0;
            for (cntr = 0; cntr < drive_params->num_sectors; cntr++) {
               if (UNRECOVERED_ERROR(sector_status_list[cntr].status)) {
                  sect_err = 1;
               } else {
                  last_good++;
               }
            }
            if (!sect_err) {
               msg(MSG_INFO, "Good multiple read cyl %d head %d\n", cyl, head);
            }
            // Try to get single read without error
#if SINGLE_READ_GOOD
            if (sector_status & (SECT_BAD_HEADER | SECT_BAD_DATA)) {
               sect_err = 1;
            }
#endif
            // Looks like a seek error. Sector headers which don't have the
            // expected cylinder such as when tracks are spared can trigger
            // this also
            if ((sector_status & SECT_WRONG_CYL)) {
#if 0
               // This avoided trying to read a head when cylinder was
               // unreachable due to hitting the stop
               printf("Seek error\n");
               if (head == 5 && cyl < 7) {
                  printf("Ignoring seek err\n");
                  err_cnt = 200;
                  head_offset[5] = -7;
                  continue;
               }
#endif
#if !DISABLE_OFFTRACK_SEEK
               if (err_cnt < retries) {
                  static int first = 1;
                  int offset;

                  msg(MSG_ERR, "Retrying seek cyl %d, cyl off by %d\n", cyl,
                        seek_difference);
                  if (seek_difference > 0) {
                     offset = CYL_STEPS/4;
                     head_dir[head] = 1;
                  } else {
                     offset = -CYL_STEPS/4;
                     head_dir[head] = 0;
                  }
                  head_offset[head] = ext_stepper_update_offset(offset, head_offset[head]);
                  if (first && abs(head_offset[head]) == MAX_STEP_OFFSET) {
                     msg(MSG_FATAL, "Offset too large on first track. Try running with -S %d\n",seek_difference);
                     exit(1);
                  }
                  new_offset = ext_stepper_update_offset(offset, new_offset);
                  cur_offset = ext_stepper_step_head(new_offset, cur_offset);
               }
#endif
            }

            if (UNRECOVERED_ERROR(sector_status) && !sect_err) {
               recovered = 1;
            }
            if (sect_err) {
               if (head_dir[head]) {
                  // If trying to find first cylinder go by bigger steps
                  // until we find 1 good sector.
                  if (cyl_first && last_good == 0) {
                     new_offset = cur_offset + CYL_STEPS/4;
                  } else {
                     new_offset = cur_offset + 1;
                  }
                  // For first time trying to find proper cylinder for head
                  // allow stepping as much as needed
                  if (new_offset - head_offset[head] >  CYL_STEPS * 3/5
                     && !cyl_first) {
                     new_offset = round(head_offset[head]);
                     head_dir[head] = 0;
                  }
               } else {
                  new_offset = cur_offset - 1;
                  if (new_offset - head_offset[head] < -CYL_STEPS * 3/5) {
                     new_offset = round(head_offset[head]);
                     head_dir[head] = 1;
                  }
               }
            } else {
               // Use position of first good read as referece for rest of heads
               if (head_first) {
                  cur_offset = 0;
                  new_offset = 0;
                  head_first = 0;
               } else {
                  // If first time for head then just use offset value
                  if (cyl_first) {
                     head_offset[head] = cur_offset;
                  } else {
                     head_offset[head] = head_offset[head] * FILTER + cur_offset * (1.0 - FILTER);
                  }
               }
            }
         // repeat until we get all the data or run out of retries
         } while (sect_err && err_cnt++ < retries);
         if (err_cnt > 0) {
            if (err_cnt == retries + 1) {
               msg(MSG_ERR, "Retries failed cyl %d head %d\n", cyl, head);
            } else {
               msg(MSG_INFO, "All sectors recovered %safter %d retries cyl %d head %d\n",
                     recovered ? "from multiple reads " : "", err_cnt, cyl, head);
            }
         }
      }
      cyl_first = 0;
      ext_stepper_step_head(CYL_STEPS, 0);
   }

   mfm_decode_done(drive_params);

   msg(MSG_DEBUG, "Track read time in ms min %f max %f avg %f\n", min * 1e3,
         max * 1e3, tot * 1e3 / count);

   deltas_stop_thread();

   return;
}

#endif

// Read a track of deltas from a drive
//
// drive_params: Drive parameters
// cyl, head: Cylinder and head reading. Head is stepped to cylinder
//    and head selected
// deltas: Memory array containing the raw MFM delta time transition data
// max_deltas: Size of deltas array
int ext_stepper_drive_read_track(DRIVE_PARAMS *drive_params, int cyl, int head,
      void *deltas, int max_deltas, int return_write_fault) {

      // Analyze can change so set it every time
   pru_write_word(MEM_PRU0_DATA, PRU0_START_TIME_CLOCKS,
      drive_params->start_time_ns / CLOCKS_TO_NS);

   drive_set_head(head);

   if (pru_exec_cmd(CMD_READ_TRACK, 0) != 0) {
      drive_print_drive_status(MSG_FATAL, drive_get_drive_status());
      exit(1);
   }
   // OK to start reading deltas
   deltas_start_read(cyl, head);

   return 0;
}
