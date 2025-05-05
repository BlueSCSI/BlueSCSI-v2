/**
 * Copyright (c) 2023-2024 zigzagjoe
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
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

#ifdef PLATFORM_MASS_STORAGE
#pragma once

// include platform-specific defines
#include "BlueSCSI_platform_msc.h"

enum  MSC_LEDState { LED_SOLIDON = 0, LED_BLINK_FAST, LED_BLINK_SLOW };
extern volatile enum MSC_LEDState MSC_LEDMode;

// run cardreader main loop (blocking)
void bluescsi_msc_loop();

#endif