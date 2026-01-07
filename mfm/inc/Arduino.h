#ifndef ARDUINO_H_
#define ARDUINO_H_

//
// Arduino.h
//
//  Created on: Aug 6, 2025
//      Author: BBMD (contains original code from DJG and others)
//
// 08/06/2025 BBMD Imported header to support external stepper control from
//    DJG's alternate build
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

#define OUTPUT 1
#define INPUT 2

#define LOW 0
#define HIGH 1

void pinMode(int pin, int type);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int bitRead(int value, int bit);

void delayMicroseconds(int microSec);
void delay(int miliSec);

#endif // ARDUINO_H__
