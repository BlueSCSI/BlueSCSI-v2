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
#include "disk.h"
#include "sd.h"

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
	while(!(SDCard_ReadTxStatus() & SDCard_STS_SPI_DONE))
	{}
	while (!SDCard_GetRxBufferSize()) {}
	return SDCard_ReadRxData();
}

static void sdSendCommand(uint8 cmd, uint32 param)
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
}

static uint8 sdReadResp()
{
	uint8 v;
	uint8 i = 128;
	do
	{
		v = sdSpiByte(0xFF);
	} while(i-- && (v == 0xFF));
	return v;
}

static uint8 sdWaitResp()
{
	uint8 v;
	uint8 i = 255;
	do
	{
		v = sdSpiByte(0xFF);
	} while(i-- && (v != 0xFE));
	return v;
}


static uint8 sdCommandAndResponse(uint8 cmd, uint32 param)
{
	SDCard_ClearRxBuffer();
	sdSpiByte(0xFF);
	sdSendCommand(cmd, param);
	return sdReadResp();
}


void sdPrepareRead(int nextBlockOffset)
{
	uint32 len = (transfer.lba + transfer.currentBlock + nextBlockOffset);
	if (!sdDev.ccs)
	{
		len = len * SCSI_BLOCK_SIZE;
	}
	uint8 v = sdCommandAndResponse(17, len);
	if (v)
	{
		scsiDiskReset();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
}

int sdIsReadReady()
{
	uint8 v = sdWaitResp();
	if (v == 0xFF)
	{
		return 0;
	}
	else if (v == 0xFE)
	{
		return 1;
	}
	else
	{
		scsiDiskReset();
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
		return 0;
	}
}

void sdReadSector()
{
	// We have a spi FIFO of 4 bytes. use it.
	// This is much better, byut after 4 bytes we're still
	// blocking a bit.
	int i;
	for (i = 0; i < SCSI_BLOCK_SIZE; i+=4)
	{
		SDCard_WriteTxData(0xFF);
		SDCard_WriteTxData(0xFF);
		SDCard_WriteTxData(0xFF);
		SDCard_WriteTxData(0xFF);

		while(!(SDCard_ReadTxStatus() & SDCard_STS_SPI_DONE))
		{}
		scsiDev.data[i] = SDCard_ReadRxData();
		scsiDev.data[i+1] = SDCard_ReadRxData();
		scsiDev.data[i+2] = SDCard_ReadRxData();
		scsiDev.data[i+3] = SDCard_ReadRxData();

	}


	sdSpiByte(0xFF); // CRC
	sdSpiByte(0xFF); // CRC
	scsiDev.dataLen = SCSI_BLOCK_SIZE;
	scsiDev.dataPtr = 0;
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
	int result;
	// Wait for a previously-written sector to complete.
	sdWaitWriteBusy();

		sdSpiByte(0xFC); // MULTIPLE byte start token
		int i;
		for (i = 0; i < SCSI_BLOCK_SIZE; i+=4)
		{
			SDCard_WriteTxData(scsiDev.data[i]);
			SDCard_WriteTxData(scsiDev.data[i+1]);
			SDCard_WriteTxData(scsiDev.data[i+2]);
			SDCard_WriteTxData(scsiDev.data[i+3]);

			while(!(SDCard_ReadTxStatus() & SDCard_STS_SPI_DONE))
			{}
		
			SDCard_ReadRxData();
			SDCard_ReadRxData();
			SDCard_ReadRxData();
			SDCard_ReadRxData();
		}

		sdSpiByte(0x00); // CRC
		sdSpiByte(0x00); // CRC
		uint8 dataToken = sdSpiByte(0xFF); // Response
		if (((dataToken & 0x1F) >> 1) != 0x2) // Accepted.
		{
			sdWaitWriteBusy();
			sdSpiByte(0xFD); // STOP TOKEN
			// Wait for the card to come out of busy.
			sdWaitWriteBusy();

			scsiDiskReset();

			scsiDev.status = CHECK_CONDITION;
			scsiDev.sense.code = HARDWARE_ERROR;
			scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
			scsiDev.phase = STATUS;
			result = 0;
		}
		else
		{
			// The card is probably in the busy state.
			// Don't wait, as we could read the SCSI interface instead.
			result = 1;
	}

	return result;
}

void sdCompleteWrite()
{
	// Wait for a previously-written sector to complete.
	sdWaitWriteBusy();

	sdSpiByte(0xFD); // STOP TOKEN
	// Wait for the card to come out of busy.
	sdWaitWriteBusy();
	
	uint8 r1 = sdCommandAndResponse(13, 0); // send status
	uint8 r2 = sdSpiByte(0xFF);
	if (r1 || r2)
	{
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
		uint8 status = sdCommandAndResponse(SD_SEND_IF_COND, 0x000001AA);

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
			break;
		}
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

		sdCommandAndResponse(SD_APP_CMD, 0);
		// Host Capacity Support = 1 (SDHC/SDXC supported)
		status = sdCommandAndResponse(SD_APP_SEND_OP_COND, 0x40000000);
	} while ((status != 0) && (--retries > 0));

	return retries > 0;
}

static int sdReadOCR()
{
	uint8 status = sdCommandAndResponse(SD_READ_OCR, 0);
	if(status){goto bad;}

	uint8 buf[4];
	int i;
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
	uint8 status = sdCommandAndResponse(SD_SEND_CSD, 0);
	if(status){goto bad;}
	status = sdWaitResp();
	if (status != 0xFE) { goto bad; }

	uint8 buf[16];
	int i;
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
	sdDev.version = 0;
	sdDev.ccs = 0;
	sdDev.capacity = 0;

	int result = 0;
	SD_CS_Write(1); // Set CS inactive (active low)
	SD_Init_Clk_Start(); // Turn on the slow 400KHz clock
	SD_Clk_Ctl_Write(0); // Select the 400KHz clock source.
	SDCard_Start(); // Enable SPI hardware

	// Power on sequence. 74 clock cycles of a "1" while CS unasserted.
	int i;
	for (i = 0; i < 10; ++i)
	{
		sdSpiByte(0xFF);
	}

	SD_CS_Write(0); // Set CS active (active low)
	CyDelayUs(1);

	uint8 v = sdCommandAndResponse(SD_GO_IDLE_STATE, 0);
	if(v != 1){goto bad;}

	if (!sendIfCond()) goto bad; // Sets V1 or V2 flag
	if (!sdOpCond()) goto bad;
	if (!sdReadOCR()) goto bad;

	// This command will be ignored if sdDev.ccs is set.
	// SDHC and SDXC are always 512bytes.
	v = sdCommandAndResponse(SD_SET_BLOCKLEN, SCSI_BLOCK_SIZE); //Force sector size
	if(v){goto bad;}
	v = sdCommandAndResponse(SD_CRC_ON_OFF, 0); //crc off
	if(v){goto bad;}

	// now set the sd card up for full speed
	SD_Data_Clk_Start(); // Turn on the fast clock
	SD_Clk_Ctl_Write(1); // Select the fast clock source.
	SD_Init_Clk_Stop(); // Stop the slow clock.

	if (!sdReadCSD()) goto bad;

	result = 1;
	goto out;

bad:
	sdDev.capacity = 0;

out:
	return result;

}


void sdPrepareWrite()
{
	// Set the number of blocks to pre-erase by the multiple block write command
	// We don't care about the response - if the command is not accepted, writes
	// will just be a bit slower.
	// Max 22bit parameter.
	uint32 blocks = transfer.blocks > 0x7FFFFF ? 0x7FFFFF : transfer.blocks;
	sdCommandAndResponse(SD_APP_CMD, 0);
	sdCommandAndResponse(SD_APP_SET_WR_BLK_ERASE_COUNT, blocks);

	uint32 len = (transfer.lba + transfer.currentBlock);
	if (!sdDev.ccs)
	{
		len = len * SCSI_BLOCK_SIZE;
	}
	uint8 v = sdCommandAndResponse(25, len);
	if (v)
	{
		scsiDiskReset();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
}

