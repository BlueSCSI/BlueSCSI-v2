/** 
 * SCSI2SD V6 - Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * 
 * This file is licensed under the GPL version 3 or any later version.  
 * It is derived from time.h in SCSI2SD V6.
 *  
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// Timing functions for SCSI2SD.
// This file is derived from time.h in SCSI2SD-V6.

#pragma once

#include <stdint.h>
#include "ZuluSCSI_platform.h"

#define s2s_getTime_ms() millis()
#define s2s_elapsedTime_ms(since) ((uint32_t)(millis() - (since)))
#define s2s_delay_ms(x) delay_ns(x * 1000000)
#define s2s_delay_us(x) delay_ns(x * 1000)
#define s2s_delay_ns(x) delay_ns(x)

