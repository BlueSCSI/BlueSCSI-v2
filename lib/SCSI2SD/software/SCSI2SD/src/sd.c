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

#include "scsiPhy.h"

#include <string.h>

// Global
SdDevice sdDev;

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
	send[5] = 0;

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


void sdPrepareRead()
{
	uint8 v;
	uint32 len = (transfer.lba + transfer.currentBlock);
	if (!sdDev.ccs)
	{
		len = len * SCSI_BLOCK_SIZE;
	}
	v = sdCommandAndResponse(SD_READ_MULTIPLE_BLOCK, len);
	if (v)
	{
		scsiDiskReset();
		sdClearStatus();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
	else
	{
		transfer.inProgress = 1;
	}
}

static void doReadSector()
{
	int prep, i, guard;

	// Wait for a start-block token.
	// Don't wait more than 100ms, which is the timeout recommended
	// in the standard.
	//100ms @ 64Hz = 6400000
	int maxWait = 6400000;
	uint8 token = sdSpiByte(0xFF);
	while (token != 0xFE && (maxWait-- > 0))
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
			scsiDev.sense.code = HARDWARE_ERROR;
			scsiDev.sense.asc = UNRECOVERED_READ_ERROR;
			scsiDev.phase = STATUS;
		}
		return;
	}

	// Don't do a bus settle delay if we're already in the correct phase.
	if (transfer.currentBlock == 0)
	{
		//scsiEnterPhase(DATA_OUT);
		//CyDelayUs(200);
		scsiEnterPhase(DATA_IN);
		//CyDelayUs(200); // TODO BLOODY SLOW INTERLEAVE
	}
	
	// Quickly seed the FIFO
	prep = 4;
	CY_SET_REG8(SDCard_TXDATA_PTR, 0xFF); // Put a byte in the FIFO
	CY_SET_REG8(SDCard_TXDATA_PTR, 0xFF); // Put a byte in the FIFO
	CY_SET_REG8(SDCard_TXDATA_PTR, 0xFF); // Put a byte in the FIFO
	CY_SET_REG8(SDCard_TXDATA_PTR, 0xFF); // Put a byte in the FIFO

	i = 0;
	guard = 0;

	// This loop is critically important for performance.
	// We stream data straight from the SDCard fifos into the SCSI component
	// FIFO's. If the loop isn't fast enough, the transmit FIFO's will empty,
	// and performance will suffer. Every clock cycle counts.
	while (i < SCSI_BLOCK_SIZE && !scsiDev.resetFlag)
	{
		uint8_t sdRxStatus = CY_GET_REG8(SDCard_RX_STATUS_PTR);
		uint8_t scsiStatus = CY_GET_REG8(scsiTarget_StatusReg__STATUS_REG);

		// Read from the SPIM fifo if there is room to stream the byte to the
		// SCSI fifos
		if((sdRxStatus & SDCard_STS_RX_FIFO_NOT_EMPTY) &&
			(scsiDev.resetFlag || (scsiStatus & 1)) // SCSI TX FIFO NOT FULL
			)
		{
			uint8_t val = CY_GET_REG8(SDCard_RXDATA_PTR);
			CY_SET_REG8(scsiTarget_datapath__F0_REG, val);
			guard++;
		}

		// Byte has been sent out the SCSI interface.
		if (scsiDev.resetFlag || (scsiStatus & 2)) // SCSI RX FIFO NOT EMPTY
		{
			CY_GET_REG8(scsiTarget_datapath__F1_REG);
			++i;
		}

		// How many bytes are in a 4-byte FIFO ? 5.  4 FIFO bytes PLUS one byte
		// being processed bit-by-bit. Artifically limit the number of bytes in the 
		// "combined" SPIM TX and RX FIFOS to the individual FIFO size.
		// Unlike the SCSI component, SPIM doesn't check if there's room in
		// the output FIFO before starting to transmit.
		if ((prep - guard < 4) && (prep < SCSI_BLOCK_SIZE))
		{
			CY_SET_REG8(SDCard_TXDATA_PTR, 0xFF); // Put a byte in the FIFO
			prep++;
		}
	}

	sdSpiByte(0xFF); // CRC
	sdSpiByte(0xFF); // CRC
	scsiDev.dataLen = SCSI_BLOCK_SIZE;
	scsiDev.dataPtr = SCSI_BLOCK_SIZE;
	
	while (SCSI_ReadPin(SCSI_In_ACK) && !scsiDev.resetFlag) {}
}

void sdReadSectorSingle()
{
	uint8 v;
	uint32 len = (transfer.lba + transfer.currentBlock);
	if (!sdDev.ccs)
	{
		len = len * SCSI_BLOCK_SIZE;
	}	
	v = sdCommandAndResponse(SD_READ_SINGLE_BLOCK, len);
	if (v)
	{
		scsiDiskReset();
		sdClearStatus();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
	else
	{
		doReadSector();
	}
}

void sdReadSectorMulti()
{
	// Pre: sdPrepareRead called.
	
	doReadSector();
}


void sdCompleteRead()
{
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
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = UNRECOVERED_READ_ERROR;
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

int sdWriteSector()
{
	int prep, i, guard;
	int result, maxWait;
	uint8 dataToken;

	// Don't do a bus settle delay if we're already in the correct phase.
	if (transfer.currentBlock == 0)
	{
		scsiEnterPhase(DATA_OUT);
	}
	
	sdSpiByte(0xFC); // MULTIPLE byte start token
	
	prep = 0;
	i = 0;
	guard = 0;

	// This loop is critically important for performance.
	// We stream data straight from the SCSI fifos into the SPIM component
	// FIFO's. If the loop isn't fast enough, the transmit FIFO's will empty,
	// and performance will suffer. Every clock cycle counts.	
	while (i < SCSI_BLOCK_SIZE)
	{
		uint8_t sdRxStatus = CY_GET_REG8(SDCard_RX_STATUS_PTR);
		uint8_t scsiStatus = CY_GET_REG8(scsiTarget_StatusReg__STATUS_REG);

		// Read from the SCSI fifo if there is room to stream the byte to the
		// SPIM fifos
		// See sdReadSector for comment on guard (FIFO size is really 5)
		if((guard - i < 4) &&
			(scsiDev.resetFlag || (scsiStatus & 2))
			) // SCSI RX FIFO NOT EMPTY
		{
			uint8_t val = CY_GET_REG8(scsiTarget_datapath__F1_REG);
			CY_SET_REG8(SDCard_TXDATA_PTR, val);
			guard++;
		}

		// Byte has been sent out the SPIM interface.
		if (sdRxStatus & SDCard_STS_RX_FIFO_NOT_EMPTY)
		{
			 CY_GET_REG8(SDCard_RXDATA_PTR);
			++i;
		}

		if (prep < SCSI_BLOCK_SIZE &&
			(scsiDev.resetFlag || (scsiStatus & 1)) // SCSI TX FIFO NOT FULL
			)
		{
			// Trigger the SCSI component to read a byte
			CY_SET_REG8(scsiTarget_datapath__F0_REG, 0xFF);
			prep++;
		}
	}
	
	sdSpiByte(0x00); // CRC
	sdSpiByte(0x00); // CRC

	// Don't wait more than 1s.
	// My 2g Kingston micro-sd card doesn't respond immediately.
	// My 16Gb card does.
	maxWait = 1000000;
	dataToken = sdSpiByte(0xFF); // Response
	while (dataToken == 0xFF && maxWait-- > 0)
	{
		CyDelayUs(1);
		dataToken = sdSpiByte(0xFF);
	}
	if (((dataToken & 0x1F) >> 1) != 0x2) // Accepted.
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
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
		result = 0;
	}
	else
	{
		sdWaitWriteBusy();
		result = 1;
	}

	while (SCSI_ReadPin(SCSI_In_ACK) && !scsiDev.resetFlag) {}

	return result;
}

void sdCompleteWrite()
{
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
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = WRITE_ERROR_AUTO_REALLOCATION_FAILED;
		scsiDev.phase = STATUS;
	}
}


// SD Version 2 (SDHC) support
static int sendIfCond()
{
	int retries = 50;

	do
	{
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
	int retries = 50;

	uint8 status;
	do
	{
		CyDelay(33); // Spec says to retry for 1 second.

		sdCRCCommandAndResponse(SD_APP_CMD, 0);
		// Host Capacity Support = 1 (SDHC/SDXC supported)
		status = sdCRCCommandAndResponse(SD_APP_SEND_OP_COND, 0x40000000);

		sdClearStatus();
	} while ((status != 0) && (--retries > 0));

	return retries > 0;
}

static int sdReadOCR()
{
	uint8 buf[4];
	int i;
	
	uint8 status = sdCRCCommandAndResponse(SD_READ_OCR, 0);
	if(status){goto bad;}

	for (i = 0; i < 4; ++i)
	{
		buf[i] = sdSpiByte(0xFF);
	}

	sdDev.ccs = (buf[0] & 0x40) ? 1 : 0;

	return 1;
bad:
	return 0;
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
		sdDev.capacity = ((c_size+1) * ((uint64)1 << (c_mult+2)) * ((uint64)1 << sectorSize)) / SCSI_BLOCK_SIZE;
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

int sdInit()
{
	int result = 0;
	int i;
	uint8 v;
	
	sdDev.version = 0;
	sdDev.ccs = 0;
	sdDev.capacity = 0;

	SD_CS_Write(1); // Set CS inactive (active low)
	SD_Init_Clk_Start(); // Turn on the slow 400KHz clock
	SD_Clk_Ctl_Write(0); // Select the 400KHz clock source.
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
	if (!sendIfCond()) goto bad; // Sets V1 or V2 flag
	if (!sdOpCond()) goto bad;
	if (!sdReadOCR()) goto bad;

	// This command will be ignored if sdDev.ccs is set.
	// SDHC and SDXC are always 512bytes.
	v = sdCRCCommandAndResponse(SD_SET_BLOCKLEN, SCSI_BLOCK_SIZE); //Force sector size
	if(v){goto bad;}
	v = sdCRCCommandAndResponse(SD_CRC_ON_OFF, 0); //crc off
	if(v){goto bad;}

	// now set the sd card up for full speed
	// The SD Card spec says we can run SPI @ 25MHz
	// But the PSoC 5LP SPIM datasheet says the most we can do is 18MHz.
	// I've confirmed that no data is ever put into the RX FIFO when run at
	// 20MHz or 25MHz.
	// ... and then we get timing analysis failures if the BUS_CLK is over 62MHz.
	// So we run the MASTER_CLK and BUS_CLK at 60MHz, and run the SPI clock at 30MHz
	// (15MHz SPI transfer clock).
	SDCard_Stop();
	
	// We can't run at full-speed with the pullup resistors enabled.
	SD_MISO_SetDriveMode(SD_MISO_DM_DIG_HIZ);
	SD_MOSI_SetDriveMode(SD_MOSI_DM_STRONG);
	SD_SCK_SetDriveMode(SD_SCK_DM_STRONG);
	
	SD_Data_Clk_Start(); // Turn on the fast clock
	SD_Clk_Ctl_Write(1); // Select the fast clock source.
	SD_Init_Clk_Stop(); // Stop the slow clock.
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

void sdPrepareWrite()
{
	uint32 len;
	uint8 v;
	
	// Set the number of blocks to pre-erase by the multiple block write command
	// We don't care about the response - if the command is not accepted, writes
	// will just be a bit slower.
	// Max 22bit parameter.
	uint32 blocks = transfer.blocks > 0x7FFFFF ? 0x7FFFFF : transfer.blocks;
	sdCommandAndResponse(SD_APP_CMD, 0);
	sdCommandAndResponse(SD_APP_SET_WR_BLK_ERASE_COUNT, blocks);

	len = (transfer.lba + transfer.currentBlock);
	if (!sdDev.ccs)
	{
		len = len * SCSI_BLOCK_SIZE;
	}
	v = sdCommandAndResponse(25, len);
	if (v)
	{
		scsiDiskReset();
		sdClearStatus();
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
	else
	{
		transfer.inProgress = 1;
	}
}

