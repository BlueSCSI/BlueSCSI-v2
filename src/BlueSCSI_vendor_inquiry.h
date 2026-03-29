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

// Custom SCSI inquiry data (VPD/SPD) from INI configuration
// Used by AS/400 systems that require specific inquiry responses.

#ifndef BLUESCSI_VENDOR_INQUIRY_H
#define BLUESCSI_VENDOR_INQUIRY_H

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parse custom inquiry data from bluescsi.ini for all SCSI IDs.
// Called once during initialization.
// INI format: [SCSI<id>] vpd00=XX XX XX, spd=XX XX XX (hex values)
void parseCustomInquiryData(void);

// Check if custom VPD (Vital Product Data) exists for a given SCSI ID and page code.
// If found, copies data into buf and sets *length. Returns true if custom data exists.
bool getCustomVPD(uint8_t scsiId, uint8_t pageCode, uint8_t *buf, uint8_t *length);

// Check if custom SPD (Standard Page Data / standard inquiry override) exists for a SCSI ID.
// If found, copies data into buf and sets *length. Returns true if custom data exists.
bool getCustomSPD(uint8_t scsiId, uint8_t *buf, uint16_t *length);

#ifdef __cplusplus
}
#endif

#endif // BLUESCSI_ULTRA || BLUESCSI_ULTRA_WIDE
#endif // BLUESCSI_VENDOR_INQUIRY_H
