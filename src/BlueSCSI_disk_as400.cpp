/**
 * Copyright (C) 2025-2026 Kevin Moonlight <me@yyzkevin.com>
 *
 * This file is part of BlueSCSI
 *
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

// AS/400 SCSI disk command extensions - bit manipulation helpers
// These are pure functions used by the skip read/write implementation
// in BlueSCSI_disk.cpp.

#include "BlueSCSI_disk_as400.h"
#include "BlueSCSI_platform.h"

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SdFat.h>

extern SdFs SD;

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

int as400_skip_total_true_bits(const uint8_t *mask, size_t masklen)
{
    int total = 0;
    for (size_t i = 0; i < masklen; i++)
    {
        uint8_t val = mask[i];
        while (val)
        {
            val &= (val - 1);
            total++;
        }
    }
    return total;
}

int as400_skip_contiguous_bits(const uint8_t *data, size_t byte_len, size_t bit_start)
{
    size_t total_bits = byte_len * 8;
    if (bit_start >= total_bits) return 0;

    size_t byte_index = bit_start / 8;
    int bit_offset = 7 - (bit_start % 8); // MSB-first
    int target_bit = (data[byte_index] >> bit_offset) & 1;

    size_t count = 0;
    for (size_t i = bit_start; i < total_bits; i++)
    {
        byte_index = i / 8;
        bit_offset = 7 - (i % 8);
        int current_bit = (data[byte_index] >> bit_offset) & 1;

        if (current_bit != target_bit) break;
        count++;
    }

    return target_bit ? (int)count : -(int)count;
}

// MODE SENSE all-pages response captured from a real IBM AS/400 drive
const uint8_t as400_mode_sense_all_pages[] =
{
    0x8b,0x00,0x10,0x08,0x00,0x3a,0xb4,0xa2,0x00,0x00,0x02,0x08,
    0x81,0x0a,0x04,0x01,0x30,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x82,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x19,
    0x00,0x00,0x00,0x00,0x03,0x16,0x00,0x0f,0x00,0x22,0x00,0x00,
    0x00,0x00,0x00,0x5c,0x02,0x08,0x00,0x01,0x00,0x08,0x00,0x11,
    0x40,0x00,0x00,0x00,0x84,0x16,0x00,0x0b,0x29,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0xfd,0x00,0x00,0x00,
    0x15,0x18,0x00,0x00,0x87,0x0a,0x04,0x01,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x88,0x12,0x00,0x01,0xff,0xff,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,
    0x8a,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x0a,0x80,0x80,
    0x08,0x30,0x00,0x00,0x00,0x00,0x00,0x00
};

const size_t as400_mode_sense_all_pages_len = sizeof(as400_mode_sense_all_pages);

// Default standard inquiry data from a real IBM AS/400 drive (164 bytes)
const uint8_t as400_default_inquiry[] =
{
    0x00, 0x00, 0x03, 0x02, 0x9f, 0x00, 0x01, 0x3A,  'I',  'B',  'M',  'A',  'S',  '4',  '0',  '0',
     'D',  'G',  'V',  'S',  '0',  '9',  'U',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
     '0',  '2',  'A',  '1',  '0',  '0',  '0',  '6',  '7',  'A',  'C',  'E',  '7',  '5',  'M',  'S',
     'P',  'A',  '4',  '1',  'A',  '1',  ' ',  ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,  '0',  '0',  '6',  '8',  '0',  '0',  '0',  '5',  '6',  '3',  '0',  '0',  '0',  '1',
     '2',  '2',  '0',  '9',  'L',  '4',  '0',  '4',  '4',  ' ',  ' ',  ' ',  ' ',  ' ',  'F',  '2',
     '4',  '4',  '6',  '0',  ' ',  ' ',  ' ',  ' ',  'Q',  '3',  '4',  'L',  '5',  '4',  '3',  '8',
     'M',  '0',  '3',  ' ',  'F',  '2',  '4',  '4',  '5',  '3',  ' ',  ' ',  ' ',  ' ',  '2',  '0',
     '0',  '0', 0x00, 0x00
};

const size_t as400_default_inquiry_len = ARRAY_LEN(as400_default_inquiry);

// Default VPD pages: first byte is total length, remaining bytes are page data
const uint8_t as400_default_vpd_pages[][255] =
{
    // Page 0x00 - Supported Vital Product Data pages
    {
        12, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x03, 0x80, 0x82, 0x83, 0xD1, 0xD2
    },
    // Page 0x01 - ASCII Information
    {
        51, 0x00, 0x01, 0x00, 0x2f, 0x18,  '0',  '9',  'L',  '4',  '0',
             '4',  '4',  ' ',  ' ',  ' ',  ' ',  ' ', 0x00,  'F',  '2',
             '4',  '4',  '6',  '0',  ' ',  ' ',  ' ',  ' ', 0x00, 0xf0,
            0xf9, 0xd3, 0xf4, 0xF0, 0xF4, 0xF4, 0x40, 0x40, 0x40, 0x40,
            0x40, 0xC6, 0xF2, 0xF4, 0xF4, 0xF6, 0xF0, 0x40, 0x40, 0x40,
            0x40
    },
    // Page 0x03 - ASCII Operating Definition
    {
        40, 0x00, 0x03, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x90,
            0x06, 0x60, 0x00, 0x01, 0x02, 0xA1, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x4D, 0x53, 0x50, 0x41, 0x34, 0x32,
            0x41, 0x31, 0x20, 0x20, 0x20, 0x20, 0x37, 0x33, 0x32, 0x39
    },
    // Page 0x80 - Unit Serial Number
    {
        20, 0x00, 0x80, 0x00, 0x10,  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
             '0',  '0',  '0',  '6',  '7',  'A',  'C',  'E',  '7',  '5'
    },
    // Page 0x82 - ASCII Implemented Operating Definition
    {
        52, 0x00, 0x82, 0x00, 0x30, 0x19,  'D',  'M',  'V',  'S', 0x00,
             '0',  '9',  'D', 0x00,  '0',  '0',  '0',  '6',  '7',  'A',
             'C',  'E', 0x00,  'I',  'B',  'M', 0x20, 0x20, 0x20, 0x00,
            0xC4, 0xD4, 0xE5, 0xE2, 0xF0, 0xF9, 0xC4, 0x00, 0xF0, 0xF0,
            0xF0, 0xF6, 0xF7, 0xC1, 0xC3, 0xC5, 0xC9, 0xC2, 0xD4, 0x40,
            0x40, 0x40
    },
    // Page 0x83 - Device Identification
    {
        54, 0x00, 0x83, 0x00, 0x32, 0x02, 0x01, 0x00, 0x22,  'I',  'B',
             'M',  'A',  'S',  '4',  '0',  '0',  'D',  'M',  'V',  'S',
             ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
             ' ',  ' ',  'F',  '8',  '0',  '6',  '7',  'A',  'C',  'E',
             '7',  '9', 0x01, 0x02, 0x00, 0x08, 0x00, 0x60, 0x94, 0xC7,
            0xC5, 0x06, 0x7A, 0xCE
    },
    // Page 0xD1 - IBM Vendor (drive component info)
    {
        84, 0x00, 0xD1, 0x00, 0x50,  'I',  'B',  'M',  'R',  'W',  '0',
             '0',  '5',  '5',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
             'M',  '5',  'A',  'N',  '0',  '9',  'L',  '7',  '8',  ' ',
             ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  'X',  'S',  'A',  'B',
             '0',  '4',  'X',  'G',  'U',  ' ',  ' ',  ' ',  ' ',  ' ',
             ' ',  ' ',  'A',  'L',  'E',  'D',  '0',  '5',  'K',  'E',
             'L',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  'F',  '8',
             '0',  '6',  '7',  'A',  'C',  'E',  '7',  '5',  ' ',  ' ',
             ' ',  ' ',  ' ',  ' ',
    },
    // Page 0xD2 - IBM Vendor (engineering change info)
    {
        36, 0x00, 0xD2, 0x00, 0x20,  'E',  'C',  'L',  'M',  '0',  '2',
             'G',  '0',  '9',  '7',  '6',  ' ',  ' ',  ' ',  ' ',  ' ',
             'Q',  '3',  '4',  'L',  '5',  '4',  '3',  '8',  'M',  '0',
             '3',  ' ',  ' ',  ' ',  ' ',  ' '
    }
};

const size_t as400_default_vpd_page_count = ARRAY_LEN(as400_default_vpd_pages);

extern "C" void as400_get_serial_8(uint8_t scsi_id, uint8_t *serial_buf)
{
    cid_t sd_cid;
    uint32_t sd_sn = 0;
    if (SD.card()->readCID(&sd_cid))
    {
        sd_sn = sd_cid.psn();
    }
    const char hex[] = "0123456789ABCDEF";
    serial_buf[0] = hex[(sd_sn >> 28) & 0xF];
    serial_buf[1] = hex[(sd_sn >> 24) & 0xF];
    serial_buf[2] = hex[(sd_sn >> 20) & 0xF];
    serial_buf[3] = hex[(sd_sn >> 16) & 0xF];
    serial_buf[4] = hex[(sd_sn >> 12) & 0xF];
    serial_buf[5] = hex[((sd_sn >>  8) ^ scsi_id) & 0xF];
    // Real IBM drives always have 75 as the last two digits
    serial_buf[6] = '7';
    serial_buf[7] = '5';
}

#undef ARRAY_LEN

#endif // BLUESCSI_ULTRA || BLUESCSI_ULTRA_WIDE
