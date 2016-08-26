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

static const uint16_t FIRMWARE_VERSION = 0x0607;

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

void s2s_configInit(S2S_BoardCfg* config)
{

	usbInEpState = USB_IDLE;
	usbReady = 0; // We don't know if host is connected yet.


	if ((blockDev.state & DISK_PRESENT) && sdDev.capacity)
	{
		int cfgSectors = (S2S_CFG_SIZE + 511) / 512;
		BSP_SD_ReadBlocks_DMA(
			(uint32_t*) &s2s_cfg[0],
			(sdDev.capacity - cfgSectors) * 512ll,
			512,
			cfgSectors);

		memcpy(config, s2s_cfg, sizeof(S2S_BoardCfg));

		if (memcmp(config->magic, "BCFG", 4))
		{
			// Invalid SD card config, use default.
			memset(&s2s_cfg[0], 0, S2S_CFG_SIZE);
			memcpy(config, s2s_cfg, sizeof(S2S_BoardCfg));
			memcpy(config->magic, "BCFG", 4);
			config->selectionDelay = 255; // auto
			config->flags6 = S2S_CFG_ENABLE_TERMINATOR;

			memcpy(
				&s2s_cfg[0] + sizeof(S2S_BoardCfg),
				DEFAULT_CONFIG,
				sizeof(S2S_TargetCfg));
		}
	}
	else
	{
		// No SD card, use existing config if valid
		if (memcmp(config->magic, "BCFG", 4))
		{
			// Not valid, use empty config with no disks.
			memset(&s2s_cfg[0], 0, S2S_CFG_SIZE);
			memcpy(config, s2s_cfg, sizeof(S2S_BoardCfg));
			config->selectionDelay = 255; // auto
			config->flags6 = S2S_CFG_ENABLE_TERMINATOR;
		}
	}

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
	int resultCode = scsiSelfTest();
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
debugCommand()
{
	uint8_t response[32];
	memcpy(&response, &scsiDev.cdb, 12);
	response[12] = scsiDev.msgIn;
	response[13] = scsiDev.msgOut;
	response[14] = scsiDev.lastStatus;
	response[15] = scsiDev.lastSense;
	response[16] = scsiDev.phase;
	response[17] = scsiStatusBSY();
	response[18] = scsiStatusSEL();
	response[19] = scsiStatusATN();
	response[20] = scsiStatusRST();
	response[21] = scsiDev.rstCount;
	response[22] = scsiDev.selCount;
	response[23] = scsiDev.msgCount;
	response[24] = scsiDev.cmdCount;
	response[25] = scsiDev.watchdogTick;
	response[26] = blockDev.state;
	response[27] = scsiDev.lastSenseASC >> 8;
	response[28] = scsiDev.lastSenseASC;
	response[29] = *SCSI_STS_DBX;
	response[30] = LastTrace;
	response[31] = scsiStatusACK();
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

	case S2S_CMD_DEBUG:
		debugCommand();
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

