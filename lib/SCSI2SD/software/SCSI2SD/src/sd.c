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
#pragma GCC push_options
#pragma GCC optimize("-flto")

#include "device.h"
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

enum SD_CMD_STATE { CMD_STATE_IDLE, CMD_STATE_READ, CMD_STATE_WRITE };
static int sdCmdState = CMD_STATE_IDLE;
static uint32_t sdCmdNextLBA; // Only valid in CMD_STATE_READ or CMD_STATE_WRITE
static uint32_t sdCmdTime;
static uint32_t sdLastCmdState = CMD_STATE_IDLE;

enum SD_IO_STATE { SD_DMA, SD_ACCEPTED, SD_IDLE };
static int sdIOState = SD_IDLE;

// Private DMA variables.
static uint8 sdDMARxChan = CY_DMA_INVALID_CHANNEL;
static uint8 sdDMATxChan = CY_DMA_INVALID_CHANNEL;

// Dummy location for DMA to send unchecked CRC bytes to
static uint8 discardBuffer __attribute__((aligned(4)));

// 2 bytes CRC, response, 8bits to close the clock..
// "NCR" time is up to 8 bytes.
static uint8_t writeResponseBuffer[8]  __attribute__((aligned(4)));

// Padded with a dummy byte just to allow the tx DMA channel to
// use 2-byte bursts for performance.
static uint8_t writeStartToken[2]  __attribute__((aligned(4))) = {0xFF, 0xFC};

// Source of dummy SPI bytes for DMA
static uint8_t dummyBuffer[2]  __attribute__((aligned(4))) = {0xFF, 0xFF};

volatile uint8_t sdRxDMAComplete;
volatile uint8_t sdTxDMAComplete;

static void sdCompleteRead();
static void sdCompleteWrite();

CY_ISR_PROTO(sdRxISR);
CY_ISR(sdRxISR)
{
	sdRxDMAComplete = 1;
}
CY_ISR_PROTO(sdTxISR);
CY_ISR(sdTxISR)
{
	sdTxDMAComplete = 1;
}

static uint8 sdCrc7(uint8* chr, uint8 cnt, uint8 crc)
{
	uint8 a;
	for(a = 0; a < cnt; a++)
	{
		uint8 Data = chr[a];
		uint8 i;
		for(i = 0; i < 8; i++)
		{
			crc <<= 1;
			if( (Data & 0x80) ^ (crc & 0x80) ) {crc ^= 0x09;}
			Data <<= 1;
		}
	}
	return crc & 0x7F;
}


// Read and write 1 byte.
static uint8_t sdSpiByte(uint8_t value)
{
	SDCard_WriteTxData(value);
	trace(trace_spinSpiByte);
	while (!(SDCard_ReadRxStatus() & SDCard_STS_RX_FIFO_NOT_EMPTY)) {}
	trace(trace_sdSpiByte);
	return SDCard_ReadRxData();
}

static void sdWaitWriteBusy()
{
	uint8 val;
	do
	{
		val = sdSpiByte(0xFF);
	} while (val != 0xFF);
}

static void sdPreCmdState(uint32_t newState)
{
	if (sdCmdState == CMD_STATE_READ)
	{
		sdCompleteRead();
	}
	else if (sdCmdState == CMD_STATE_WRITE)
	{
		sdCompleteWrite();
	}
	sdCmdState = CMD_STATE_IDLE;

	if (sdLastCmdState != newState && newState != CMD_STATE_IDLE)
	{
		sdWaitWriteBusy();
		sdLastCmdState = newState;
	}
}

static uint16_t sdDoCommand(
	uint8_t cmd,
	uint32_t param,
	int useCRC,
	int use2byteResponse)
{
	int waitWhileBusy = (cmd != SD_GO_IDLE_STATE) && (cmd != SD_STOP_TRANSMISSION);

	// "busy" probe. We'll examine the results later.
	if (waitWhileBusy)
	{
		SDCard_WriteTxData(0xFF);
	}

	// send is static as the address must remain consistent for the static
	// DMA descriptors to work.
	// Size must be divisible by 2 to suit 2-byte-burst TX DMA channel.
	static uint8_t send[6] __attribute__((aligned(4)));
	send[0] = cmd | 0x40;
	send[1] = param >> 24;
	send[2] = param >> 16;
	send[3] = param >> 8;
	send[4] = param;
	if (unlikely(useCRC))
	{
		send[5] = (sdCrc7(send, 5, 0) << 1) | 1;
	}
	else
	{
		send[5] = 1; // stop bit
	}

	static uint8_t dmaRxTd = CY_DMA_INVALID_TD;
	static uint8_t dmaTxTd = CY_DMA_INVALID_TD;
	if (unlikely(dmaRxTd == CY_DMA_INVALID_TD))
	{
		dmaRxTd = CyDmaTdAllocate();
		dmaTxTd = CyDmaTdAllocate();
		CyDmaTdSetConfiguration(dmaTxTd, sizeof(send), CY_DMA_DISABLE_TD, TD_INC_SRC_ADR|SD_TX_DMA__TD_TERMOUT_EN);
		CyDmaTdSetAddress(dmaTxTd, LO16((uint32)&send), LO16((uint32)SDCard_TXDATA_PTR));
		CyDmaTdSetConfiguration(dmaRxTd, sizeof(send), CY_DMA_DISABLE_TD, SD_RX_DMA__TD_TERMOUT_EN);
		CyDmaTdSetAddress(dmaRxTd, LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)&discardBuffer));
	}

	sdTxDMAComplete = 0;
	sdRxDMAComplete = 0;

	CyDmaChSetInitialTd(sdDMARxChan, dmaRxTd);
	CyDmaChSetInitialTd(sdDMATxChan, dmaTxTd);

	// Some Samsung cards enter a busy-state after single-sector reads.
	// But we also need to wait for R1B to complete from the multi-sector
	// reads.
	if (waitWhileBusy)
	{
		trace(trace_spinSDRxFIFO);
		while (!(SDCard_ReadRxStatus() & SDCard_STS_RX_FIFO_NOT_EMPTY)) {}
		int busy = SDCard_ReadRxData() != 0xFF;
		if (unlikely(busy))
		{
			trace(trace_spinSDBusy);
			while (sdSpiByte(0xFF) != 0xFF) {}
		}
	}

	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaChSetRequest(sdDMATxChan, CY_DMA_CPU_REQ);
	CyDmaClearPendingDrq(sdDMARxChan);

	// There is no flow control, so we must ensure we can read the bytes
	// before we start transmitting
	CyDmaChEnable(sdDMARxChan, 1);
	CyDmaChEnable(sdDMATxChan, 1);

	trace(trace_spinSDDMA);
	int allComplete = 0;
	while (!allComplete)
	{
		uint8_t intr = CyEnterCriticalSection();
		allComplete = sdTxDMAComplete && sdRxDMAComplete;
		if (!allComplete)
		{
			__WFI();
		}
		CyExitCriticalSection(intr);
	}

	uint16_t response = sdSpiByte(0xFF); // Result code or stuff byte
	if (unlikely(cmd == SD_STOP_TRANSMISSION))
	{
		// Stuff byte is required for this command only.
		// Part 1 Simplified standard 3.01
		// "The stop command has an execution delay due to the serial command
		// transmission."
		response = sdSpiByte(0xFF);
	}

	uint32_t start = getTime_ms();

	trace(trace_spinSDBusy);
	while ((response & 0x80) && likely(elapsedTime_ms(start) <= 200))
	{
		response = sdSpiByte(0xFF);
	}
	if (unlikely(use2byteResponse))
	{
		response = (response << 8) | sdSpiByte(0xFF);
	}
	return response;
}


static inline uint16_t sdCommandAndResponse(uint8_t cmd, uint32_t param)
{
	return sdDoCommand(cmd, param, 0, 0);
}

static inline uint16_t sdCRCCommandAndResponse(uint8_t cmd, uint32_t param)
{
	return sdDoCommand(cmd, param, 1, 0);
}

// Clear the sticky status bits on error.
static void sdClearStatus()
{
	sdSpiByte(0xFF);
	uint16_t r2 = sdDoCommand(SD_SEND_STATUS, 0, 1, 1);
	(void) r2;
}

void
sdReadMultiSectorPrep(uint32_t sdLBA, uint32_t sdSectors)
{
	uint32_t tmpNextLBA = sdLBA + sdSectors;

	if (!sdDev.ccs)
	{
		sdLBA = sdLBA * SD_SECTOR_SIZE;
		tmpNextLBA = tmpNextLBA * SD_SECTOR_SIZE;
	}

	if (sdCmdState == CMD_STATE_READ && sdCmdNextLBA == sdLBA)
	{
		// Well, that was lucky. We're already reading this data
		sdCmdNextLBA = tmpNextLBA;
		sdCmdTime = getTime_ms();
	}
	else
	{
		sdPreCmdState(CMD_STATE_READ);

		uint8_t v = sdCommandAndResponse(SD_READ_MULTIPLE_BLOCK, sdLBA);
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
			sdCmdNextLBA = tmpNextLBA;
			sdCmdState = CMD_STATE_READ;
			sdCmdTime = getTime_ms();
		}
	}
}

static void
dmaReadSector(uint8_t* outputBuffer)
{
	// Wait for a start-block token.
	// Don't wait more than 200ms.  The standard recommends 100ms.
	uint32_t start = getTime_ms();
	uint8_t token = sdSpiByte(0xFF);
	trace(trace_spinSDBusy);
	while (token != 0xFE && likely(elapsedTime_ms(start) <= 200))
	{
		if (unlikely(token && ((token & 0xE0) == 0)))
		{
			// Error token!
			break;
		}
		token = sdSpiByte(0xFF);
	}
	if (unlikely(token != 0xFE))
	{
		if (transfer.multiBlock)
		{
			sdCompleteRead();
		}
		if (scsiDev.status != CHECK_CONDITION)
		{
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = HARDWARE_ERROR;
			scsiDev.target->sense.asc = UNRECOVERED_READ_ERROR;
			scsiDev.phase = STATUS;
		}
		sdClearStatus();
		return;
	}

	static uint8_t dmaRxTd[2] = { CY_DMA_INVALID_TD, CY_DMA_INVALID_TD};
	static uint8_t dmaTxTd = CY_DMA_INVALID_TD;
	if (unlikely(dmaRxTd[0] == CY_DMA_INVALID_TD))
	{
		dmaRxTd[0] = CyDmaTdAllocate();
		dmaRxTd[1] = CyDmaTdAllocate();
		dmaTxTd = CyDmaTdAllocate();

		// Receive 512 bytes of data and then 2 bytes CRC.
		CyDmaTdSetConfiguration(dmaRxTd[0], SD_SECTOR_SIZE, dmaRxTd[1], TD_INC_DST_ADR);
		CyDmaTdSetConfiguration(dmaRxTd[1], 2, CY_DMA_DISABLE_TD, SD_RX_DMA__TD_TERMOUT_EN);
		CyDmaTdSetAddress(dmaRxTd[1], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)&discardBuffer));

		CyDmaTdSetConfiguration(dmaTxTd, SD_SECTOR_SIZE + 2, CY_DMA_DISABLE_TD, SD_TX_DMA__TD_TERMOUT_EN);
		CyDmaTdSetAddress(dmaTxTd, LO16((uint32)&dummyBuffer), LO16((uint32)SDCard_TXDATA_PTR));

	}
	CyDmaTdSetAddress(dmaRxTd[0], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)outputBuffer));

	sdIOState = SD_DMA;
	sdTxDMAComplete = 0;
	sdRxDMAComplete = 0;

	// Re-loading the initial TD's here is very important, or else
	// we'll be re-using the last-used TD, which would be the last
	// in the chain (ie. CRC TD)
	CyDmaChSetInitialTd(sdDMARxChan, dmaRxTd[0]);
	CyDmaChSetInitialTd(sdDMATxChan, dmaTxTd);

	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaChSetRequest(sdDMATxChan, CY_DMA_CPU_REQ);
	CyDmaClearPendingDrq(sdDMARxChan);

	// There is no flow control, so we must ensure we can read the bytes
	// before we start transmitting
	CyDmaChEnable(sdDMARxChan, 1);
	CyDmaChEnable(sdDMATxChan, 1);
}

int
sdReadSectorDMAPoll()
{
	if (sdRxDMAComplete && sdTxDMAComplete)
	{
		// DMA transfer is complete
		sdIOState = SD_IDLE;
		return 1;
	}
	else
	{
		return 0;
	}
}

void sdReadSingleSectorDMA(uint32_t lba, uint8_t* outputBuffer)
{
	sdPreCmdState(CMD_STATE_READ);

	uint8 v;
	if (!sdDev.ccs)
	{
		lba = lba * SD_SECTOR_SIZE;
	}
	v = sdCommandAndResponse(SD_READ_SINGLE_BLOCK, lba);
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
		dmaReadSector(outputBuffer);
	}
}

void
sdReadMultiSectorDMA(uint8_t* outputBuffer)
{
	// Pre: sdReadMultiSectorPrep called.
	dmaReadSector(outputBuffer);
}

static void sdCompleteRead()
{
	if (unlikely(sdIOState != SD_IDLE))
	{
		// Not much choice but to wait until we've completed the transfer.
		// Cancelling the transfer can't be done as we have no way to reset
		// the SD card.
		trace(trace_spinSDCompleteRead);
		while (!sdReadSectorDMAPoll()) { /* spin */ }
	}


	if (sdCmdState == CMD_STATE_READ)
	{
		uint8 r1b = sdCommandAndResponse(SD_STOP_TRANSMISSION, 0);

		if (unlikely(r1b) && (scsiDev.phase == DATA_IN))
		{
			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = HARDWARE_ERROR;
			scsiDev.target->sense.asc = UNRECOVERED_READ_ERROR;
			scsiDev.phase = STATUS;
		}
	}

	// R1b has an optional trailing "busy" signal, but we defer waiting on this.
	// The next call so sdCommandAndResponse will wait for the busy state to
	// clear.

	sdCmdState = CMD_STATE_IDLE;
}

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


	sdIOState = SD_DMA;
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

void sdCompleteTransfer()
{
	sdPreCmdState(CMD_STATE_IDLE);
}


// SD Version 2 (SDHC) support
static int sendIfCond()
{
	int retries = 50;

	do
	{
		// 11:8 Host voltage. 1 = 2.7-3.6V
		// 7:0 Echo bits. Ignore.
		uint8 status = sdCRCCommandAndResponse(SD_SEND_IF_COND, 0x000001AA);

		if (status == SD_R1_IDLE)
		{
			// Version 2 card.
			sdDev.version = 2;
			// Read 32bit response. Should contain the same bytes that
			// we sent in the command parameter.
			sdSpiByte(0xFF);
			sdSpiByte(0xFF);
			sdSpiByte(0xFF);
			sdSpiByte(0xFF);
			break;
		}
		else if (status & SD_R1_ILLEGAL)
		{
			// Version 1 card.
			sdDev.version = 1;
			sdClearStatus();
			break;
		}

		sdClearStatus();
	} while (--retries > 0);

	return retries > 0;
}

static int sdOpCond()
{
	uint32_t start = getTime_ms();

	uint8 status;
	do
	{
		sdCRCCommandAndResponse(SD_APP_CMD, 0);
		// Host Capacity Support = 1 (SDHC/SDXC supported)
		status = sdCRCCommandAndResponse(SD_APP_SEND_OP_COND, 0x40000000);

		sdClearStatus();

	// Spec says to poll for 1 second.
	} while ((status != 0) && (elapsedTime_ms(start) < 1000));

	return status == 0;
}

static int sdReadOCR()
{
	uint32_t start = getTime_ms();
	int complete;
	uint8 status;

	do
	{
		uint8 buf[4];
		int i;

		status = sdCRCCommandAndResponse(SD_READ_OCR, 0);
		if(status) { break; }

		for (i = 0; i < 4; ++i)
		{
			buf[i] = sdSpiByte(0xFF);
		}

		sdDev.ccs = (buf[0] & 0x40) ? 1 : 0;
		complete = (buf[0] & 0x80);

	} while (!status &&
		!complete &&
		(elapsedTime_ms(start) < 1000));

	return (status == 0) && complete;
}

static void sdReadCID()
{
	uint8 startToken;
	int maxWait, i;

	uint8 status = sdCRCCommandAndResponse(SD_SEND_CID, 0);
	if(status){return;}

	maxWait = 1023;
	do
	{
		startToken = sdSpiByte(0xFF);
	} while(maxWait-- && (startToken != 0xFE));
	if (startToken != 0xFE) { return; }

	for (i = 0; i < 16; ++i)
	{
		sdDev.cid[i] = sdSpiByte(0xFF);
	}
	sdSpiByte(0xFF); // CRC
	sdSpiByte(0xFF); // CRC
}

static int sdReadCSD()
{
	uint8 startToken;
	int maxWait, i;

	uint8 status = sdCRCCommandAndResponse(SD_SEND_CSD, 0);
	if(status){goto bad;}

	maxWait = 1023;
	do
	{
		startToken = sdSpiByte(0xFF);
	} while(maxWait-- && (startToken != 0xFE));
	if (startToken != 0xFE) { goto bad; }

	for (i = 0; i < 16; ++i)
	{
		sdDev.csd[i] = sdSpiByte(0xFF);
	}
	sdSpiByte(0xFF); // CRC
	sdSpiByte(0xFF); // CRC

	if ((sdDev.csd[0] >> 6) == 0x00)
	{
		// CSD version 1
		// C_SIZE in bits [73:62]
		uint32 c_size = (((((uint32)sdDev.csd[6]) & 0x3) << 16) | (((uint32)sdDev.csd[7]) << 8) | sdDev.csd[8]) >> 6;
		uint32 c_mult = (((((uint32)sdDev.csd[9]) & 0x3) << 8) | ((uint32)sdDev.csd[0xa])) >> 7;
		uint32 sectorSize = sdDev.csd[5] & 0x0F;
		sdDev.capacity = ((c_size+1) * ((uint64)1 << (c_mult+2)) * ((uint64)1 << sectorSize)) / SD_SECTOR_SIZE;
	}
	else if ((sdDev.csd[0] >> 6) == 0x01)
	{
		// CSD version 2
		// C_SIZE in bits [69:48]

		uint32 c_size =
			((((uint32)sdDev.csd[7]) & 0x3F) << 16) |
			(((uint32)sdDev.csd[8]) << 8) |
			((uint32)sdDev.csd[7]);
		sdDev.capacity = (c_size + 1) * 1024;
	}
	else
	{
		goto bad;
	}

	return 1;
bad:
	return 0;
}

static void sdInitDMA()
{
	// One-time init only.
	if (sdDMATxChan == CY_DMA_INVALID_CHANNEL)
	{
		sdDMATxChan =
			SD_TX_DMA_DmaInitialize(
				2, // Bytes per burst
				1, // request per burst
				HI16(CYDEV_SRAM_BASE),
				HI16(CYDEV_PERIPH_BASE)
				);

		sdDMARxChan =
			SD_RX_DMA_DmaInitialize(
				1, // Bytes per burst
				1, // request per burst
				HI16(CYDEV_PERIPH_BASE),
				HI16(CYDEV_SRAM_BASE)
				);

		CyDmaChDisable(sdDMATxChan);
		CyDmaChDisable(sdDMARxChan);

		SD_RX_DMA_COMPLETE_StartEx(sdRxISR);
		SD_TX_DMA_COMPLETE_StartEx(sdTxISR);
	}
}

int sdInit()
{
	int result = 0;
	int i;
	uint8 v;

	sdCmdState = CMD_STATE_IDLE;
	sdDev.version = 0;
	sdDev.ccs = 0;
	sdDev.capacity = 0;
	memset(sdDev.csd, 0, sizeof(sdDev.csd));
	memset(sdDev.cid, 0, sizeof(sdDev.cid));

	sdInitDMA();

	SD_CS_SetDriveMode(SD_CS_DM_STRONG);
	SD_CS_Write(1); // Set CS inactive (active low)

	// Set the SPI clock for 400kHz transfers
	// 25MHz / 400kHz approx factor of 63.
	// The register contains (divider - 1)
	uint16_t clkDiv25MHz =  SD_Data_Clk_GetDividerRegister();
	SD_Data_Clk_SetDivider(((clkDiv25MHz + 1) * 63) - 1);
	// Wait for the clock to settle.
	CyDelayUs(1);

	SDCard_Start(); // Enable SPI hardware

	// Power on sequence. 74 clock cycles of a "1" while CS unasserted.
	for (i = 0; i < 10; ++i)
	{
		sdSpiByte(0xFF);
	}

	SD_CS_Write(0); // Set CS active (active low)
	CyDelayUs(1);

	sdSpiByte(0xFF);
	v = sdDoCommand(SD_GO_IDLE_STATE, 0, 1, 0);
	if(v != 1){goto bad;}

	ledOn();
	if (!sendIfCond()) goto bad; // Sets V1 or V2 flag  CMD8
	if (!sdOpCond()) goto bad; // ACMD41. Wait for init completes.
	if (!sdReadOCR()) goto bad; // CMD58. Get CCS flag. Only valid after init.

	// This command will be ignored if sdDev.ccs is set.
	// SDHC and SDXC are always 512bytes.
	v = sdCRCCommandAndResponse(SD_SET_BLOCKLEN, SD_SECTOR_SIZE); //Force sector size
	if(v){goto bad;}
	v = sdCRCCommandAndResponse(SD_CRC_ON_OFF, 0); //crc off
	if(v){goto bad;}

	// now set the sd card back to full speed.
	// The SD Card spec says we can run SPI @ 25MHz
	SDCard_Stop();

	// We can't run at full-speed with the pullup resistors enabled.
	SD_MISO_SetDriveMode(SD_MISO_DM_DIG_HIZ);
	SD_MOSI_SetDriveMode(SD_MOSI_DM_STRONG);
	SD_SCK_SetDriveMode(SD_SCK_DM_STRONG);

	SD_Data_Clk_SetDivider(clkDiv25MHz);
	CyDelayUs(1);
	SDCard_Start();

	// Clear out rubbish data through clock change
	CyDelayUs(1);
	SDCard_ReadRxStatus();
	SDCard_ReadTxStatus();
	SDCard_ClearFIFO();

	if (!sdReadCSD()) goto bad;
	sdReadCID();

	result = 1;
	goto out;

bad:
	SD_Data_Clk_SetDivider(clkDiv25MHz); // Restore the clock for our next retry
	sdDev.capacity = 0;

out:
	sdClearStatus();
	ledOff();
	return result;

}

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

void sdCheckPresent()
{
	// Check if there's an SD card present.
	if ((scsiDev.phase == BUS_FREE) &&
		(sdIOState == SD_IDLE) &&
		(sdCmdState == CMD_STATE_IDLE))
	{
		// The CS line is pulled high by the SD card.
		// De-assert the line, and check if it's high.
		// This isn't foolproof as it'll be left floating without
		// an SD card. We can't use the built-in pull-down resistor as it will
		// overpower the SD pullup resistor.
		SD_CS_Write(0);
		SD_CS_SetDriveMode(SD_CS_DM_DIG_HIZ);

		// Delay extended to work with 60cm cables running cards at 2.85V
		CyDelayCycles(128);
		uint8_t cs = SD_CS_Read();
		SD_CS_SetDriveMode(SD_CS_DM_STRONG)	;

		if (cs && !(blockDev.state & DISK_PRESENT))
		{
			static int firstInit = 1;

			// Debounce
			CyDelay(250);

			if (sdInit())
			{
				blockDev.state |= DISK_PRESENT | DISK_INITIALISED;

				// Always "start" the device. Many systems (eg. Apple System 7)
				// won't respond properly to
				// LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED sense
				// code, even if they stopped it first with
				// START STOP UNIT command.
				blockDev.state |= DISK_STARTED;

				if (!firstInit)
				{
					int i;
					for (i = 0; i < MAX_SCSI_TARGETS; ++i)
					{
						scsiDev.targets[i].unitAttention = PARAMETERS_CHANGED;
					}
				}
				firstInit = 0;
			}
		}
		else if (!cs && (blockDev.state & DISK_PRESENT))
		{
			sdDev.capacity = 0;
			blockDev.state &= ~DISK_PRESENT;
			blockDev.state &= ~DISK_INITIALISED;
			int i;
			for (i = 0; i < MAX_SCSI_TARGETS; ++i)
			{
				scsiDev.targets[i].unitAttention = PARAMETERS_CHANGED;
			}
		}
	}
}

#pragma GCC pop_options
