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

#include <string.h>

// Global
BlockDevice blockDev;
Transfer transfer;

static int doSdInit()
{
	int result = sdInit();
	if (result)
	{
		blockDev.state = blockDev.state | DISK_INITIALISED;
		
		// artificially limit this value according to EEPROM config.
		blockDev.capacity =
			(config->maxBlocks && (sdDev.capacity > config->maxBlocks))
				? config->maxBlocks : sdDev.capacity;
	}
	return result;
}


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

		sdPrepareWrite();
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
		sdPrepareRead();
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
				doSdInit();
			}
		}
		else
		{
			blockDev.state &= ~DISK_STARTED;
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

void scsiDiskPoll()
{
	if (scsiDev.phase == DATA_IN &&
		transfer.currentBlock != transfer.blocks)
	{
		if (scsiDev.dataLen == 0)
		{
			sdReadSector();
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
				sdCompleteRead();
			}
		}
	}
	else if (scsiDev.phase == DATA_OUT &&
		transfer.currentBlock != transfer.blocks)
	{
		if (scsiDev.dataPtr == SCSI_BLOCK_SIZE)
		{
			int writeOk = sdWriteSector();
			scsiDev.dataPtr = 0;
			transfer.currentBlock++;
			if (transfer.currentBlock >= transfer.blocks)
			{
				scsiDev.dataLen = 0;
				scsiDev.phase = STATUS;
				scsiDiskReset();

				if (writeOk)
				{
					sdCompleteWrite();
				}
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
	// WP pin not available for micro-sd
	// TODO read card WP register
	#if 0
	if (SD_WP_Read())
	{
		blockDev.state = blockDev.state | DISK_WP;
	}
	#endif

	if (SD_CD_Read() == 1)
	{
		blockDev.state = blockDev.state | DISK_PRESENT;

		// Wait up to 5 seconds for the SD card to wake up.
		int retry;
		for (retry = 0; retry < 5; ++retry)
		{
			if (doSdInit())
			{
				break;
			}
			else
			{
				CyDelay(1000);
			}
		}
	}
}

