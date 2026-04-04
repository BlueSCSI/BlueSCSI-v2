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

// AS/400 SCSI disk command extensions
// Implements Write Same(10), Skip Read(10), and Skip Write(10)
// These are vendor-specific commands used by IBM AS/400 (iSeries) systems.

#ifndef BLUESCSI_DISK_AS400_H
#define BLUESCSI_DISK_AS400_H

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)

#include <stdint.h>
#include <stdlib.h>

// Count the number of set bits (1-bits) in the skip mask
int as400_skip_total_true_bits(const uint8_t *mask, size_t masklen);

// Find the length of the next contiguous run of same-value bits starting at bit_start.
// Returns positive for 1-runs (sectors to transfer), negative for 0-runs (sectors to skip).
// Bits are numbered MSB-first within each byte.
int as400_skip_contiguous_bits(const uint8_t *data, size_t byte_len, size_t bit_start);

// MODE SENSE all-pages response captured from a real IBM AS/400 drive
extern const uint8_t as400_mode_sense_all_pages[];
extern const size_t as400_mode_sense_all_pages_len;

// LOG SENSE page data
extern const uint8_t as400_log_sense_page_00[];
extern const size_t as400_log_sense_page_00_len;
extern const uint16_t as400_log_sense_page_30_page_length;
extern const uint8_t  as400_log_sense_page_30_page_list_length;
extern const uint16_t as400_log_sense_page_31_page_length;

// I/O counters (currently always zero, could be wired up)
extern uint32_t as400_read_ops;
extern uint32_t as400_write_ops;

// Default inquiry data from a real IBM AS/400 drive
extern const uint8_t as400_default_inquiry[];
extern const size_t as400_default_inquiry_len;

// Default VPD pages: each row is [length, data...]
extern const uint8_t as400_default_vpd_pages[][255];
extern const size_t as400_default_vpd_page_count;

#ifdef __cplusplus
extern "C" {
#endif
// Generate an 8-byte serial number from the MCU unique ID and SCSI ID
void as400_get_serial_8(uint8_t scsi_id, uint8_t *serial_buf);
#ifdef __cplusplus
}
#endif

#endif // BLUESCSI_ULTRA || BLUESCSI_ULTRA_WIDE
#endif // BLUESCSI_DISK_AS400_H
