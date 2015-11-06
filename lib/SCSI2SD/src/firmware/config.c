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

#include "config.h"
#include "debug.h"
#include "led.h"

#include "scsi.h"
#include "scsiPhy.h"
#include "sd.h"
#include "disk.h"
#include "trace.h"
#include "bootloader.h"
#include "bsp.h"

#include "../../include/scsi2sd.h"
#include "../../include/hidpacket.h"

#include "usb_device/usb_device.h"
#include "usb_device/usbd_hid.h"
#include "usb_device/usbd_composite.h"
#include "bsp_driver_sd.h"


#include <string.h>

static const uint16_t FIRMWARE_VERSION = 0x0600;

// 1 flash row
static const uint8_t DEFAULT_CONFIG[128] =
{
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x3F, 0x00,
	0x00, 0x02, 0x3F, 0x00, 0xFF, 0x00, 0x20, 0x63, 0x6F, 0x64, 0x65, 0x73,
	0x72, 0x63, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x53,
	0x43, 0x53, 0x49, 0x32, 0x53, 0x44, 0x20, 0x31, 0x2E, 0x30, 0x31, 0x32,
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x00, 0x00
};


static uint8_t s2s_cfg[S2S_CFG_SIZE] S2S_DMA_ALIGN;


enum USB_STATE
{
	USB_IDLE,
	USB_DATA_SENT
};


static int usbInEpState;
#if 0
static int usbDebugEpState;
#endif
static int usbReady; // TODO MM REMOVE. Unused ?

static void initS2S_BoardCfg(S2S_BoardCfg* config) {
	if (memcmp(config->magic, "BCFG", 4)) {
		config->selectionDelay = 255; // auto
		config->flags6 = S2S_CFG_ENABLE_TERMINATOR;

		memcpy(
			s2s_cfg + sizeof(S2S_BoardCfg),
			DEFAULT_CONFIG,
			sizeof(S2S_TargetCfg));
	}
}


void s2s_configInit(S2S_BoardCfg* config)
{

	usbInEpState = USB_IDLE;
	usbReady = 0; // We don't know if host is connected yet.


	if (blockDev.state & DISK_PRESENT && sdDev.capacity)
	{
		int cfgSectors = (S2S_CFG_SIZE + 511) / 512;
		BSP_SD_ReadBlocks_DMA(
			(uint32_t*) &s2s_cfg[0],
			(sdDev.capacity - cfgSectors) * 512ll,
			512,
			cfgSectors);

		memcpy(config, s2s_cfg, sizeof(S2S_BoardCfg));
	}

	initS2S_BoardCfg(config);

	scsiPhyConfig();
}


static void
pingCommand()
{
	uint8_t response[] =
	{
		S2S_CFG_STATUS_GOOD
	};
	hidPacket_send(response, sizeof(response));
}

static void
sdInfoCommand()
{
	uint8_t response[sizeof(sdDev.csd) + sizeof(sdDev.cid)];
	memcpy(response, sdDev.csd, sizeof(sdDev.csd));
	memcpy(response + sizeof(sdDev.csd), sdDev.cid, sizeof(sdDev.cid));

	hidPacket_send(response, sizeof(response));
}


static void
scsiTestCommand()
{
	int resultCode = 0; // TODO scsiSelfTest();
	uint8_t response[] =
	{
		resultCode == 0 ? S2S_CFG_STATUS_GOOD : S2S_CFG_STATUS_ERR,
		resultCode
	};
	hidPacket_send(response, sizeof(response));
}

static void
scsiDevInfoCommand()
{
	uint8_t response[] =
	{
		FIRMWARE_VERSION >> 8,
		FIRMWARE_VERSION & 0xff,
		sdDev.capacity >> 24,
		sdDev.capacity >> 16,
		sdDev.capacity >> 8,
		sdDev.capacity
	};
	hidPacket_send(response, sizeof(response));
}


static void
sdWriteCommand(const uint8_t* cmd, size_t cmdSize)
{
	if (cmdSize < 517)
	{
		return; // ignore.
	}
	uint32_t lba =
		(((uint32_t)cmd[1]) << 24) |
		(((uint32_t)cmd[2]) << 16) |
		(((uint32_t)cmd[3]) << 8) |
		((uint32_t)cmd[4]);

	// Must be aligned.
	uint8_t buf[512] S2S_DMA_ALIGN;
	memcpy(buf, &cmd[5], 512);
	BSP_SD_WriteBlocks_DMA((uint32_t*) buf, lba * 512ll, 512, 1);

	uint8_t response[] =
	{
		S2S_CFG_STATUS_GOOD
	};
	hidPacket_send(response, sizeof(response));
}

static void
sdReadCommand(const uint8_t* cmd, size_t cmdSize)
{
	if (cmdSize < 5)
	{
		return; // ignore.
	}
	uint32_t lba =
		(((uint32_t)cmd[1]) << 24) |
		(((uint32_t)cmd[2]) << 16) |
		(((uint32_t)cmd[3]) << 8) |
		((uint32_t)cmd[4]);

	BSP_SD_ReadBlocks_DMA((uint32_t*) cmd, lba * 512ll, 512, 1);
	hidPacket_send(cmd, 512);
}

static void
processCommand(const uint8_t* cmd, size_t cmdSize)
{
	switch (cmd[0])
	{
	case S2S_CMD_PING:
		pingCommand();
		break;

	case S2S_CMD_REBOOT:
		s2s_enterBootloader();
		break;

	case S2S_CMD_SDINFO:
		sdInfoCommand();
		break;

	case S2S_CMD_SCSITEST:
		scsiTestCommand();
		break;

	case S2S_CMD_DEVINFO:
		scsiDevInfoCommand();
		break;

	case S2S_CMD_SD_WRITE:
		sdWriteCommand(cmd, cmdSize);
		break;

	case S2S_CMD_SD_READ:
		sdReadCommand(cmd, cmdSize);
		break;

	case S2S_CMD_NONE: // invalid
	default:
		break;
	}
}

void s2s_configPoll()
{
	if (!USBD_Composite_IsConfigured(&hUsbDeviceFS))
	{
		usbInEpState = USB_IDLE;
		return;
	}

	if (USBD_HID_IsReportReady(&hUsbDeviceFS))
	{
		s2s_ledOn();

		// The host sent us some data!
		uint8_t hidBuffer[USBHID_LEN];
		int byteCount = USBD_HID_GetReport(&hUsbDeviceFS, hidBuffer, sizeof(hidBuffer));
		hidPacket_recv(hidBuffer, byteCount);

		size_t cmdSize;
		const uint8_t* cmd = hidPacket_getPacket(&cmdSize);
		if (cmd && (cmdSize > 0))
		{
			processCommand(cmd, cmdSize);
		}

		s2s_ledOff();
	}

	switch (usbInEpState)
	{
	case USB_IDLE:
		{
			uint8_t hidBuffer[USBHID_LEN];
			const uint8_t* nextChunk = hidPacket_getHIDBytes(hidBuffer);

			if (nextChunk)
			{
				USBD_HID_SendReport (&hUsbDeviceFS, nextChunk, sizeof(hidBuffer));
				usbInEpState = USB_DATA_SENT;
			}
		}
		break;

	case USB_DATA_SENT:
		if (!USBD_HID_IsBusy(&hUsbDeviceFS))
		{
			// Data accepted.
			usbInEpState = USB_IDLE;
		}
		break;
	}

}

void debugPoll()
{
#if 0
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
		hidBuffer[17] = SCSI_ReadFilt(SCSI_Filt_BSY);
		hidBuffer[18] = SCSI_ReadFilt(SCSI_Filt_SEL);
		hidBuffer[19] = SCSI_ReadFilt(SCSI_Filt_ATN);
		hidBuffer[20] = SCSI_ReadFilt(SCSI_Filt_RST);
		hidBuffer[21] = scsiDev.rstCount;
		hidBuffer[22] = scsiDev.selCount;
		hidBuffer[23] = scsiDev.msgCount;
		hidBuffer[24] = scsiDev.cmdCount;
		hidBuffer[25] = scsiDev.watchdogTick;
		hidBuffer[26] = blockDev.state;
		hidBuffer[27] = scsiDev.lastSenseASC >> 8;
		hidBuffer[28] = scsiDev.lastSenseASC;
		hidBuffer[29] = scsiReadDBxPins();
		hidBuffer[30] = LastTrace;

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
#endif
}

#if 0
CY_ISR(debugTimerISR)
{
	Debug_Timer_ReadStatusRegister();
	Debug_Timer_Interrupt_ClearPending();
	uint8 savedIntrStatus = CyEnterCriticalSection();
	debugPoll();
	CyExitCriticalSection(savedIntrStatus);
}
#endif

void s2s_debugInit()
{
#if 0
	Debug_Timer_Interrupt_StartEx(debugTimerISR);
	Debug_Timer_Start();
#endif
}

void debugPause()
{
#if 0
	Debug_Timer_Stop();
#endif
}

void debugResume()
{
#if 0
	Debug_Timer_Start();
#endif
}

int isDebugEnabled()
{
	return usbReady;
}

// Public method for storing MODE SELECT results.
void s2s_configSave(int scsiId, uint16_t bytesPerSector)
{
	S2S_TargetCfg* cfg = (S2S_TargetCfg*) s2s_getConfigById(scsiId);
	cfg->bytesPerSector = bytesPerSector;

	BSP_SD_WriteBlocks_DMA(
		(uint32_t*) &s2s_cfg[0],
		(sdDev.capacity - S2S_CFG_SIZE) * 512ll,
		512,
		(S2S_CFG_SIZE + 511) / 512);
}


const S2S_TargetCfg* s2s_getConfigByIndex(int i)
{
	return (const S2S_TargetCfg*)
		(s2s_cfg + sizeof(S2S_BoardCfg) + (i * sizeof(S2S_TargetCfg)));
}

const S2S_TargetCfg* s2s_getConfigById(int scsiId)
{
	int i;
	for (i = 0; i < S2S_MAX_TARGETS; ++i)
	{
		const S2S_TargetCfg* tgt = s2s_getConfigByIndex(i);
		if ((tgt->scsiId & S2S_CFG_TARGET_ID_BITS) == scsiId)
		{
			return tgt;
		}
	}
	return NULL;

}

