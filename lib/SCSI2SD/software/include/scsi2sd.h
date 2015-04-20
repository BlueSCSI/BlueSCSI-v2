//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.
#ifndef scsi2sd_h
#define scsi2sd_h

#ifdef __cplusplus
extern "C" {
#endif

/* Common type definitions shared between the firmware and config tools

	The configuration data is stored in flash.

	The flash is organised as 2 arrays of 256 rows, with each row
	having 256 bytes. Total of 128kb.

	Linear flash memory map:
	-----------------------------------------
	Array 1 |Row 255 | Bootloader metadata
			---------------------------------
			|Row 254 |
			|Row 252 | Blank
			---------------------------------
			|Row 251 |
			| ...    |
			|Row 236 | Config target 3
			| ...    |
			|Row 220 | Config target 2
			| ...    |
			|Row 204 | Config target 1
			| ...    |
			|Row 188 | Config target 0
			---------------------------------
			|Row 235 |
			| ...    |
			|Row 0   |
	--------|        |
	Array 0	|Row 255 | Blank
			---------------------------------
			|Row 121 |
			| ...    |
			|Row 37  | Application
			---------------------------------
			|Row 36  |
			| ...    |
			|Row 0   | Bootloader

*/

#include "stdint.h"

#define MAX_SCSI_TARGETS 4
#define SCSI_CONFIG_ARRAY 1
#define SCSI_CONFIG_ROWS 16

// 256 bytes data, 32 bytes ECC
#define SCSI_CONFIG_ROW_SIZE 256
#define SCSI_CONFIG_ROW_ECC 288
#define SCSI_CONFIG_0_ROW 188
#define SCSI_CONFIG_1_ROW 204
#define SCSI_CONFIG_2_ROW 220
#define SCSI_CONFIG_3_ROW 236

typedef enum
{
	CONFIG_TARGET_ID_BITS = 0x07,
	CONFIG_TARGET_ENABLED = 0x80
} CONFIG_TARGET_FLAGS;

typedef enum
{
	CONFIG_ENABLE_UNIT_ATTENTION = 1,
	CONFIG_ENABLE_PARITY = 2,
} CONFIG_FLAGS;

typedef enum
{
	CONFIG_FIXED,
	CONFIG_REMOVEABLE,
	CONFIG_OPTICAL,
	CONFIG_FLOPPY_14MB
} CONFIG_TYPE;

typedef enum
{
	CONFIG_QUIRKS_NONE,
	CONFIG_QUIRKS_APPLE
} CONFIG_QUIRKS;

typedef struct __attribute__((packed))
{
	uint8_t deviceType;
	uint8_t pageCode;
	uint8_t reserved;
	uint8_t pageLength;
	uint8_t data[0]; // pageLength bytes.
} VPD;

typedef struct __attribute__((packed))
{
	// bits 7 -> 3 = CONFIG_TARGET_FLAGS
	// bits 2 -> 0 = target SCSI ID.
	uint8_t scsiId;

	uint8_t deviceType; // CONFIG_TYPE
	uint8_t flags; // CONFIG_FLAGS
	uint8_t deviceTypeModifier; // Used in INQUIRY response.

	uint32_t sdSectorStart;
	uint32_t scsiSectors;

	uint16_t bytesPerSector;

	// Max allowed by legacy IBM-PC bios is 6 bits (63)
	uint16_t sectorsPerTrack;

	// MS-Dos up to 7.10 will crash on >= 256 heads.
	uint16_t headsPerCylinder;


	char vendor[8];
	char prodId[16];
	char revision[4];
	char serial[16];

	uint16_t quirks; // CONFIG_QUIRKS

	uint8_t reserved[960]; // Pad out to 1024 bytes for main section.

	uint8_t vpd[3072]; // Total size is 4k.
} TargetConfig;

typedef enum
{
	CONFIG_NONE, // Invalid

	// Command content:
	// uint8_t CONFIG_PING
	// Response:
	// CONFIG_STATUS
	CONFIG_PING,

	// Command content:
	// uint8_t CONFIG_WRITEFLASH
	// uint8_t[256] flashData
	// uint8_t flashArray
	// uint8_t flashRow
	// Response:
	// CONFIG_STATUS
	CONFIG_WRITEFLASH,

	// Command content:
	// uint8_t CONFIG_READFLASH
	// uint8_t flashArray
	// uint8_t flashRow
	// Response:
	// 256 bytes of flash
	CONFIG_READFLASH,

	// Command content:
	// uint8_t CONFIG_REBOOT
	// Response: None.
	CONFIG_REBOOT,

	// Command content:
	// uint8_t CONFIG_INFO
	// Response:
	// uint8_t[16] CSD
	// uint8_t[16] CID
	CONFIG_SDINFO,

	// Command content:
	// uint8_t CONFIG_SCSITEST
	// Response:
	// CONFIG_STATUS
	// uint8_t result code (0 = passed)
	CONFIG_SCSITEST
} CONFIG_COMMAND;

typedef enum
{
	CONFIG_STATUS_GOOD,
	CONFIG_STATUS_ERR
} CONFIG_STATUS;




#ifdef __cplusplus
} // extern "C"

	#include <type_traits>
	static_assert(
		std::is_pod<TargetConfig>::value, "Misuse of TargetConfig struct"
		);
	static_assert(
		sizeof(TargetConfig) == 4096,
		"TargetConfig struct size mismatch"
		);

#endif

#endif
