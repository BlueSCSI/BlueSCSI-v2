//	Copyright (C) 2016 Michael McMaster <michael@codesrc.com>
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


#include "usbd_msc_storage_sd.h"
#include "stm32f2xx.h"
#include "bsp_driver_sd.h"
#include "../bsp.h"
#include "../disk.h"
#include "../led.h"
#include "../sd.h"
#include "../config.h"
#include "../geometry.h"
#include "../inquiry.h"
#include "usb_device.h"



static int8_t s2s_usbd_storage_Init(uint8_t lun);

static int8_t s2s_usbd_storage_GetCapacity(
	uint8_t lun,
	uint32_t *block_num,
	uint16_t *block_size);

static int8_t s2s_usbd_storage_IsReady(uint8_t lun);

static int8_t s2s_usbd_storage_IsWriteProtected(uint8_t lun);

int8_t s2s_usbd_storage_Read(
	uint8_t lun,
	uint8_t *buf,
	uint32_t blk_addr,
	uint16_t blk_len);

int8_t s2s_usbd_storage_Write(
	uint8_t lun,
	uint8_t *buf,
	uint32_t blk_addr,
	uint16_t blk_len);

int8_t s2s_usbd_storage_GetMaxLun (void);
uint32_t s2s_usbd_storage_Inquiry (uint8_t lun, uint8_t* buf, uint8_t maxlen);


USBD_StorageTypeDef USBD_MSC_SD_fops =
{
	s2s_usbd_storage_Init,
	s2s_usbd_storage_GetCapacity,
	s2s_usbd_storage_IsReady,
	s2s_usbd_storage_IsWriteProtected,
	s2s_usbd_storage_Read,
	s2s_usbd_storage_Write,
	s2s_usbd_storage_GetMaxLun,
	s2s_usbd_storage_Inquiry
};

static const S2S_TargetCfg* getUsbConfig(uint8_t lun) {
	int count = 0;
	for (int i = 0; i < S2S_MAX_TARGETS; ++i)
	{
		const S2S_TargetCfg* cfg = s2s_getConfigByIndex(i);
		if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
		{
			if (count == lun)
			{
				return cfg;
			}
			else
			{
				count++;
			}
		}
	}
	return s2s_getConfigByIndex(0); // Fallback, try not to crash
}

int8_t s2s_usbd_storage_Init(uint8_t lun)
{
	return (0);
}

int8_t s2s_usbd_storage_GetCapacity (uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
	const S2S_TargetCfg* cfg = getUsbConfig(lun);

	uint32_t capacity = getScsiCapacity(
		cfg->sdSectorStart,
		cfg->bytesPerSector,
		cfg->scsiSectors);

	*block_num  = capacity;
	*block_size = cfg->bytesPerSector;
	return (0);
}

uint32_t s2s_usbd_storage_Inquiry (uint8_t lun, uint8_t* buf, uint8_t maxlen)
{
	const S2S_TargetCfg* cfg = getUsbConfig(lun);

	return s2s_getStandardInquiry(cfg, buf, maxlen);
}

int8_t s2s_usbd_storage_IsReady (uint8_t lun)
{
	const S2S_TargetCfg* cfg = getUsbConfig(lun);
	return (
			cfg &&
			(blockDev.state & DISK_PRESENT) &&
			(blockDev.state & DISK_INITIALISED)
			) ? 0 : 1; // inverse logic
}


int8_t s2s_usbd_storage_IsWriteProtected (uint8_t lun)
{
	return blockDev.state & DISK_WP;
}

int8_t s2s_usbd_storage_Read (uint8_t lun,
		uint8_t *buf,
		uint32_t blk_addr,
		uint16_t blk_len)
{
	s2s_ledOn();
	const S2S_TargetCfg* cfg = getUsbConfig(lun);

	if (cfg->bytesPerSector == 512)
	{
		BSP_SD_ReadBlocks_DMA(
			(uint32_t*) buf,
			(cfg->sdSectorStart + blk_addr) * 512ll,
			512,
			blk_len);
	}
	else
	{
		int sdPerScsi = SDSectorsPerSCSISector(cfg->bytesPerSector);
		int sdSectorNum =  cfg->sdSectorStart + (blk_addr * sdPerScsi);

		for (int blk = 0; blk < blk_len; ++blk)
		{
			for (int i = 0; i < SDSectorsPerSCSISector(cfg->bytesPerSector); ++i)
			{
				uint8_t partial[512] S2S_DMA_ALIGN;
				BSP_SD_ReadBlocks_DMA(
					(uint32_t*) partial,
					sdSectorNum * 512LL,
					512,
					1);
				sdSectorNum++;

				int validBytes = cfg->bytesPerSector % SD_SECTOR_SIZE;
				if (validBytes == 0) validBytes = SD_SECTOR_SIZE;

				memcpy(buf, partial, validBytes);

				buf += validBytes;
			}

		}

	}
	s2s_ledOff();
	return 0;
}

int8_t s2s_usbd_storage_Write (uint8_t lun,
		uint8_t *buf,
		uint32_t blk_addr,
		uint16_t blk_len)
{
	s2s_ledOn();
	const S2S_TargetCfg* cfg = getUsbConfig(lun);

	if (cfg->bytesPerSector == 512)
	{
		BSP_SD_WriteBlocks_DMA(
			(uint32_t*) buf,
			(cfg->sdSectorStart + blk_addr) * 512ll,
			512,
			blk_len);
	}
	else
	{
		int sdPerScsi = SDSectorsPerSCSISector(cfg->bytesPerSector);
		int sdSectorNum =  cfg->sdSectorStart + (blk_addr * sdPerScsi);

		for (int blk = 0; blk < blk_len; ++blk)
		{
			for (int i = 0; i < SDSectorsPerSCSISector(cfg->bytesPerSector); ++i)
			{
				uint8_t partial[512] S2S_DMA_ALIGN;
				BSP_SD_WriteBlocks_DMA(
					(uint32_t*) partial,
					sdSectorNum * 512LL,
					512,
					1);
				sdSectorNum++;

				int validBytes = cfg->bytesPerSector % SD_SECTOR_SIZE;
				if (validBytes == 0) validBytes = SD_SECTOR_SIZE;

				memcpy(buf, partial, validBytes);

				buf += validBytes;
			}

		}

	}

	s2s_ledOff();
	return 0;
}

int8_t s2s_usbd_storage_GetMaxLun (void)
{
	int count = 0;
	for (int i = 0; i < S2S_MAX_TARGETS; ++i)
	{
		const S2S_TargetCfg* cfg = s2s_getConfigByIndex(i);
		if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
		{
			count++;
		}
	}
	return count - 1 < 0 ? 0 : count - 1;
}

void s2s_initUsbDeviceStorage(void)
{
	USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_MSC_SD_fops);
}

