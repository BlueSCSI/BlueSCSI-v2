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

#include "device.h"
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

// Private DMA variables.
static int dmaInProgress = 0;
static uint8 sdDMARxChan = CY_DMA_INVALID_CHANNEL;
static uint8 sdDMATxChan = CY_DMA_INVALID_CHANNEL;

// DMA descriptors
static uint8 sdDMARxTd[2] = { CY_DMA_INVALID_TD, CY_DMA_INVALID_TD };
static uint8 sdDMATxTd[2] = { CY_DMA_INVALID_TD, CY_DMA_INVALID_TD };

// Dummy location for DMA to send unchecked CRC bytes to
static uint8 discardBuffer;

// Source of dummy SPI bytes for DMA
static uint8 dummyBuffer = 0xFF;

volatile static uint8 rxDMAComplete;
volatile static uint8 txDMAComplete;

CY_ISR_PROTO(sdRxISR);
CY_ISR(sdRxISR)
{
	rxDMAComplete = 1;
}
CY_ISR_PROTO(sdTxISR);
CY_ISR(sdTxISR)
{
	txDMAComplete = 1;
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
static uint8 sdSpiByte(uint8 value)
{
	SDCard_WriteTxData(value);
	while (!(SDCard_ReadRxStatus() & SDCard_STS_RX_FIFO_NOT_EMPTY)) {}
	return SDCard_ReadRxData();
}

static void sdSendCRCCommand(uint8 cmd, uint32 param)
{
	uint8 send[6];

	send[0] = cmd | 0x40;
	send[1] = param >> 24;
	send[2] = param >> 16;
	send[3] = param >> 8;
	send[4] = param;
	send[5] = (sdCrc7(send, 5, 0) << 1) | 1;

	for(cmd = 0; cmd < sizeof(send); cmd++)
	{
		sdSpiByte(send[cmd]);
	}
	// Allow command to process before reading result code.
	sdSpiByte(0xFF);
}

static void sdSendCommand(uint8 cmd, uint32 param)
{
	uint8 send[6];

	send[0] = cmd | 0x40;
	send[1] = param >> 24;
	send[2] = param >> 16;
	send[3] = param >> 8;
	send[4] = param;
	send[5] = 1; // 7:1 CRC, 0: Stop bit.

	for(cmd = 0; cmd < sizeof(send); cmd++)
	{
		sdSpiByte(send[cmd]);
	}
	// Allow command to process before reading result code.
	sdSpiByte(0xFF);
}

static uint8 sdReadResp()
{
	uint8 v;
	uint8 i = 128;
	do
	{
		v = sdSpiByte(0xFF);
	} while(i-- && (v & 0x80));
	return v;
}

static uint8 sdCommandAndResponse(uint8 cmd, uint32 param)
{
	sdSpiByte(0xFF);
	sdSendCommand(cmd, param);
	return sdReadResp();
}

static uint8 sdCRCCommandAndResponse(uint8 cmd, uint32 param)
{
	sdSpiByte(0xFF);
	sdSendCRCCommand(cmd, param);
	return sdReadResp();
}

// Clear the sticky status bits on error.
static void sdClearStatus()
{
	uint8 r2hi = sdCRCCommandAndResponse(SD_SEND_STATUS, 0);
	uint8 r2lo = sdSpiByte(0xFF);
	(void) r2hi; (void) r2lo;
}


void
sdReadMultiSectorPrep()
{
	uint8 v;
	uint32 scsiLBA = (transfer.lba + transfer.currentBlock);
	uint32 sdLBA = SCSISector2SD(scsiDev.target->cfg, scsiLBA);

	if (!sdDev.ccs)
	{
		sdLBA = sdLBA * SD_SECTOR_SIZE;
	}
	v = sdCommandAndResponse(SD_READ_MULTIPLE_BLOCK, sdLBA);
	if (v)
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
		transfer.inProgress = 1;
	}
}

static void
dmaReadSector(uint8_t* outputBuffer)
{
	// Wait for a start-block token.
	// Don't wait more than 200ms.
	// The standard recommends 100ms.
	uint32_t start = getTime_ms();
	uint8 token = sdSpiByte(0xFF);
	while (token != 0xFE && (diffTime_ms(start, getTime_ms()) <= 200))
	{
		token = sdSpiByte(0xFF);
	}
	if (token != 0xFE)
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
		return;
	}

	// Receive 512 bytes of data and then 2 bytes CRC.
	CyDmaTdSetConfiguration(sdDMARxTd[0], SD_SECTOR_SIZE, sdDMARxTd[1], TD_INC_DST_ADR);
	CyDmaTdSetAddress(sdDMARxTd[0], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)outputBuffer));
	CyDmaTdSetConfiguration(sdDMARxTd[1], 2, CY_DMA_DISABLE_TD, SD_RX_DMA__TD_TERMOUT_EN);
	CyDmaTdSetAddress(sdDMARxTd[1], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)&discardBuffer));

	CyDmaTdSetConfiguration(sdDMATxTd[0], SD_SECTOR_SIZE + 2, CY_DMA_DISABLE_TD, SD_TX_DMA__TD_TERMOUT_EN);
	CyDmaTdSetAddress(sdDMATxTd[0], LO16((uint32)&dummyBuffer), LO16((uint32)SDCard_TXDATA_PTR));

	dmaInProgress = 1;
	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaClearPendingDrq(sdDMATxChan);
	CyDmaClearPendingDrq(sdDMARxChan);

	txDMAComplete = 0;
	rxDMAComplete = 0;

	// Re-loading the initial TD's here is very important, or else
	// we'll be re-using the last-used TD, which would be the last
	// in the chain (ie. CRC TD)
	CyDmaChSetInitialTd(sdDMARxChan, sdDMARxTd[0]);
	CyDmaChSetInitialTd(sdDMATxChan, sdDMATxTd[0]);

	// There is no flow control, so we must ensure we can read the bytes
	// before we start transmitting
	CyDmaChEnable(sdDMARxChan, 1);
	CyDmaChEnable(sdDMATxChan, 1);
}

int
sdReadSectorDMAPoll()
{
	if (rxDMAComplete && txDMAComplete)
	{
		// DMA transfer is complete
		dmaInProgress = 0;
		return 1;
	}
	else
	{
		return 0;
	}
}

void sdReadSingleSectorDMA(uint32_t lba, uint8_t* outputBuffer)
{
	uint8 v;
	if (!sdDev.ccs)
	{
		lba = lba * SD_SECTOR_SIZE;
	}
	v = sdCommandAndResponse(SD_READ_SINGLE_BLOCK, lba);
	if (v)
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


void sdCompleteRead()
{
	if (dmaInProgress)
	{
		// Not much choice but to wait until we've completed the transfer.
		// Cancelling the transfer can't be done as we have no way to reset
		// the SD card.
		while (!sdReadSectorDMAPoll()) { /* spin */ }
	}

	transfer.inProgress = 0;

	// We cannot send even a single "padding" byte, as we normally would when
	// sending a command.  If we've just finished reading the very last block
	// on the card, then reading an additional dummy byte will just trigger
	// an error condition as we're trying to read past-the-end of the storage
	// device.
	// ie. do not use sdCommandAndResponse here.
	uint8 r1b;
	sdSendCommand(SD_STOP_TRANSMISSION, 0);
	r1b = sdReadResp();

	if (r1b)
	{
		// Try very hard to make sure the transmission stops
		int retries = 255;
		while (r1b && retries)
		{
			r1b = sdCommandAndResponse(SD_STOP_TRANSMISSION, 0);
			retries--;
		}

		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = HARDWARE_ERROR;
		scsiDev.target->sense.asc = UNRECOVERED_READ_ERROR;
		scsiDev.phase = STATUS;
	}

	// R1b has an optional trailing "busy" signal.
	{
		uint8 busy;
		do
		{
			busy = sdSpiByte(0xFF);
		} while (busy == 0);
	}
}

static void sdWaitWriteBusy()
{
	uint8 val;
	do
	{
		val = sdSpiByte(0xFF);
	} while (val != 0xFF);
}

void
sdWriteMultiSectorDMA(uint8_t* outputBuffer)
{
	sdSpiByte(0xFC); // MULTIPLE byte start token

	// Transmit 512 bytes of data and then 2 bytes CRC.
	CyDmaTdSetConfiguration(sdDMATxTd[0], SD_SECTOR_SIZE, sdDMATxTd[1], TD_INC_SRC_ADR);
	CyDmaTdSetAddress(sdDMATxTd[0], LO16((uint32)outputBuffer), LO16((uint32)SDCard_TXDATA_PTR));
	CyDmaTdSetConfiguration(sdDMATxTd[1], 2, CY_DMA_DISABLE_TD, SD_TX_DMA__TD_TERMOUT_EN);
	CyDmaTdSetAddress(sdDMATxTd[1], LO16((uint32)&dummyBuffer), LO16((uint32)SDCard_TXDATA_PTR));

	CyDmaTdSetConfiguration(sdDMARxTd[0], SD_SECTOR_SIZE + 2, CY_DMA_DISABLE_TD, SD_RX_DMA__TD_TERMOUT_EN);
	CyDmaTdSetAddress(sdDMARxTd[0], LO16((uint32)SDCard_RXDATA_PTR), LO16((uint32)&discardBuffer));

	
	dmaInProgress = 1;
	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaClearPendingDrq(sdDMATxChan);
	CyDmaClearPendingDrq(sdDMARxChan);

	txDMAComplete = 0;
	rxDMAComplete = 0;

	// Re-loading the initial TD's here is very important, or else
	// we'll be re-using the last-used TD, which would be the last
	// in the chain (ie. CRC TD)
	CyDmaChSetInitialTd(sdDMARxChan, sdDMARxTd[0]);
	CyDmaChSetInitialTd(sdDMATxChan, sdDMATxTd[0]);

	// There is no flow control, so we must ensure we can read the bytes
	// before we start transmitting
	CyDmaChEnable(sdDMARxChan, 1);
	CyDmaChEnable(sdDMATxChan, 1);
}

int
sdWriteSectorDMAPoll()
{
	if (rxDMAComplete && txDMAComplete)
	{
		uint8_t dataToken = sdSpiByte(0xFF); // Response
		if (dataToken == 0x0FF)
		{
			return 0; // Write has not completed.
		}
		else if (((dataToken & 0x1F) >> 1) != 0x2) // Accepted.
		{
			uint8 r1b, busy;
		
			sdWaitWriteBusy();

			r1b = sdCommandAndResponse(SD_STOP_TRANSMISSION, 0);
			(void) r1b;
			sdSpiByte(0xFF);

			// R1b has an optional trailing "busy" signal.
			do
			{
				busy = sdSpiByte(0xFF);
			} while (busy == 0);

			// Wait for the card to come out of busy.
			sdWaitWriteBusy();

			transfer.inProgress = 0;
			scsiDiskReset();
			sdClearStatus();

			scsiDev.status = CHECK_CONDITION;
			scsiDev.target->sense.code = HARDWARE_ERROR;
			scsiDev.target->sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
			scsiDev.phase = STATUS;
		}
		else
		{
			sdWaitWriteBusy();
		}
		// DMA transfer is complete and the SD card has accepted the write.
		dmaInProgress = 0;

		return 1;
	}
	else
	{
		return 0;
	}
}

void sdCompleteWrite()
{
	if (dmaInProgress)
	{
		// Not much choice but to wait until we've completed the transfer.
		// Cancelling the transfer can't be done as we have no way to reset
		// the SD card.
		while (!sdWriteSectorDMAPoll()) { /* spin */ }
	}
	
	transfer.inProgress = 0;

	uint8 r1, r2;

	sdSpiByte(0xFD); // STOP TOKEN
	// Wait for the card to come out of busy.
	sdWaitWriteBusy();

	r1 = sdCommandAndResponse(13, 0); // send status
	r2 = sdSpiByte(0xFF);
	if (r1 || r2)
	{
		sdClearStatus();
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = HARDWARE_ERROR;
		scsiDev.target->sense.asc = WRITE_ERROR_AUTO_REALLOCATION_FAILED;
		scsiDev.phase = STATUS;
	}
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
	} while ((status != 0) && (diffTime_ms(start, getTime_ms()) < 1000));

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
		(diffTime_ms(start, getTime_ms()) < 1000));

	return (status == 0) && complete;
}

static int sdReadCSD()
{
	uint8 startToken;
	int maxWait, i;
	uint8 buf[16];

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
		buf[i] = sdSpiByte(0xFF);
	}
	sdSpiByte(0xFF); // CRC
	sdSpiByte(0xFF); // CRC

	if ((buf[0] >> 6) == 0x00)
	{
		// CSD version 1
		// C_SIZE in bits [73:62]
		uint32 c_size = (((((uint32)buf[6]) & 0x3) << 16) | (((uint32)buf[7]) << 8) | buf[8]) >> 6;
		uint32 c_mult = (((((uint32)buf[9]) & 0x3) << 8) | ((uint32)buf[0xa])) >> 7;
		uint32 sectorSize = buf[5] & 0x0F;
		sdDev.capacity = ((c_size+1) * ((uint64)1 << (c_mult+2)) * ((uint64)1 << sectorSize)) / SD_SECTOR_SIZE;
	}
	else if ((buf[0] >> 6) == 0x01)
	{
		// CSD version 2
		// C_SIZE in bits [69:48]

		uint32 c_size =
			((((uint32)buf[7]) & 0x3F) << 16) |
			(((uint32)buf[8]) << 8) |
			((uint32)buf[7]);
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
				1, // Bytes per burst
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

		sdDMARxTd[0] = CyDmaTdAllocate();
		sdDMARxTd[1] = CyDmaTdAllocate();
		sdDMATxTd[0] = CyDmaTdAllocate();
		sdDMATxTd[1] = CyDmaTdAllocate();

		SD_RX_DMA_COMPLETE_StartEx(sdRxISR);
		SD_TX_DMA_COMPLETE_StartEx(sdTxISR);
	}
}

int sdInit()
{
	int result = 0;
	int i;
	uint8 v;

	sdDev.version = 0;
	sdDev.ccs = 0;
	sdDev.capacity = 0;

	sdInitDMA();

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

	v = sdCRCCommandAndResponse(SD_GO_IDLE_STATE, 0);
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

	result = 1;
	goto out;

bad:
	sdDev.capacity = 0;

out:
	sdClearStatus();
	ledOff();
	return result;

}

void sdWriteMultiSectorPrep()
{
	uint8 v;
	
	// Set the number of blocks to pre-erase by the multiple block write command
	// We don't care about the response - if the command is not accepted, writes
	// will just be a bit slower.
	// Max 22bit parameter.
	uint32_t sdBlocks =
		transfer.blocks * SDSectorsPerSCSISector(scsiDev.target->cfg);
	uint32 blocks = sdBlocks > 0x7FFFFF ? 0x7FFFFF : sdBlocks;
	sdCommandAndResponse(SD_APP_CMD, 0);
	sdCommandAndResponse(SD_APP_SET_WR_BLK_ERASE_COUNT, blocks);

	uint32 scsiLBA = (transfer.lba + transfer.currentBlock);
	uint32 sdLBA = SCSISector2SD(scsiDev.target->cfg, scsiLBA);
	if (!sdDev.ccs)
	{
		sdLBA = sdLBA * SD_SECTOR_SIZE;
	}
	v = sdCommandAndResponse(25, sdLBA);
	if (v)
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
		transfer.inProgress = 1;
	}
}

