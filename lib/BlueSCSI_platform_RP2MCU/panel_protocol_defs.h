// SPDX-License-Identifier: GPL-2.0-or-later
//
//  Copyright (C) 2025-2026  Ian Scott
//  Copyright (C) 2026       Eric Helgeson
//
//  NOTE: This file alone is licensed GPL-2.0-or-later.
//
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the
//  Free Software Foundation; either version 2 of the License, or (at your
//  option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <stdint.h>

// Panel communication protocol definitions
// Shared between ESP32 front panel and RP2350 main board

// Protocol constants
#define PANEL_PROTOCOL_HEADER_SIZE     5
#define PANEL_PROTOCOL_MAX_PAYLOAD     4096

// Liveness magic: a live main board running this protocol stamps
// PANEL_ALIVE_MAGIC into panel_playback_status_t.alive_magic on every reply. A
// dead/disconnected bus instead reads back uniformly 0x00 or 0xFF, so 0xA5 is
// deliberately distinct from 0x00/0xFF
#define PANEL_ALIVE_MAGIC              0xA5

// Command bit masks
#define PANEL_CMD_DIR_WRITE           0x00
#define PANEL_CMD_DIR_READ            0x80
#define PANEL_CMD_DIR_MASK            0x80
#define PANEL_CMD_ASYNC_FLAG          0x40  // Bit 6 indicates async operation

// Write commands (bit 7 = 0, bit 6 = 1 for async)
#define PANEL_CMD_GET_DIR_ENTRY_COUNT  0x42  // Get count of entries in current directory (async)
#define PANEL_CMD_GET_ENTRY_INFO       0x43  // Get info for entry at index (async, arg: index)
#define PANEL_CMD_SELECT_ENTRY         0x44  // Select entry by index: -1 (0xFFFF)=parent dir, >=0=select entry (async, arg: signed 16-bit index)
#define PANEL_CMD_GET_CURRENT_PATH     0x45  // Get current directory path (async)
#define PANEL_CMD_GET_DEVICE_LIST      0x46  // Get list of all configured devices (async)
#define PANEL_CMD_EJECT_IMAGE          0x47  // Unload current image (async)
#define PANEL_CMD_GET_LOADED_IMAGE_STATUS 0x48  // Get status of currently loaded image (async)
#define PANEL_CMD_SELECT_PREV_IMAGE    0x49  // Load previous image in current directory (async)
#define PANEL_CMD_SELECT_NEXT_IMAGE    0x4A  // Load next image in current directory (async)
#define PANEL_CMD_SELECT_IMAGE_BY_NAME 0x4B  // Load image by filename (async, payload: null-terminated filename)
#define PANEL_CMD_CHECK_FIRMWARE       0x50  // Check for firmware update (async)
#define PANEL_CMD_START_FIRMWARE_READ  0x51  // Start firmware read (async, arg: chunk index)
#define PANEL_CMD_START_FILE_UPLOAD    0x52  // Start file upload (async, payload: file_upload_start_t)
#define PANEL_CMD_WRITE_FILE_CHUNK     0x53  // Write file chunk (async, arg: chunk crc16, payload: chunk data)
#define PANEL_CMD_FINISH_FILE_UPLOAD   0x54  // Finish file upload (async)
#define PANEL_CMD_GET_RP2350_FW_STATUS 0x55  // Get RP2350 firmware status (async, returns rp2350_fw_status_t)
#define PANEL_CMD_START_RP2350_UPDATE  0x56  // Start RP2350 firmware update from SD card (async, reboots on success)
#define PANEL_CMD_START_FILE_DOWNLOAD  0x57  // Start file download (async, payload: null-terminated filename)
#define PANEL_CMD_READ_FILE_CHUNK      0x58  // Read file chunk (async, arg: chunk index, returns chunk data)
// 0x59 reserved for PANEL_CMD_GET_INITIATOR_STATUS on the main board (not used by this panel build)
#define PANEL_CMD_DELETE_FILE          0x5A  // Delete file (async, payload: null-terminated path)
#define PANEL_CMD_RENAME_FILE          0x5B  // Rename file (async, payload: oldpath\0newpath\0)
#define PANEL_CMD_TOUCH_FILE           0x5C  // Create empty file (async, payload: null-terminated path)
#define PANEL_CMD_MKDIR                0x5D  // Create directory (async, payload: null-terminated path)
#define PANEL_CMD_RESET                0x7F  // Reset system

// Read commands (bit 7 = 1)
#define PANEL_CMD_POLL_STATUS          0x80  // Get general status (1 byte response)
#define PANEL_CMD_POLL_OP_READY        0x81  // Check if async operation ready (3 bytes: ready, size low, size high)
#define PANEL_CMD_GET_DEVICE_STATUS    0x83  // Get device status (1 byte response)
#define PANEL_CMD_GET_FIRMWARE_INFO    0x84  // Get firmware info after CHECK_FIRMWARE (45 bytes)
#define PANEL_CMD_GET_PLAYBACK_STATUS  0x85  // Get current playback status (panel_playback_status_t)
#define PANEL_CMD_GET_COMMAND_STATUS   0x86  // Get detailed async command status (panel_command_status_t)

// Status codes for POLL_STATUS
#define PANEL_STATUS_OK                0x00  // System OK
#define PANEL_STATUS_BUSY              0x01  // Operation in progress
#define PANEL_STATUS_ERROR             0x02  // Last operation failed
#define PANEL_STATUS_NO_OPERATION      0x03  // No operation pending

// Device status codes for GET_DEVICE_STATUS
#define PANEL_DEVICE_STATUS_NO_IMAGE    0x00  // No image loaded
#define PANEL_DEVICE_STATUS_LOADED      0x01  // Image loaded and ready
#define PANEL_DEVICE_STATUS_LOADING     0x02  // Image loading in progress
#define PANEL_DEVICE_STATUS_ERROR       0x03  // Image error
#define PANEL_DEVICE_STATUS_NO_CARD     0x04  // SD card not present
#define PANEL_DEVICE_STATUS_WRONG_MODE  0x05  // SD card configured for the other device type
#define PANEL_DEVICE_STATUS_TRAY_OPEN   0x06  // Optical tray open (disc ejected), awaiting load/close

// Special argument values
#define PANEL_ARG_EXTENDED             0xFFFF  // Use payload for extended data
#define PANEL_ARG_IGNORED              0x0000  // Argument not used

// Protocol header structure (5 bytes)
typedef struct __attribute__((packed)) {
    uint8_t command;       // Command code with direction bit
    uint16_t argument;     // Optional argument halfword
    uint16_t payload_size; // Size of phase 2 transfer (little-endian)
} panel_protocol_header_t;

// Status response structure for POLL_OP_READY (3 bytes)
typedef struct __attribute__((packed)) {
    uint8_t ready_flag;    // 1 if ready, 0 if not
    uint16_t response_size; // Size of result data (little-endian)
} panel_status_response_t;

// Detailed command status for GET_COMMAND_STATUS (4 bytes)
typedef struct __attribute__((packed)) {
    uint8_t command;       // Current/last async command
    uint8_t state;         // PANEL_ASYNC_* state
    uint8_t progress;      // 0-100 progress (command-specific)
    uint8_t last_result;   // Result of last completed operation
} panel_command_status_t;

// Helper macros
#define PANEL_CMD_IS_READ(cmd)  ((cmd) & PANEL_CMD_DIR_MASK)
#define PANEL_CMD_IS_WRITE(cmd) (!PANEL_CMD_IS_READ(cmd))
#define PANEL_CMD_IS_ASYNC(cmd) ((cmd) & PANEL_CMD_ASYNC_FLAG)

// Async operation states (internal to main board)
typedef enum {
    PANEL_ASYNC_IDLE = 0,
    PANEL_ASYNC_PROCESSING,
    PANEL_ASYNC_READY,
    PANEL_ASYNC_ERROR
} panel_async_state_t;

// Firmware info structure (45 bytes)
typedef struct __attribute__((packed)) {
    uint32_t size;         // Firmware size in bytes
    uint32_t version;      // Packed version number
    uint8_t sha256[32];    // SHA256 hash (32 bytes)
    uint8_t available;     // 1 if update available, 0 if not
    uint8_t reserved[8];   // Reserved for alignment
} panel_firmware_info_t;

// RP2350 firmware status structure
typedef struct __attribute__((packed)) {
    uint32_t current_version;      // Running version
    uint32_t available_version;    // Available update version (0 if none)
    uint8_t update_progress;       // Update progress (0-100), 0 if not updating
    uint8_t last_update_result;    // Result of last update attempt
} rp2350_fw_status_t;

// Firmware chunk size (must fit within PANEL_PROTOCOL_MAX_PAYLOAD)
#define PANEL_FIRMWARE_CHUNK_SIZE  PANEL_PROTOCOL_MAX_PAYLOAD

// File upload chunk size (must fit within PANEL_PROTOCOL_MAX_PAYLOAD)
#define PANEL_FILE_CHUNK_SIZE      PANEL_PROTOCOL_MAX_PAYLOAD

// File upload start structure (variable length with filename and hash)
typedef struct __attribute__((packed)) {
    uint32_t file_size;        // Total file size in bytes
    uint16_t filename_len;     // Length of filename string
    // Filename follows (null-terminated string)
} panel_file_upload_start_t;

// File upload result codes
#define PANEL_UPLOAD_OK            0x00
#define PANEL_UPLOAD_ERROR_DISK    0x01
#define PANEL_UPLOAD_ERROR_SPACE   0x02
#define PANEL_UPLOAD_ERROR_WRITE   0x03
#define PANEL_UPLOAD_ERROR_PATH    0x04

// File download start structure (5 bytes)
typedef struct __attribute__((packed)) {
    uint8_t result_code;       // PANEL_DOWNLOAD_* result code
    uint32_t file_size;        // Total file size in bytes (valid if result_code == 0)
} panel_file_download_start_result_t;

// File download result codes
#define PANEL_DOWNLOAD_OK              0x00
#define PANEL_DOWNLOAD_ERROR_NOT_FOUND 0x01
#define PANEL_DOWNLOAD_ERROR_READ      0x02

// File delete result codes (DELETE_FILE returns a single result_code byte)
#define PANEL_DELETE_OK                0x00
#define PANEL_DELETE_ERROR_NOT_FOUND   0x01
#define PANEL_DELETE_ERROR_IN_USE      0x02  // target is a currently-loaded image
#define PANEL_DELETE_ERROR_IO          0x03
#define PANEL_DELETE_ERROR_PATH        0x04  // empty path or ".." traversal

// File rename result codes (RENAME_FILE returns a single result_code byte)
#define PANEL_RENAME_OK                0x00
#define PANEL_RENAME_ERROR_NOT_FOUND   0x01
#define PANEL_RENAME_ERROR_EXISTS      0x02  // destination already exists
#define PANEL_RENAME_ERROR_IN_USE      0x03  // source is a currently-loaded image
#define PANEL_RENAME_ERROR_IO          0x04
#define PANEL_RENAME_ERROR_PATH        0x05  // empty/invalid path or ".." traversal

// File touch (create empty file) result codes (single result_code byte)
#define PANEL_TOUCH_OK                 0x00
#define PANEL_TOUCH_ERROR_EXISTS       0x01  // a file/dir with that name already exists
#define PANEL_TOUCH_ERROR_PATH         0x02  // empty/invalid path or ".." traversal
#define PANEL_TOUCH_ERROR_IO           0x03

// Make-directory result codes (single result_code byte)
#define PANEL_MKDIR_OK                 0x00
#define PANEL_MKDIR_ERROR_EXISTS       0x01  // a file/dir with that name already exists
#define PANEL_MKDIR_ERROR_PATH         0x02  // empty/invalid path or ".." traversal
#define PANEL_MKDIR_ERROR_IO           0x03

// Disc type codes for playback status
#define PANEL_DISC_TYPE_NO_DISC    0x00
#define PANEL_DISC_TYPE_DATA       0x01  // Data CD-ROM
#define PANEL_DISC_TYPE_AUDIO      0x02
#define PANEL_DISC_TYPE_MIXED      0x03
#define PANEL_DISC_TYPE_HDD        0x04  // IDE hard disk

// Audio playback status codes (matches CDRomAudioStatus)
#define PANEL_AUDIO_STATUS_DATA_ONLY          0x00
#define PANEL_AUDIO_STATUS_PLAYING            0x11
#define PANEL_AUDIO_STATUS_PAUSED             0x12
#define PANEL_AUDIO_STATUS_PLAYING_COMPLETED  0x13
#define PANEL_AUDIO_STATUS_PLAY_ERROR         0x14
#define PANEL_AUDIO_STATUS_NONE               0x15

// Playback status structure (76 bytes)
typedef struct __attribute__((packed)) {
    uint8_t disc_inserted;    // 1 if disc loaded, 0 if not
    uint8_t disc_type;        // PANEL_DISC_TYPE_*
    uint8_t is_playing;       // 1 if currently playing audio, 0 if not
    uint8_t audio_status;     // PANEL_AUDIO_STATUS_*
    uint8_t current_track;    // Current track number (1-99)
    uint8_t track_position_m; // Track position: minutes
    uint8_t track_position_s; // Track position: seconds
    uint8_t track_position_f; // Track position: frames
    char disc_name[64];       // Current disc name (null-terminated)
    uint8_t device_status;    // PANEL_DEVICE_STATUS_*
    uint8_t alive_magic;      // PANEL_ALIVE_MAGIC when written by a live main board
    uint8_t tray_open;        // 1 if optical tray open (disc ejected), awaiting load/close
    uint8_t reserved[1];      // Reserved for future use
} panel_playback_status_t;

// Entry type for directory listings
#define PANEL_ENTRY_TYPE_DIRECTORY  0x00  // Subdirectory (navigate into it)
#define PANEL_ENTRY_TYPE_FILE       0x01  // Image file (load it)

// Directory entry information structure (68 bytes)
typedef struct __attribute__((packed)) {
    char name[64];           // Filename or directory name (null-terminated)
    uint8_t entry_type;      // PANEL_ENTRY_TYPE_*
    uint8_t reserved[3];     // Padding for alignment
} dir_entry_info_t;

// Device type codes
#define PANEL_DEVICE_TYPE_ATAPI  0x00  // CD-ROM drive
#define PANEL_DEVICE_TYPE_IDE    0x01  // Hard disk drive
#define PANEL_DEVICE_TYPE_SCSI   0x02  // SCSI device (BlueSCSI)

// Device summary (68 bytes)
typedef struct __attribute__((packed)) {
    uint16_t device_index;
    uint8_t  device_type;       // PANEL_DEVICE_TYPE_*
    uint8_t  device_status;     // PANEL_DEVICE_STATUS_*
    char     device_label[32];  // e.g. "SCSI ID 3", "CD-ROM"
    char     image_name[32];    // Current image name or empty
} device_summary_t;

// Device list response (variable length)
typedef struct __attribute__((packed)) {
    uint8_t  device_count;
    uint8_t  max_devices;
    uint8_t  reserved[2];
    device_summary_t devices[];
} device_list_response_t;

// Currently loaded image status structure (212 bytes)
typedef struct __attribute__((packed)) {
    uint8_t image_loaded;         // 1 if image loaded, 0 if not
    uint8_t device_type;          // PANEL_DEVICE_TYPE_*
    uint8_t reserved1[2];         // Padding
    char image_name[64];          // Name of loaded image (null-terminated)
    char directory_path[128];     // Directory containing the image (null-terminated)
    uint32_t image_index;         // Index in current directory (0-based, only files counted)
    uint32_t total_images;        // Total number of images in directory
    // IDE-specific (only when device_type == PANEL_DEVICE_TYPE_IDE)
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint8_t reserved2[8];         // Reserved for future use
} loaded_image_status_t;
