//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
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

#ifdef STM32F2xx
#include "stm32f2xx.h"
#endif

#ifdef STM32F4xx
#include "stm32f4xx.h"
#endif

#include "sdio.h"
#include "bsp_driver_sd.h"


#include "scsi.h"
#include "config.h"
#include "disk.h"
#include "sd.h"
#include "led.h"
#include "time.h"

#include "scsiPhy.h"

#include <string.h>

// Global
SdDevice sdDev;

static int sdCmdActive = 0;

int
sdReadDMAPoll(uint32_t remainingSectors)
{
	// TODO DMA byte counting disabled for now as it's not
	// working.
	// We can ask the SDIO controller how many bytes have been
	// processed (SDIO_GetDataCounter()) but I'm not sure if that
	// means the data has been transfered via dma to memory yet.
//	uint32_t dmaBytesRemaining = __HAL_DMA_GET_COUNTER(hsd.hdmarx) * 4;

	if (HAL_SD_GetState(&hsd) != HAL_SD_STATE_BUSY)
	{
		// DMA transfer is complete
		sdCmdActive = 0;
		return remainingSectors;
	}
/*	else
	{
		return remainingSectors - ((dmaBytesRemaining + (SD_SECTOR_SIZE - 1)) / SD_SECTOR_SIZE);
	}*/
	return 0;
}

void sdReadDMA(uint32_t lba, uint32_t sectors, uint8_t* outputBuffer)
{
	if (HAL_SD_ReadBlocks_DMA(&hsd, outputBuffer, lba, sectors) != HAL_OK)
	{
		scsiDiskReset();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = HARDWARE_ERROR;
		scsiDev.target->sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
	else
	{
		sdCmdActive = 1;
	}
}

void sdCompleteTransfer()
{
	if (sdCmdActive)
	{
		HAL_SD_Abort(&hsd);
		sdCmdActive = 0;
	}
}

static void sdClear()
{
	sdDev.version = 0;
	sdDev.capacity = 0;
	memset(sdDev.csd, 0, sizeof(sdDev.csd));
	memset(sdDev.cid, 0, sizeof(sdDev.cid));
}

static int sdDoInit()
{
	int result = 0;

	sdClear();

	int8_t error = BSP_SD_Init();
	if (error == MSD_OK)
	{
		HAL_SD_CardInfoTypeDef cardInfo;
		HAL_SD_GetCardInfo(&hsd, &cardInfo);
		memcpy(sdDev.csd, hsd.CSD, sizeof(sdDev.csd));
		memcpy(sdDev.cid, hsd.CID, sizeof(sdDev.cid));
		sdDev.capacity = cardInfo.BlockNbr;
		blockDev.state |= DISK_PRESENT | DISK_INITIALISED;
		result = 1;

		goto out;
	}

//bad:
	blockDev.state &= ~(DISK_PRESENT | DISK_INITIALISED);

	sdDev.capacity = 0;

out:
	s2s_ledOff();
	return result;
}

int sdInit()
{
	// Check if there's an SD card present.
	int result = 0;

	static int firstInit = 1;

	if (firstInit)
	{
		blockDev.state &= ~(DISK_PRESENT | DISK_INITIALISED);
		sdClear();
	}

	if (firstInit || (scsiDev.phase == BUS_FREE))
	{
		uint8_t cs = HAL_GPIO_ReadPin(nSD_CD_GPIO_Port, nSD_CD_Pin) ? 0 : 1;
		uint8_t wp = HAL_GPIO_ReadPin(nSD_WP_GPIO_Port, nSD_WP_Pin) ? 0 : 1;

		if (cs && !(blockDev.state & DISK_PRESENT))
		{
			s2s_ledOn();

			// Debounce
			if (!firstInit)
			{
				s2s_delay_ms(250);
			}

			if (sdDoInit())
			{
				blockDev.state |= DISK_PRESENT | DISK_INITIALISED;

				if (wp)
				{
					blockDev.state |= DISK_WP;
				}
				else
				{
					blockDev.state &= ~DISK_WP;
				}

				// Always "start" the device. Many systems (eg. Apple System 7)
				// won't respond properly to
				// LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED sense
				// code, even if they stopped it first with
				// START STOP UNIT command.
				blockDev.state |= DISK_STARTED;

				result = 1;

				s2s_ledOff();
			}
			else
			{
				for (int i = 0; i < 10; ++i)
				{
					// visual indicator of SD error
					s2s_ledOff();
					s2s_delay_ms(50);
					s2s_ledOn();
					s2s_delay_ms(50);
				}
				s2s_ledOff();
			}
		}
		else if (!cs && (blockDev.state & DISK_PRESENT))
		{
			sdDev.capacity = 0;
			blockDev.state &= ~DISK_PRESENT;
			blockDev.state &= ~DISK_INITIALISED;
			int i;
			for (i = 0; i < S2S_MAX_TARGETS; ++i)
			{
				scsiDev.targets[i].unitAttention = PARAMETERS_CHANGED;
			}

			HAL_SD_DeInit(&hsd);
		}
	}
	firstInit = 0;

	return result;
}

