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

#include "usbd_msc_mem.h"

// private constants/enums
#define SD_SECTOR_SIZE 512

/* USB Mass storage Standard Inquiry Data */
const uint8_t STORAGE_InquiryData[] = {
    /* LUN 0 */
    0x00,
    0x80,
    0x00,
    0x01,
    (USBD_STD_INQUIRY_LENGTH - 5U),
    0x00,
    0x00,
    0x00,
    'Z', 'u', 'l', 'u', 'S', 'C', 'S', 'I', /* Manufacturer : 8 bytes */
    'G', 'D', '3', '2', 0, 0, 0, 0, /* Product      : 16 Bytes */
    0, 0, 0, 0, 0, 0, 0, 0,
    '1', '.', '0', '0',                     /* Version      : 4 Bytes */
};

/* return true if USB presence detected / eligble to enter CR mode */
bool platform_senseMSC();

/* perform MSC-specific init tasks */
void platform_enterMSC();

/* return true if we should remain in card reader mode. called in a loop. */
bool platform_runMSC();

/* perform any cleanup tasks for the MSC-specific functionality */
void platform_exitMSC();

#endif