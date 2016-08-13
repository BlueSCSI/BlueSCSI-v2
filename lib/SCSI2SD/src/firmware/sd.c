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

#include "stm32f2xx.h"
#include "sdio.h"
#include "bsp_driver_sd.h"


#include "scsi.h"
#include "config.h"
#include "disk.h"
#include "sd.h"
#include "led.h"
#include "time.h"
#include "trace.h"

#include "scsiPhy.h"

#include <string.h>

// Global
SdDevice sdDev;

static int sdCmdActive = 0;

int
sdReadDMAPoll()
{
	if (hsd.DmaTransferCplt ||
		hsd.SdTransferCplt ||
		(HAL_SD_ErrorTypedef)hsd.SdTransferErr != SD_OK)
	{
		HAL_SD_CheckReadOperation(&hsd, (uint32_t)SD_DATATIMEOUT);
		// DMA transfer is complete
		sdCmdActive = 0;
		return 1;
	}
	else
	{
		return 0;
	}
}

void sdReadDMA(uint32_t lba, uint32_t sectors, uint8_t* outputBuffer)
{
	if (HAL_SD_ReadBlocks_DMA(
			&hsd, (uint32_t*)outputBuffer, lba * 512ll, 512, sectors
			) != SD_OK)
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
		HAL_SD_StopTransfer(&hsd);
		HAL_DMA_Abort(hsd.hdmarx);
		HAL_DMA_Abort(hsd.hdmatx);
		sdCmdActive = 0;
	}
}


#if 0
void
sdWriteMultiSectorDMA(uint8_t* outputBuffer)
{
	static uint8_t dmaRxTd[2] = { CY_DMA_INVALID_TD, CY_DMA_INVALID_TD};
	static uint8_t dmaTxTd[3] = { CY_DMA_INVALID_TD, CY_DMA_INVALID_TD, CY_DMA_INVALID_TD};
	if (unlikely(dmaRxTd[0] == CY_DMA_INVALID_TD))
	{
		dmaRxTd[0] = CyDmaTdAllocate();
		dmaRxTd[1] = CyDmaTdAllocate();
		dmaTxTd[0] = CyDmaTdAllocate();
		dmaTxTd[1] = CyDmaTdAllocate();
		dmaTxTd[2] = CyDmaTdAllocate();

		// Transmit 512 bytes of data and then 2 bytes CRC, and then get the response byte
		// We need to do this without stopping the clock
		CyDmaTdSetConfiguration(dmaTxTd[0], 2, dmaTxTd[1], TD_INC_SRC_ADR);
		CyDmaTdSetAddress(dmaTxTd[0], LO16((uint32)&writeStartToken), LO16((uint32)SDCard_TXDATA_PTR));

		CyDmaTdSetConfiguration(dmaTxTd[1], SD_SECTOR_SIZE, dmaTxTd[2], TD_INC_SRC_ADR);

		CyDmaTdSetConfiguration(dmaTxTd[2], 2 + sizeof(writeResponseBuffer), CY_DMA_DISABLE_TD, SD_TX_DMA__TD_TERMOUT_EN);
		CyDmaTdSetAddress(dmaTxTd[2], LO16((uint32)&dummyBuffer), LO16((uint32)SDCard_TXDATA_PTR));

		CyDmaTdSetConfiguration(dmaRxTd[0], SD_SECTOR_SIZE + 4, dmaRxTd[1], 0);
		CyDmaTdSetAddress(dmaRxTd[0], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)&discardBuffer));
		CyDmaTdSetConfiguration(dmaRxTd[1], sizeof(writeResponseBuffer), CY_DMA_DISABLE_TD, SD_RX_DMA__TD_TERMOUT_EN|TD_INC_DST_ADR);
		CyDmaTdSetAddress(dmaRxTd[1], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)&writeResponseBuffer));
	}
	CyDmaTdSetAddress(dmaTxTd[1], LO16((uint32)outputBuffer), LO16((uint32)SDCard_TXDATA_PTR));


	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaChSetRequest(sdDMATxChan, CY_DMA_CPU_REQ);
	CyDmaClearPendingDrq(sdDMARxChan);

	sdTxDMAComplete = 0;
	sdRxDMAComplete = 0;

	// Re-loading the initial TD's here is very important, or else
	// we'll be re-using the last-used TD, which would be the last
	// in the chain (ie. CRC TD)
	CyDmaChSetInitialTd(sdDMARxChan, dmaRxTd[0]);
	CyDmaChSetInitialTd(sdDMATxChan, dmaTxTd[0]);

	// There is no flow control, so we must ensure we can read the bytes
	// before we start transmitting
	CyDmaChEnable(sdDMARxChan, 1);
	CyDmaChEnable(sdDMATxChan, 1);
}

int
sdWriteSectorDMAPoll()
{
	if (sdRxDMAComplete && sdTxDMAComplete)
	{
		if (sdIOState == SD_DMA)
		{
			// Retry a few times. The data token format is:
			// XXX0AAA1
			int i = 0;
			uint8_t dataToken;
			do
			{
				dataToken = writeResponseBuffer[i]; // Response
				++i;
			} while (((dataToken & 0x0101) != 1) && (i < sizeof(writeResponseBuffer)));

			// At this point we should either have an accepted token, or we'll
			// timeout and proceed into the error case below.
			if (unlikely(((dataToken & 0x1F) >> 1) != 0x2)) // Accepted.
			{
				sdIOState = SD_IDLE;

				sdWaitWriteBusy();
				sdSpiByte(0xFD); // STOP TOKEN
				sdWaitWriteBusy();

				sdCmdState = CMD_STATE_IDLE;
				scsiDiskReset();
				sdClearStatus();

				scsiDev.status = CHECK_CONDITION;
				scsiDev.target->sense.code = HARDWARE_ERROR;
				scsiDev.target->sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
				scsiDev.phase = STATUS;
			}
			else
			{
				sdIOState = SD_ACCEPTED;
			}
		}

		if (sdIOState == SD_ACCEPTED)
		{
			// Wait while the SD card is busy
			if (sdSpiByte(0xFF) == 0xFF)
			{
				sdIOState = SD_IDLE;
			}
		}

		return sdIOState == SD_IDLE;
	}
	else
	{
		return 0;
	}
}

static void sdCompleteWrite()
{
	if (unlikely(sdIOState != SD_IDLE))
	{
		// Not much choice but to wait until we've completed the transfer.
		// Cancelling the transfer can't be done as we have no way to reset
		// the SD card.
		trace(trace_spinSDCompleteWrite);
		while (!sdWriteSectorDMAPoll()) { /* spin */ }
	}

	if (sdCmdState == CMD_STATE_WRITE)
	{
		sdWaitWriteBusy();

		sdSpiByte(0xFD); // STOP TOKEN

		sdWaitWriteBusy();
	}


	if (likely(scsiDev.phase == DATA_OUT))
	{
		uint16_t r2 = sdDoCommand(SD_SEND_STATUS, 0, 0, 1);
		if (unlikely(r2))
		{
			sdClearStatus();
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = HARDWARE_ERROR;
			scsiDev.target->sense.asc = WRITE_ERROR_AUTO_REALLOCATION_FAILED;
			scsiDev.phase = STATUS;
		}
	}
	sdCmdState = CMD_STATE_IDLE;
}

#endif
static void sdInitDMA()
{
	// One-time init only.
	static uint8_t init = 0;
	if (init == 0)
	{
		init = 1;

		//TODO MM SEE STUPID SD_DMA_RxCplt that require the SD IRQs to preempt
		// Configured with 4 bits preemption, NO sub priority.
		HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 8, 0);
		HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
		HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 8, 0);
		HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
	}
}

void sdTmpRead(uint8_t* data, uint32_t lba, int sectors)
{
	BSP_SD_ReadBlocks_DMA((uint32_t*) data, lba * 512ll, 512, sectors);
}

void sdTmpWrite(uint8_t* data, uint32_t lba, int sectors)
{
	BSP_SD_WriteBlocks_DMA((uint32_t*) data, lba * 512ll, 512, sectors);
}

static void sdClear()
{
	sdDev.version = 0;
	sdDev.ccs = 0;
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
		memcpy(sdDev.csd, &SDCardInfo.SD_csd, sizeof(sdDev.csd));
		memcpy(sdDev.cid, &SDCardInfo.SD_cid, sizeof(sdDev.cid));
		sdDev.capacity = SDCardInfo.CardCapacity / SD_SECTOR_SIZE;
		blockDev.state |= DISK_PRESENT | DISK_INITIALISED;
		result = 1;

		// SD Benchmark code
		// Currently 8MB/s
		#ifdef SD_BENCHMARK
		while(1)
		{
			s2s_ledOn();
			// 100MB
			int maxSectors = MAX_SECTOR_SIZE / SD_SECTOR_SIZE;
			for (
				int i = 0;
				i < (100LL * 1024 * 1024 / (maxSectors * SD_SECTOR_SIZE));
				++i)
			{
				sdTmpRead(&scsiDev.data[0], 0, maxSectors);
			}
			s2s_ledOff();

			for(int i = 0; i < 10; ++i) s2s_delay_ms(1000);
		}
		#endif

		goto out;
	}

//bad:
	blockDev.state &= ~(DISK_PRESENT | DISK_INITIALISED);

	sdDev.capacity = 0;

out:
	s2s_ledOff();
	return result;
}

#if 0
void sdWriteMultiSectorPrep(uint32_t sdLBA, uint32_t sdSectors)
{
	uint32_t tmpNextLBA = sdLBA + sdSectors;

	if (!sdDev.ccs)
	{
		sdLBA = sdLBA * SD_SECTOR_SIZE;
		tmpNextLBA = tmpNextLBA * SD_SECTOR_SIZE;
	}

	if (sdCmdState == CMD_STATE_WRITE && sdCmdNextLBA == sdLBA)
	{
		// Well, that was lucky. We're already writing this data
		sdCmdNextLBA = tmpNextLBA;
		sdCmdTime = getTime_ms();
	}
	else
	{
		sdPreCmdState(CMD_STATE_WRITE);

		// Set the number of blocks to pre-erase by the multiple block write
		// command. We don't care about the response - if the command is not
		// accepted, writes will just be a bit slower. Max 22bit parameter.
		uint32 blocks = sdSectors > 0x7FFFFF ? 0x7FFFFF : sdSectors;
		sdCommandAndResponse(SD_APP_CMD, 0);
		sdCommandAndResponse(SD_APP_SET_WR_BLK_ERASE_COUNT, blocks);

		uint8_t v = sdCommandAndResponse(SD_WRITE_MULTIPLE_BLOCK, sdLBA);
		if (unlikely(v))
		{
			scsiDiskReset();
			sdClearStatus();
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = HARDWARE_ERROR;
			scsiDev.target->sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
			scsiDev.phase = STATUS;
		}
		else
		{
			sdCmdTime = getTime_ms();
			sdCmdNextLBA = tmpNextLBA;
			sdCmdState = CMD_STATE_WRITE;
		}
	}
}

void sdPoll()
{
	if ((scsiDev.phase == BUS_FREE) &&
		(sdCmdState != CMD_STATE_IDLE) &&
		(elapsedTime_ms(sdCmdTime) >= 50))
	{
		sdPreCmdState(CMD_STATE_IDLE);
	}
}

#endif
int sdInit()
{
	// Check if there's an SD card present.
	int result = 0;

	static int firstInit = 1;

	if (firstInit)
	{
		blockDev.state &= ~(DISK_PRESENT | DISK_INITIALISED);
		sdClear();
		sdInitDMA();
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

