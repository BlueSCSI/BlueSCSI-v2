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

// Shared verbatim with the front panel firmware at
// open-retro-storage-frontpanel/sw-frontpanel/main/panel_protocol_defs_initiator.h
// Keep the two byte-identical; the panel compiles this as C11.

#include "panel_protocol_defs.h"
#include <stddef.h>   // offsetof
#include <assert.h>   // static_assert, when compiled as C11

// Get initiator mode status (async, returns initiator_status_response_t).
// The shared header reserves command 0x59 for this BlueSCSI-only command.
#define PANEL_CMD_GET_INITIATOR_STATUS 0x59

// Operating mode, reported in the device list response. The shared
// device_list_response_t exposes this byte as reserved[0]; 0x00 (target) is the
// default and is backward-compatible with firmware that leaves it zeroed.
#define PANEL_MODE_TARGET      0x00
#define PANEL_MODE_INITIATOR   0x01
#define PANEL_DEVLIST_MODE(list) ((list)->reserved[0])

// Answer to PANEL_CMD_GET_INITIATOR_SUMMARY: everything the imaging screen
// draws on every frame, and nothing else.
//
// This is a synchronous read served from the main board's ISR. That is the
// whole point of it existing alongside PANEL_CMD_GET_INITIATOR_STATUS: the
// latter is async, so the main loop completes it, and a board that is imaging
// is busy driving the SCSI bus and does not get there. The per-target detail
// is worth waiting for and tolerates being stale; the progress of the run is
// not and does not.
typedef struct __attribute__((packed)) {
    uint8_t  alive_magic;      // PANEL_ALIVE_MAGIC, as in the playback status
    uint8_t  protocol_version; // PANEL_PROTOCOL_VERSION
    uint8_t  operating_mode;   // PANEL_MODE_*, so the panel can see a mode change
                               // without going back to the playback status
    uint8_t  phase;            // PANEL_INITIATOR_PHASE_*
    uint8_t  current_target;   // SCSI ID being worked on, 0xFF when none
    uint8_t  progress;         // 0-100, percent of the current target
    uint8_t  targets_found;
    uint8_t  targets_imaged;
    uint16_t speed_kbps;
} panel_initiator_summary_t;

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

// Why a target was skipped (initiator_target_info_t.skip_reason). Only
// meaningful when status is PANEL_INITIATOR_TARGET_ERROR.
#define PANEL_INITIATOR_SKIP_NONE            0x00
#define PANEL_INITIATOR_SKIP_TOO_LARGE_FAT32 0x01  // >= 4 GiB, card is not exFAT
#define PANEL_INITIATOR_SKIP_UNSUPPORTED     0x02  // not a block device
#define PANEL_INITIATOR_SKIP_FILE_EXISTS     0x03  // InitiatorImageHandling = skip
#define PANEL_INITIATOR_SKIP_TOO_MANY        0x04  // ran out of -NNN suffixes
#define PANEL_INITIATOR_SKIP_NO_SPACE        0x05  // SD card full

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
    uint8_t  skip_reason;        // PANEL_INITIATOR_SKIP_*
} initiator_target_info_t;       // 50 bytes

// Initiator status response (variable length: 42 byte header + targets[]).
// current_filename and speed_kbps describe the target being imaged now, so
// they live in the header rather than being repeated for all eight targets.
typedef struct __attribute__((packed)) {
    uint8_t  phase;              // PANEL_INITIATOR_PHASE_*
    uint8_t  current_target_id;  // 0-7, or 0xFF if none
    uint8_t  initiator_id;
    uint8_t  targets_found;
    uint8_t  targets_imaged;
    uint8_t  drives_imaged_mask; // bitmask of IDs that have been imaged
    uint16_t speed_kbps;         // last measured read speed, 0 when not imaging
    char     current_filename[32]; // image being written, empty when not imaging
    uint8_t  reserved[2];
    initiator_target_info_t targets[];  // variable-length array
} initiator_status_response_t;

static_assert(sizeof(initiator_status_response_t) == 42,
              "initiator status header size drifted from the panel's copy");
static_assert(sizeof(initiator_target_info_t) == 50,
              "initiator target info size drifted from the panel's copy");
