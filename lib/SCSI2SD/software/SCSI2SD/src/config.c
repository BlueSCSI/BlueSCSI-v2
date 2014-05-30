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

#include <string.h>

// CYDEV_EEPROM_ROW_SIZE == 16.
static const char magic[CYDEV_EEPROM_ROW_SIZE] = "codesrc_00000002";

// Config shadow RAM (copy of EEPROM)
static Config shadow =
{
	0, // SCSI ID
	" codesrc", // vendor  (68k Apple Drive Setup: Set to " SEAGATE")
	"         SCSI2SD", //prodId (68k Apple Drive Setup: Set to "          ST225N")
	" 3.4", // revision (68k Apple Drive Setup: Set to "1.0 ")
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
	USB_EP_DEBUG = 4
};
enum USB_STATE
{
	USB_IDLE,
	USB_DATA_SENT
};
int usbInEpState;
int usbDebugEpState;
uint8_t debugBuffer[64];

int usbReady;

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
		usbInEpState = usbDebugEpState = USB_IDLE;
	}

	if(USBFS_GetEPState(USB_EP_OUT) == USBFS_OUT_BUFFER_FULL)
	{
		int byteCount;

		ledOn();

		// The host sent us some data!
		byteCount = USBFS_GetEPCount(USB_EP_OUT);

		// Assume that byteCount <= sizeof(shadow).
		// shadow should be padded out to 64bytes, which is the largest
		// possible HID transfer.
		USBFS_ReadOutEP(USB_EP_OUT, (uint8 *)&shadow, byteCount);
		shadow.maxSectors = ntohl(shadow.maxSectors);
		shadow.bytesPerSector = ntohs(shadow.bytesPerSector);

		if (shadow.bytesPerSector > MAX_SECTOR_SIZE)
		{
			shadow.bytesPerSector = MAX_SECTOR_SIZE;
		}
		else if (shadow.bytesPerSector < MIN_SECTOR_SIZE)
		{
			shadow.bytesPerSector = MIN_SECTOR_SIZE;
		}

		CFG_EEPROM_Start();
		saveConfig(); // write to eeprom
		CFG_EEPROM_Stop();

		// Send the updated data.
		usbInEpState = USB_IDLE;

		// Allow the host to send us another updated config.
		USBFS_EnableOutEP(USB_EP_OUT);

		// Set unt attention as the block size may have changed.
		scsiDev.unitAttention = MODE_PARAMETERS_CHANGED;

		ledOff();
	}

	switch (usbInEpState)
	{
	case USB_IDLE:
		shadow.maxSectors = htonl(shadow.maxSectors);
		shadow.bytesPerSector = htons(shadow.bytesPerSector);

		USBFS_LoadInEP(USB_EP_IN, (uint8 *)&shadow, sizeof(shadow));
		shadow.maxSectors = ntohl(shadow.maxSectors);
		shadow.bytesPerSector = ntohs(shadow.bytesPerSector);
		usbInEpState = USB_DATA_SENT;
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

#ifdef MM_DEBUG
void debugPoll()
{
	if (!usbReady)
	{
		return;
	}
	
	switch (usbDebugEpState)
	{
	case USB_IDLE:
		memcpy(&debugBuffer, &scsiDev.cdb, 12);
		debugBuffer[12] = scsiDev.msgIn;
		debugBuffer[13] = scsiDev.msgOut;
		debugBuffer[14] = scsiDev.lastStatus;
		debugBuffer[15] = scsiDev.lastSense;
		debugBuffer[16] = scsiDev.phase;
		debugBuffer[17] = SCSI_ReadPin(SCSI_In_BSY);
		debugBuffer[18] = SCSI_ReadPin(SCSI_In_SEL);
		debugBuffer[19] = SCSI_ReadPin(SCSI_ATN_INT);
		debugBuffer[20] = SCSI_ReadPin(SCSI_RST_INT);
		debugBuffer[21] = scsiDev.rstCount;
		debugBuffer[22] = scsiDev.selCount;
		debugBuffer[23] = scsiDev.msgCount;
		debugBuffer[24] = scsiDev.cmdCount;
		debugBuffer[25] = scsiDev.watchdogTick;

		USBFS_LoadInEP(USB_EP_DEBUG, (uint8 *)&debugBuffer, sizeof(debugBuffer));
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
#endif

void debugInit()
{
#ifdef MM_DEBUG
	Debug_Timer_Interrupt_StartEx(debugTimerISR);
	Debug_Timer_Start();
#else
	Debug_Timer_Interrupt_Stop();
	Debug_Timer_Stop();
#endif
	
}

// Public method for storing MODE SELECT results.
void configSave()
{
	CFG_EEPROM_Start();
	saveConfig(); // write to eeprom
	CFG_EEPROM_Stop();
}

