/**
* ZuluSCSI™ - Copyright (c) 2024 Rabbit Hole Computing™
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/
#include "timings.h"
#include "BlueSCSI_platform.h"
#if defined(ARCH_RP2350)
uint8_t g_max_sync_20_period = 18;
uint8_t g_max_sync_10_period = 25;
uint8_t g_max_sync_5_period  = 50;
#elif defined(ARCH_RP2040)
uint8_t g_max_sync_20_period = 25;
uint8_t g_max_sync_10_period = 25;
uint8_t g_max_sync_5_period  = 50;
#endif
uint8_t g_force_sync = 0;
uint8_t g_force_offset = 15;