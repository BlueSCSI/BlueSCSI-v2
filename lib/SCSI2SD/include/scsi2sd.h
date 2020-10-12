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

	The configuration data is now stored on the SD card, occupying the
	last 2 sectors.

	BoardConfig
	TargetConfig (disk 0)
	TargetConfig (disk 1)
	TargetConfig (disk 2)
	TargetConfig (disk 3)
	TargetConfig (disk 4)
	TargetConfig (disk 5)
	TargetConfig (disk 6)

*/

#include "stdint.h"

#define S2S_MAX_TARGETS 7
#define S2S_CFG_SIZE (S2S_MAX_TARGETS * sizeof(S2S_TargetCfg) + sizeof(S2S_BoardCfg))

typedef enum
{
	S2S_CFG_TARGET_ID_BITS = 0x07,
	S2S_CFG_TARGET_ENABLED = 0x80
} S2S_CFG_TARGET_FLAGS;

typedef enum
{
	S2S_CFG_ENABLE_UNIT_ATTENTION = 1,
	S2S_CFG_ENABLE_PARITY = 2,
	S2S_CFG_ENABLE_SCSI2 = 4,
	S2S_CFG_DISABLE_GLITCH = 8,
	S2S_CFG_ENABLE_CACHE = 16,
	S2S_CFG_ENABLE_DISCONNECT = 32,
	S2S_CFG_ENABLE_SEL_LATCH = 64,
	S2S_CFG_MAP_LUNS_TO_IDS = 128
} S2S_CFG_FLAGS;

typedef enum
{
	S2S_CFG_ENABLE_TERMINATOR = 1,
	S2S_CFG_ENABLE_BLIND_WRITES = 2,
} S2S_CFG_FLAGS6;

typedef enum
{
	S2S_CFG_FIXED,
	S2S_CFG_REMOVEABLE,
	S2S_CFG_OPTICAL,
	S2S_CFG_FLOPPY_14MB,
	S2S_CFG_MO,
	S2S_CFG_SEQUENTIAL

} S2S_CFG_TYPE;

typedef enum
{
	S2S_CFG_QUIRKS_NONE = 0,
	S2S_CFG_QUIRKS_APPLE = 1,
	S2S_CFG_QUIRKS_OMTI = 2,
	S2S_CFG_QUIRKS_XEBEC = 4,
	S2S_CFG_QUIRKS_VMS = 8
} S2S_CFG_QUIRKS;

typedef enum
{
	S2S_CFG_SPEED_NoLimit,
	S2S_CFG_SPEED_ASYNC_15,
	S2S_CFG_SPEED_ASYNC_33,
	S2S_CFG_SPEED_ASYNC_50,
	S2S_CFG_SPEED_SYNC_5,
	S2S_CFG_SPEED_SYNC_10,
	S2S_CFG_SPEED_TURBO
} S2S_CFG_SPEED;

typedef struct __attribute__((packed))
{
	// bits 7 -> 3 = S2S_CFG_TARGET_FLAGS
	// bits 2 -> 0 = target SCSI ID.
	uint8_t scsiId;

	uint8_t deviceType; // S2S_CFG_TYPE
	uint8_t flagsDEPRECATED; // S2S_CFG_FLAGS, removed in v4.5
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

	uint16_t quirks; // S2S_CFG_QUIRKS

	uint8_t reserved[64]; // Pad out to 128 bytes for main section.
} S2S_TargetCfg;

typedef struct __attribute__((packed))
{
	char magic[4]; // 'BCFG'
	uint8_t flags; // S2S_CFG_FLAGS
	uint8_t startupDelay; // Seconds.
	uint8_t selectionDelay; // milliseconds. 255 = auto
	uint8_t flags6; // S2S_CFG_FLAGS6

	uint8_t scsiSpeed;

	uint8_t reserved[119]; // Pad out to 128 bytes
} S2S_BoardCfg;

typedef enum
{
	S2S_CMD_NONE, // Invalid

	// Command content:
	// uint8_t S2S_CFG_PING
	// Response:
	// S2S_CFG_STATUS
	S2S_CMD_PING,

	// Command content:
	// uint8_t S2S_CFG_WRITEFLASH
	// uint8_t[256] flashData
	// uint8_t flashArray
	// uint8_t flashRow
	// Response:
	// S2S_CFG_STATUS
	S2S_CMD_WRITEFLASH,

	// Command content:
	// uint8_t S2S_CFG_READFLASH
	// uint8_t flashArray
	// uint8_t flashRow
	// Response:
	// 256 bytes of flash
	S2S_CMD_READFLASH,

	// Command content:
	// uint8_t S2S_CFG_REBOOT
	// Response: None.
	S2S_CMD_REBOOT,

	// Command content:
	// uint8_t S2S_CFG_INFO
	// Response:
	// uint8_t[16] CSD
	// uint8_t[16] CID
	S2S_CMD_SDINFO,

	// Command content:
	// uint8_t S2S_CFG_SCSITEST
	// Response:
	// S2S_CFG_STATUS
	// uint8_t result code (0 = passed)
	S2S_CMD_SCSITEST,

	// Command content:
	// uint8_t S2S_CFG_DEVINFO
	// Response:
	// uint16_t protocol version (MSB)
	// uint16_t firmware version (MSB)
	// uint32_t SD capacity(MSB)
	S2S_CMD_DEVINFO,

	// Command content:
	// uint8_t S2S_CFG_SD_WRITE
	// uint32_t Sector Number (MSB)
	// uint8_t[512] data
	// Response:
	// S2S_CFG_STATUS
	S2S_CMD_SD_WRITE,

	// Command content:
	// uint8_t S2S_CFG_SD_READ
	// uint32_t Sector Number (MSB)
	// Response:
	// 512 bytes of data
	S2S_CMD_SD_READ,

	// Command content:
	// uint8_t S2S_CFG_DEBUG
	// Response:
	S2S_CMD_DEBUG,
} S2S_COMMAND;

typedef enum
{
	S2S_CFG_STATUS_GOOD,
	S2S_CFG_STATUS_ERR,
	S2S_CFG_STATUS_BUSY
} S2S_CFG_STATUS;




#ifdef __cplusplus
} // extern "C"

	#include <type_traits>
	static_assert(
		std::is_pod<S2S_TargetCfg>::value, "Misuse of TargetConfig struct"
		);
	static_assert(
		sizeof(S2S_TargetCfg) == 128,
		"TargetConfig struct size mismatch"
		);

	static_assert(
		std::is_pod<S2S_BoardCfg>::value, "Misuse of BoardConfig struct"
		);
	static_assert(
		sizeof(S2S_BoardCfg) == 128,
		"BoardConfig struct size mismatch"
		);

#endif

#endif
