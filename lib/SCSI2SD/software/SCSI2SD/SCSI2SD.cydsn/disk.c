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

#include <string.h>

// Global
BlockDevice blockDev;
Transfer transfer;

static void startRead(int nextBlock);
static int sdInit();

static void doFormatUnit()
{
	// Low-level formatting is not required.
	// Nothing left to do.
}

static void doReadCapacity()
{
	uint32 lba = (((uint32) scsiDev.cdb[2]) << 24) +
		(((uint32) scsiDev.cdb[3]) << 16) +
		(((uint32) scsiDev.cdb[4]) << 8) +
		scsiDev.cdb[5];
	int pmi = scsiDev.cdb[8] & 1;

	if (!pmi && lba)
	{
		// error.
		// We don't do anything with the "partial medium indicator", and
		// assume that delays are constant across each block. But the spec
		// says we must return this error if pmi is specified incorrectly.
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else if (blockDev.capacity > 0)
	{
		uint32 highestBlock = blockDev.capacity - 1;

		scsiDev.data[0] = highestBlock >> 24;
		scsiDev.data[1] = highestBlock >> 16;
		scsiDev.data[2] = highestBlock >> 8;
		scsiDev.data[3] = highestBlock;

		scsiDev.data[4] = blockDev.bs >> 24;
		scsiDev.data[5] = blockDev.bs >> 16;
		scsiDev.data[6] = blockDev.bs >> 8;
		scsiDev.data[7] = blockDev.bs;
		scsiDev.dataLen = 8;
		scsiDev.phase = DATA_IN;
	}
	else
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = NOT_READY;
		scsiDev.sense.asc = MEDIUM_NOT_PRESENT;
		scsiDev.phase = STATUS;
	}
}

static void doWrite(uint32 lba, uint32 blocks)
{
	if (blockDev.state & DISK_WP)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = WRITE_PROTECTED;
		scsiDev.phase = STATUS;	
	}
	else if (((uint64) lba) + blocks > blockDev.capacity)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		scsiDev.phase = STATUS;
	}
	else
	{
		transfer.dir = TRANSFER_WRITE;
		transfer.lba = lba;
		transfer.blocks = blocks;
		transfer.currentBlock = 0;
		scsiDev.phase = DATA_OUT;
		scsiDev.dataLen = SCSI_BLOCK_SIZE;
	}
}


static void doRead(uint32 lba, uint32 blocks)
{
	if (((uint64) lba) + blocks > blockDev.capacity)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		scsiDev.phase = STATUS;
	}
	else
	{
		transfer.dir = TRANSFER_READ;
		transfer.lba = lba;
		transfer.blocks = blocks;
		transfer.currentBlock = 0;
		scsiDev.phase = DATA_IN;
		scsiDev.dataLen = 0; // No data yet
		startRead(0);
	}
}

static void doSeek(uint32 lba)
{
	if (lba >= blockDev.capacity)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		scsiDev.phase = STATUS;
	}
}

static int doTestUnitReady()
{
	int ready = 1;
	if (!(blockDev.state & DISK_STARTED))
	{
		ready = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = NOT_READY;
		scsiDev.sense.asc = LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED;
		scsiDev.phase = STATUS;
	}
	else if (!(blockDev.state & DISK_PRESENT))
	{
		ready = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = NOT_READY;
		scsiDev.sense.asc = MEDIUM_NOT_PRESENT;
		scsiDev.phase = STATUS;
	}
	else if (!(blockDev.state & DISK_INITIALISED))
	{
		ready = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = NOT_READY;
		scsiDev.sense.asc = LOGICAL_UNIT_NOT_READY_CAUSE_NOT_REPORTABLE;
		scsiDev.phase = STATUS;
	}
	return ready;
}

// Handle direct-access scsi device commands
int scsiDiskCommand()
{
	int commandHandled = 1;

	uint8 command = scsiDev.cdb[0];
	if (command == 0x1B)
	{
		// START STOP UNIT
		// Enable or disable media access operations.
		// Ignore load/eject requests. We can't do that.
		//int immed = scsiDev.cdb[1] & 1;
		int start = scsiDev.cdb[4] & 1;

		if (start)
		{
			blockDev.state = blockDev.state | DISK_STARTED;
			if (!(blockDev.state & DISK_INITIALISED))
			{
				if (sdInit())
				{
					blockDev.state = blockDev.state | DISK_INITIALISED;
				}
			}
		}
		else
		{
			blockDev.state = blockDev.state & (-1 ^ DISK_STARTED);
		}
	}
	else if (command == 0x00)
	{
		// TEST UNIT READY
		doTestUnitReady();
	}
	else if (!doTestUnitReady())
	{
		// Status and sense codes already set by doTestUnitReady
	}
	else if (command == 0x04)
	{
		// FORMAT UNIT
		doFormatUnit();
	}
	else if (command == 0x08)
	{
		// READ(6)
		uint32 lba =
			(((uint32) scsiDev.cdb[1] & 0x1F) << 16) +
			(((uint32) scsiDev.cdb[2]) << 8) +
			scsiDev.cdb[3];
		uint32 blocks = scsiDev.cdb[4];
		if (blocks == 0) blocks = 256;
		doRead(lba, blocks);
	}

	else if (command == 0x28)
	{
		// READ(10)
		// Ignore all cache control bits - we don't support a memory cache.

		uint32 lba =
			(((uint32) scsiDev.cdb[2]) << 24) +
			(((uint32) scsiDev.cdb[3]) << 16) +
			(((uint32) scsiDev.cdb[4]) << 8) +
			scsiDev.cdb[5];
		uint32 blocks =
			(((uint32) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];

		doRead(lba, blocks);
	}

	else if (command == 0x25)
	{
		// READ CAPACITY
		doReadCapacity();
	}

	else if (command == 0x0B)
	{
		// SEEK(6)
		uint32 lba =
			(((uint32) scsiDev.cdb[1] & 0x1F) << 16) +
			(((uint32) scsiDev.cdb[2]) << 8) +
			scsiDev.cdb[3];

		doSeek(lba);
	}

	else if (command == 0x2B)
	{
		// SEEK(10)
		uint32 lba =
			(((uint32) scsiDev.cdb[2]) << 24) +
			(((uint32) scsiDev.cdb[3]) << 16) +
			(((uint32) scsiDev.cdb[4]) << 8) +
			scsiDev.cdb[5];

		doSeek(lba);
	}
	else if (command == 0x0A)
	{
		// WRITE(6)
		uint32 lba =
			(((uint32) scsiDev.cdb[1] & 0x1F) << 16) +
			(((uint32) scsiDev.cdb[2]) << 8) +
			scsiDev.cdb[3];
		uint32 blocks = scsiDev.cdb[4];
		if (blocks == 0) blocks = 256;
		doWrite(lba, blocks);
	}

	else if (command == 0x2A)
	{
		// WRITE(10)
		// Ignore all cache control bits - we don't support a memory cache.

		uint32 lba =
			(((uint32) scsiDev.cdb[2]) << 24) +
			(((uint32) scsiDev.cdb[3]) << 16) +
			(((uint32) scsiDev.cdb[4]) << 8) +
			scsiDev.cdb[5];
		uint32 blocks =
			(((uint32) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];

		doWrite(lba, blocks);
	}
	else if (command == 0x36)
	{
		// LOCK UNLOCK CACHE
		// We don't have a cache to lock data into. do nothing.
	}
	else if (command == 0x34)
	{
		// PRE-FETCH.
		// We don't have a cache to pre-fetch into. do nothing.
	}
	else if (command == 0x1E)
	{
		// PREVENT ALLOW MEDIUM REMOVAL
		// Not much we can do to prevent the user removing the SD card.
		// do nothing.
	}
	else if (command == 0x01)
	{
		// REZERO UNIT
		// Set the lun to a vendor-specific state. Ignore.
	}
	else if (command == 0x35)
	{
		// SYNCHRONIZE CACHE
		// We don't have a cache. do nothing.
	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
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


static int sdInit()
{
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

	uint8 v = sdCommandAndResponse(0, 0);
	if(v != 1){goto bad;}

	// TODO CMD8 + valid CC for ver2 + cards. arg 0x00..01AA


	// TODO SDv2 support: ACMD41, fallback to CMD1

	v = sdCommandAndResponse(1, 0);
     for(i=0;v != 0 && i<50;++i){
          CyDelay(50);
          v = sdCommandAndResponse(1, 0);
     }
     if(v){goto bad;}
     
	v = sdCommandAndResponse(16, SCSI_BLOCK_SIZE); //Force sector size
	if(v){goto bad;}
	v = sdCommandAndResponse(59, 0); //crc off
	if(v){goto bad;}

	// now set the sd card up for full speed
	SD_Data_Clk_Start(); // Turn on the fast clock
	SD_Clk_Ctl_Write(1); // Select the fast clock source.
	SD_Init_Clk_Stop(); // Stop the slow clock.
	
	v = sdCommandAndResponse(0x9, 0);
	if(v){goto bad;}
	v = sdWaitResp();
	if (v != 0xFE) { goto bad; }
	uint8 buf[16];
	for (i = 0; i < 16; ++i)
	{
		buf[i] = sdSpiByte(0xFF);
	}
	sdSpiByte(0xFF); // CRC 
	sdSpiByte(0xFF); // CRC
	uint32 c_size = (((((uint32)buf[6]) & 0x3) << 16) | (((uint32)buf[7]) << 8) | buf[8]) >> 6;
	uint32 c_mult = (((((uint32)buf[9]) & 0x3) << 8) | ((uint32)buf[0xa])) >> 7;
	uint32 sectorSize = buf[5] & 0x0F;
	blockDev.capacity = ((c_size+1) * ((uint64)1 << (c_mult+2)) * ((uint64)1 << sectorSize)) / SCSI_BLOCK_SIZE;
	result = 1;
	goto out;
	
bad:
	blockDev.capacity = 0;
	
out:
	return result;

}

static void startRead(int nextBlock)
{
// TODO 4Gb limit
// NOTE: CMD17 is NOT in hex. decimal 17.
	uint8 v = sdCommandAndResponse(17, ((uint32)SCSI_BLOCK_SIZE) * (transfer.lba + transfer.currentBlock + nextBlock));
	if (v)
	{
		scsiDiskReset();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
}

static int readReady()
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
static void readSector()
{
// TODO this is slow. Really slow.
// Even if we don't use DMA, we still want to read/write multiple bytes
// at a time.
/*
	int i;
	for (i = 0; i < SCSI_BLOCK_SIZE; ++i)
	{
		scsiDev.data[i] = sdSpiByte(0xFF);
	}
*/

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

static void writeSector()
{
	uint8 v = sdCommandAndResponse(24, ((uint32)SCSI_BLOCK_SIZE) * (transfer.lba + transfer.currentBlock));
	if (v)
	{
		scsiDiskReset();

		scsiDev.status = CHECK_CONDITION;
		scsiDev.sense.code = HARDWARE_ERROR;
		scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
		scsiDev.phase = STATUS;
	}
	else
	{
		SDCard_WriteTxData(0xFE);
		int i;
		for (i = 0; i < SCSI_BLOCK_SIZE; ++i)
		{
			SDCard_WriteTxData(scsiDev.data[i]);
		}
		while(!(SDCard_ReadTxStatus() & SDCard_STS_SPI_DONE))
		{}
		sdSpiByte(0x00); // CRC
		sdSpiByte(0x00); // CRC
		SDCard_ClearRxBuffer();		
		v = sdSpiByte(0x00); // Response
		if (((v & 0x1F) >> 1) != 0x2) // Accepted.
		{
			scsiDiskReset();

			scsiDev.status = CHECK_CONDITION;
			scsiDev.sense.code = HARDWARE_ERROR;
			scsiDev.sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
			scsiDev.phase = STATUS;
		}
		else
		{
			// Wait for the card to come out of busy.
			v = sdSpiByte(0xFF);
			while (v == 0)
			{
				v = sdSpiByte(0xFF);
			}
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
	}
}

void scsiDiskPoll()
{
	if (scsiDev.phase == DATA_IN &&
		transfer.currentBlock != transfer.blocks)
	{
		if (scsiDev.dataLen == 0)
		{
			if (readReady())
			{
				readSector();
				if ((transfer.currentBlock + 1) < transfer.blocks)
				{
					startRead(1); // Tell SD card to grab data while we send
									// buffer to SCSI.
				}
			}
		}
		else if (scsiDev.dataPtr == scsiDev.dataLen)
		{
			scsiDev.dataLen = 0;
			scsiDev.dataPtr = 0;
			transfer.currentBlock++;
			if (transfer.currentBlock >= transfer.blocks)
			{
				scsiDev.phase = STATUS;
				scsiDiskReset();
			}
		}
	}
	else if (scsiDev.phase == DATA_OUT &&
		transfer.currentBlock != transfer.blocks)
	{
		if (scsiDev.dataPtr == SCSI_BLOCK_SIZE)
		{
			writeSector();
			scsiDev.dataPtr = 0;
			transfer.currentBlock++;
			if (transfer.currentBlock >= transfer.blocks)
			{
				scsiDev.dataLen = 0;
				scsiDev.phase = STATUS;
				scsiDiskReset();
			}
		}
	}	
}

void scsiDiskReset()
{
 // todo if SPI command in progress, cancel it.
	scsiDev.dataPtr = 0;
	scsiDev.savedDataPtr = 0;
	scsiDev.dataLen = 0;
	transfer.lba = 0;
	transfer.blocks = 0;
	transfer.currentBlock = 0;
}

void scsiDiskInit()
{
	blockDev.bs = SCSI_BLOCK_SIZE;
	blockDev.capacity = 0;
	scsiDiskReset();

	// Don't require the host to send us a START STOP UNIT command
	blockDev.state = DISK_STARTED;
	if (SD_WP_Read())
	{
		blockDev.state = blockDev.state | DISK_WP;
	}

	if (SD_CD_Read() == 0)
	{
		blockDev.state = blockDev.state | DISK_PRESENT;

// todo IF FAILS, TRY AGAIN LATER.
// 5000 works well with the Mac.
		CyDelay(5000); // allow the card to wake up.
		if (sdInit())
		{
			blockDev.state = blockDev.state | DISK_INITIALISED;
		}
	}
}

