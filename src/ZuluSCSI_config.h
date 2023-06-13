/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
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

// Compile-time configuration parameters.
// Other settings can be set by ini file at runtime.

#pragma once

#include <ZuluSCSI_platform.h>

// Use variables for version number
#define FW_VER_NUM      "23.06.13"
#define FW_VER_SUFFIX   "devel"
#define ZULU_FW_VERSION FW_VER_NUM "-" FW_VER_SUFFIX

// Configuration and log file paths
#define CONFIGFILE  "zuluscsi.ini"
#define LOGFILE     "zululog.txt"
#define CRASHFILE   "zuluerr.txt"

// Prefix for command file to create new image (case-insensitive)
#define CREATEFILE "create"

// Log buffer size in bytes, must be a power of 2
#ifndef LOGBUFSIZE
#define LOGBUFSIZE 16384
#endif
#define LOG_SAVE_INTERVAL_MS 1000

// Watchdog timeout
// Watchdog will first issue a bus reset and if that does not help, crashdump.
#define WATCHDOG_BUS_RESET_TIMEOUT 15000
#define WATCHDOG_CRASH_TIMEOUT 30000

// HDD image file format
#define HDIMG_ID_POS  2                 // Position to embed ID number
#define HDIMG_LUN_POS 3                 // Position to embed LUN numbers
#define HDIMG_BLK_POS 5                 // Position to embed block size numbers
#define MAX_FILE_PATH 64                // Maximum file name length

// Image definition options
#define IMAGE_INDEX_MAX 9               // Maximum number of 'IMG0' style statements parsed

// SCSI config
#define NUM_SCSIID  8          // Maximum number of supported SCSI-IDs (The minimum is 0)
#define NUM_SCSILUN 1          // Maximum number of LUNs supported     (Currently has to be 1)
#define READ_PARITY_CHECK 0    // Perform read parity check (unverified)

// SCSI raw fallback configuration when no image files are detected
// Presents the whole SD card as an SCSI drive
#define RAW_FALLBACK_ENABLE 1
#define RAW_FALLBACK_SCSI_ID 1
#define RAW_FALLBACK_BLOCKSIZE 512

// Default SCSI drive information (can be overridden in INI file)
// Selected based on device type (fixed, removable, optical, floppy, mag-optical, tape)
// Each entry has {vendor, product, version, serial}
// If serial number is left empty, SD card serial number is used.
#define DRIVEINFO_FIXED     {"ZULUSCSI", "HARDDRIVE", PLATFORM_REVISION, ""}
#define DRIVEINFO_REMOVABLE {"ZULUSCSI", "REMOVABLE", PLATFORM_REVISION, ""}
#define DRIVEINFO_OPTICAL   {"ZULUSCSI", "CDROM",     PLATFORM_REVISION, ""}
#define DRIVEINFO_FLOPPY    {"ZULUSCSI", "FLOPPY",    PLATFORM_REVISION, ""}
#define DRIVEINFO_MAGOPT    {"ZULUSCSI", "MO_DRIVE",  PLATFORM_REVISION, ""}
#define DRIVEINFO_TAPE      {"ZULUSCSI", "TAPE",      PLATFORM_REVISION, ""}

// Default SCSI drive information when Apple quirks are enabled
#define APPLE_DRIVEINFO_FIXED     {"CDC",      "ZuluSCSI HDD",      PLATFORM_REVISION, "1.0"}
#define APPLE_DRIVEINFO_REMOVABLE {"IOMEGA",   "BETA230",           PLATFORM_REVISION, "2.02"}
#define APPLE_DRIVEINFO_OPTICAL   {"MATSHITA", "CD-ROM CR-8004A",   PLATFORM_REVISION, "2.0a"}
#define APPLE_DRIVEINFO_FLOPPY    {"TEAC",     "FD235HS",           PLATFORM_REVISION, "1.44"}
#define APPLE_DRIVEINFO_MAGOPT    {"MOST",     "RMD-5200",          PLATFORM_REVISION, "1.0"}
#define APPLE_DRIVEINFO_TAPE      {"ZULUSCSI", "APPLE_TAPE",        PLATFORM_REVISION, ""}

// Default delay for SCSI phases.
// Can be adjusted in ini file
#define DEFAULT_SCSI_DELAY_US 10
#define DEFAULT_REQ_TYPE_SETUP_NS 500

// Use prefetch buffer in read requests
#ifndef PREFETCH_BUFFER_SIZE
#define PREFETCH_BUFFER_SIZE 8192
#endif
