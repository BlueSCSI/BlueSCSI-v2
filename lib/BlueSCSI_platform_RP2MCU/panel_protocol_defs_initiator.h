/**
 * BlueSCSI - Copyright (c) 2026 Eric Helgeson
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
 */

#pragma once

#include "panel_protocol_defs.h"
#include <stddef.h>   // offsetof

// Get initiator mode status (async, returns initiator_status_response_t).
// The shared header reserves command 0x59 for this BlueSCSI-only command.
#define PANEL_CMD_GET_INITIATOR_STATUS 0x59

// Operating mode, reported in the device list response. The shared
// device_list_response_t exposes this byte as reserved[0]; 0x00 (target) is the
// default and is backward-compatible with firmware that leaves it zeroed.
#define PANEL_MODE_TARGET      0x00
#define PANEL_MODE_INITIATOR   0x01
#define PANEL_DEVLIST_MODE(list) ((list)->reserved[0])

static_assert(offsetof(device_list_response_t, reserved) == 2,
              "device_list mode byte offset drifted from the shared header");

// Initiator mode phase codes
#define PANEL_INITIATOR_PHASE_IDLE       0x00
#define PANEL_INITIATOR_PHASE_SCANNING   0x01
#define PANEL_INITIATOR_PHASE_IMAGING    0x02
#define PANEL_INITIATOR_PHASE_COMPLETE   0x03
#define PANEL_INITIATOR_PHASE_ERROR      0x04

// Initiator target status codes
#define PANEL_INITIATOR_TARGET_NOT_FOUND 0x00
#define PANEL_INITIATOR_TARGET_FOUND     0x01
#define PANEL_INITIATOR_TARGET_IMAGING   0x02
#define PANEL_INITIATOR_TARGET_DONE      0x03
#define PANEL_INITIATOR_TARGET_ERROR     0x04

// Per-target info reported during initiator mode (50 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  scsi_id;
    uint8_t  device_type;        // 0=HD, 5=CD, 7=MO
    uint8_t  ansi_version;
    uint8_t  status;             // PANEL_INITIATOR_TARGET_*
    uint32_t sectorcount;
    uint32_t sectorsize;
    uint32_t sectors_done;
    uint32_t bad_sector_count;
    char     vendor[9];          // INQUIRY vendor (null-terminated)
    char     product[17];        // INQUIRY product (null-terminated)
    uint8_t  sense_key;
    uint8_t  asc;
    uint8_t  ascq;
    uint8_t  reserved;
} initiator_target_info_t;       // 50 bytes

// Initiator status response (variable length: header + targets[])
typedef struct __attribute__((packed)) {
    uint8_t  phase;              // PANEL_INITIATOR_PHASE_*
    uint8_t  current_target_id;  // 0-7, or 0xFF if none
    uint8_t  initiator_id;
    uint8_t  targets_found;
    uint8_t  targets_imaged;
    uint8_t  drives_imaged_mask; // bitmask of IDs that have been imaged
    uint8_t  reserved[2];
    initiator_target_info_t targets[];  // variable-length array
} initiator_status_response_t;
