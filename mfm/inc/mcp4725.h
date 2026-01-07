#ifndef __MCP4725_H__
#define __MCP4725_H__

//
// mcp4725.h
//
//  Created on: Aug 6, 2025
//      Author: BBMD (contains original code from DJG)
//
// 08/06/25 BBMD Imported header to support external stepper control from
//    DJG's alternate build (currently untested!)
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

#define MCP4725_PD_NONE 0
#define MCP4725_PD_1k 1
#define MCP4725_PD_100k 2
#define MCP4725_PD_500k 3

void mcp4725_open();
void mcp4725_close();
void mcp4725_set_dac(int value, int powerdown, int set_eeprom);
void mcp4725_get_status(int *value, int *eeprom_value, int *pd, int *busy);
void mcp4725_disable_dac(int diable);

#endif
