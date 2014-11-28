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

#include "device.h"
#include "config.h"
#include "USBFS.h"
#include "led.h"

#include "scsi.h"
#include "scsiPhy.h"
#include "disk.h"

#include "../../include/scsi2sd.h"
#include "../../include/hidpacket.h"

#include <string.h>

// CYDEV_EEPROM_ROW_SIZE == 16.
static const char magic[CYDEV_EEPROM_ROW_SIZE] = "codesrc_00000002";
static const uint16_t FIRMWARE_VERSION = 0x0360;

// Config shadow RAM (copy of EEPROM)
static Config shadow =
{
	0, // SCSI ID
	" codesrc", // vendor  (68k Apple Drive Setup: Set to " SEAGATE")
	"         SCSI2SD", //prodId (68k Apple Drive Setup: Set to "          ST225N")
	" 3.6", // revision (68k Apple Drive Setup: Set to "1.0 ")
	1, // enable parity
	1, // enable unit attention,
	0, // RESERVED
	0, // Max sectors (0 == disabled)
	512 // Sector size
	// reserved bytes will be initialised to 0.
};

enum USB_ENDPOINTS
{
	USB_EP_OUT = 1,
	USB_EP_IN = 2,
	USB_EP_COMMAND = 3,
	USB_EP_DEBUG = 4
};
enum USB_STATE
{
	USB_IDLE,
	USB_DATA_SENT
};

static uint8_t hidBuffer[USBHID_LEN];

static int usbInEpState;
static int usbDebugEpState;
static int usbReady;

// Global
Config* config = NULL;

// The PSoC 5LP compile to little-endian format.
static uint32_t ntohl(uint32_t val)
{
	return
		((val & 0xFF) << 24) |
		((val & 0xFF00) << 8) |
		((val >> 8) & 0xFF00) |
		((val >> 24) & 0xFF);
}
static uint16_t ntohs(uint16_t val)
{
	return
		((val & 0xFF) << 8) |
		((val >> 8) & 0xFF);
}
static uint32_t htonl(uint32_t val)
{
	return
		((val & 0xFF) << 24) |
		((val & 0xFF00) << 8) |
		((val >> 8) & 0xFF00) |
		((val >> 24) & 0xFF);
}
static uint16_t htons(uint16_t val)
{
	return
		((val & 0xFF) << 8) |
		((val >> 8) & 0xFF);
}

static void saveConfig()
{
	int shadowRows = (sizeof(shadow) / CYDEV_EEPROM_ROW_SIZE) + 1;
	int row;
	int status = CYRET_SUCCESS;

	CySetTemp();
	for (row = 0; (row < shadowRows) && (status == CYRET_SUCCESS); ++row)
	{
		CFG_EEPROM_Write(((uint8*)&shadow) + (row * CYDEV_EEPROM_ROW_SIZE), row);
	}
	if (status == CYRET_SUCCESS)
	{
		CFG_EEPROM_Write((uint8*)magic, row);
	}
}

void configInit()
{
	int shadowRows, shadowBytes;
	uint8* eeprom = (uint8*)CYDEV_EE_BASE;

	// We could map cfgPtr directly into the EEPROM memory,
	// but that would waste power. Copy it to RAM then turn off
	// the EEPROM.
	CFG_EEPROM_Start();
	CyDelayUs(5); // 5us to start per datasheet.

	// Check magic
	shadowRows = (sizeof(shadow) / CYDEV_EEPROM_ROW_SIZE) + 1;
	shadowBytes = CYDEV_EEPROM_ROW_SIZE * shadowRows;

	if (memcmp(eeprom + shadowBytes, magic, sizeof(magic)))
	{
		// Initial state, invalid, or upgrade required.
		if (!memcmp(eeprom + shadowBytes, magic, sizeof(magic) - 1) &&
			((eeprom + shadowBytes)[sizeof(magic) - 2] == '1'))
		{
			// Upgrade from version 1.
			memcpy(&shadow, eeprom, sizeof(shadow));
			shadow.bytesPerSector = 512;
		}

		saveConfig();
	}
	else
	{
		memcpy(&shadow, eeprom, sizeof(shadow));
	}

	config = &shadow;
	CFG_EEPROM_Stop();

	// The USB block will be powered by an internal 3.3V regulator.
	// The PSoC must be operating between 4.6V and 5V for the regulator
	// to work.
	USBFS_Start(0, USBFS_5V_OPERATION);
	usbInEpState = usbDebugEpState = USB_IDLE;
	usbReady = 0; // We don't know if host is connected yet.
}

static void
readFlashCommand(const uint8_t* cmd, size_t cmdSize)
{
	if (cmdSize < 3)
	{
		return; // ignore.
	}
	uint8_t flashArray = cmd[1];
	uint8_t flashRow = cmd[2];

	uint8_t* flash =
		CY_FLASH_BASE +
		(CY_FLASH_SIZEOF_ARRAY * (size_t) flashArray) +
		(CY_FLASH_SIZEOF_ROW * (size_t) flashRow);

	hidPacket_send(flash, SCSI_CONFIG_ROW_SIZE);
}

static void
writeFlashCommand(const uint8_t* cmd, size_t cmdSize)
{
	if (cmdSize < 259)
	{
		return; // ignore.
	}
	uint8_t flashArray = cmd[257];
	uint8_t flashRow = cmd[258];

	// Be very careful not to overwrite the bootloader or other
	// code
	if ((flashArray != SCSI_CONFIG_ARRAY) ||
		(flashRow < SCSI_CONFIG_0_ROW) ||
		(flashRow >= SCSI_CONFIG_3_ROW + SCSI_CONFIG_ROWS))
	{
		uint8_t response[] = { CONFIG_STATUS_ERR};
		hidPacket_send(response, sizeof(response));
	}
	else
	{
		uint8_t spcBuffer[CYDEV_FLS_ROW_SIZE + CYDEV_ECC_ROW_SIZE];
		CyFlash_Start();
		CySetFlashEEBuffer(spcBuffer);
		CySetTemp();
		int status = CyWriteRowData(flashArray, flashRow, cmd + 1);
		CyFlash_Stop();

		uint8_t response[] =
		{
			status == CYRET_SUCCESS ? CONFIG_STATUS_GOOD : CONFIG_STATUS_ERR
		};
		hidPacket_send(response, sizeof(response));
	}
}

static void
pingCommand()
{
	uint8_t response[] =
	{
		CONFIG_STATUS_GOOD
	};
	hidPacket_send(response, sizeof(response));
}

static void
processCommand(const uint8_t* cmd, size_t cmdSize)
{
	switch (cmd[0])
	{
	case CONFIG_PING:
		pingCommand();
		break;

	case CONFIG_READFLASH:
		readFlashCommand(cmd, cmdSize);
		break;

	case CONFIG_WRITEFLASH:
		writeFlashCommand(cmd, cmdSize);
		break;

	case CONFIG_REBOOT:
		Bootloadable_1_Load();
		break;

	case CONFIG_NONE: // invalid
	default:
		break;
	}
}

void configPoll()
{
	int reset = 0;
	if (!usbReady || USBFS_IsConfigurationChanged())
	{
		reset = 1;
	}
	usbReady = USBFS_bGetConfiguration();

	if (!usbReady)
	{
		return;
	}

	if (reset)
	{
		USBFS_EnableOutEP(USB_EP_OUT);
		USBFS_EnableOutEP(USB_EP_COMMAND);
		usbInEpState = usbDebugEpState = USB_IDLE;
	}

	if(USBFS_GetEPState(USB_EP_OUT) == USBFS_OUT_BUFFER_FULL)
	{
		ledOn();

		// The host sent us some data!
		int byteCount = USBFS_GetEPCount(USB_EP_OUT);
		USBFS_ReadOutEP(USB_EP_OUT, hidBuffer, sizeof(hidBuffer));
		hidPacket_recv(hidBuffer, byteCount);

		size_t cmdSize;
		const uint8_t* cmd = hidPacket_getPacket(&cmdSize);
		if (cmd && (cmdSize > 0))
		{
			processCommand(cmd, cmdSize);
		}

		// Allow the host to send us another updated config.
		USBFS_EnableOutEP(USB_EP_OUT);

		ledOff();
	}

	switch (usbInEpState)
	{
	case USB_IDLE:
		{
			const uint8_t* nextChunk = hidPacket_getHIDBytes(hidBuffer);

			if (nextChunk)
			{
				USBFS_LoadInEP(USB_EP_IN, nextChunk, sizeof(hidBuffer));
				usbInEpState = USB_DATA_SENT;
			}
		}
		break;

	case USB_DATA_SENT:
		if (USBFS_bGetEPAckState(USB_EP_IN))
		{
			// Data accepted.
			usbInEpState = USB_IDLE;
		}
		break;
	}
}

void debugPoll()
{
	if (!usbReady)
	{
		return;
	}

	if(USBFS_GetEPState(USB_EP_COMMAND) == USBFS_OUT_BUFFER_FULL)
	{
		// The host sent us some data!
		int byteCount = USBFS_GetEPCount(USB_EP_COMMAND);
		USBFS_ReadOutEP(USB_EP_COMMAND, (uint8 *)&hidBuffer, byteCount);

		if (byteCount >= 1 &&
			hidBuffer[0] == 0x01)
		{
			// Reboot command.
			Bootloadable_1_Load();
		}

		// Allow the host to send us another command.
		// (assuming we didn't reboot outselves)
		USBFS_EnableOutEP(USB_EP_COMMAND);
	}

	switch (usbDebugEpState)
	{
	case USB_IDLE:
		memcpy(&hidBuffer, &scsiDev.cdb, 12);
		hidBuffer[12] = scsiDev.msgIn;
		hidBuffer[13] = scsiDev.msgOut;
		hidBuffer[14] = scsiDev.lastStatus;
		hidBuffer[15] = scsiDev.lastSense;
		hidBuffer[16] = scsiDev.phase;
		hidBuffer[17] = SCSI_ReadPin(SCSI_In_BSY);
		hidBuffer[18] = SCSI_ReadPin(SCSI_In_SEL);
		hidBuffer[19] = SCSI_ReadPin(SCSI_ATN_INT);
		hidBuffer[20] = SCSI_ReadPin(SCSI_RST_INT);
		hidBuffer[21] = scsiDev.rstCount;
		hidBuffer[22] = scsiDev.selCount;
		hidBuffer[23] = scsiDev.msgCount;
		hidBuffer[24] = scsiDev.cmdCount;
		hidBuffer[25] = scsiDev.watchdogTick;

		hidBuffer[58] = sdDev.capacity >> 24;
		hidBuffer[59] = sdDev.capacity >> 16;
		hidBuffer[60] = sdDev.capacity >> 8;
		hidBuffer[61] = sdDev.capacity;

		hidBuffer[62] = FIRMWARE_VERSION >> 8;
		hidBuffer[63] = FIRMWARE_VERSION;

		USBFS_LoadInEP(USB_EP_DEBUG, (uint8 *)&hidBuffer, sizeof(hidBuffer));
		usbDebugEpState = USB_DATA_SENT;
		break;

	case USB_DATA_SENT:
		if (USBFS_bGetEPAckState(USB_EP_DEBUG))
		{
			// Data accepted.
			usbDebugEpState = USB_IDLE;
		}
		break;
	}
}

CY_ISR(debugTimerISR)
{
	Debug_Timer_ReadStatusRegister();
	Debug_Timer_Interrupt_ClearPending();
	uint8 savedIntrStatus = CyEnterCriticalSection();
	debugPoll();
	CyExitCriticalSection(savedIntrStatus);
}

void debugInit()
{
	Debug_Timer_Interrupt_StartEx(debugTimerISR);
	Debug_Timer_Start();
}

// Public method for storing MODE SELECT results.
void configSave()
{
	CFG_EEPROM_Start();
	saveConfig(); // write to eeprom
	CFG_EEPROM_Stop();
}

