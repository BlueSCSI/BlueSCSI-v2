//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//	Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
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
#include "scsiPhy.h"
#include "config.h"
#include "disk.h"
#include "sd.h"
#include "time.h"

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
	}
	return result;
}

// Callback once all data has been read in the data out phase.
static void doFormatUnitComplete(void)
{
	// TODO start writing the initialisation pattern to the SD
	// card
	scsiDev.phase = STATUS;
}

static void doFormatUnitSkipData(int bytes)
{
	// We may not have enough memory to store the initialisation pattern and
	// defect list data.  Since we're not making use of it yet anyway, just
	// discard the bytes.
	scsiEnterPhase(DATA_OUT);
	int i;
	for (i = 0; i < bytes; ++i)
	{
		scsiReadByte();
	}
}

// Callback from the data out phase.
static void doFormatUnitPatternHeader(void)
{
	int defectLength =
		((((uint16_t)scsiDev.data[2])) << 8) +
			scsiDev.data[3];

	int patternLength =
		((((uint16_t)scsiDev.data[4 + 2])) << 8) +
		scsiDev.data[4 + 3];

		doFormatUnitSkipData(defectLength + patternLength);
		doFormatUnitComplete();
}

// Callback from the data out phase.
static void doFormatUnitHeader(void)
{
	int IP = (scsiDev.data[1] & 0x08) ? 1 : 0;
	int DSP = (scsiDev.data[1] & 0x04) ? 1 : 0;

	if (! DSP) // disable save parameters
	{
		configSave(); // Save the "MODE SELECT savable parameters"
	}
	
	if (IP)
	{
		// We need to read the initialisation pattern header first.
		scsiDev.dataLen += 4;
		scsiDev.phase = DATA_OUT;
		scsiDev.postDataOutHook = doFormatUnitPatternHeader;
	}
	else
	{
		// Read the defect list data
		int defectLength =
			((((uint16_t)scsiDev.data[2])) << 8) +
			scsiDev.data[3];
		doFormatUnitSkipData(defectLength);
		doFormatUnitComplete();
	}
}

static void doReadCapacity()
{
	uint32_t lba = (((uint32) scsiDev.cdb[2]) << 24) +
		(((uint32) scsiDev.cdb[3]) << 16) +
		(((uint32) scsiDev.cdb[4]) << 8) +
		scsiDev.cdb[5];
	int pmi = scsiDev.cdb[8] & 1;

	uint32_t capacity = getScsiCapacity();

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
	else if (capacity > 0)
	{
		uint32_t highestBlock = capacity - 1;

		scsiDev.data[0] = highestBlock >> 24;
		scsiDev.data[1] = highestBlock >> 16;
		scsiDev.data[2] = highestBlock >> 8;
		scsiDev.data[3] = highestBlock;

		scsiDev.data[4] = config->bytesPerSector >> 24;
		scsiDev.data[5] = config->bytesPerSector >> 16;
		scsiDev.data[6] = config->bytesPerSector >> 8;
		scsiDev.data[7] = config->bytesPerSector;
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
	else if (((uint64) lba) + blocks > getScsiCapacity())
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
		scsiDev.dataLen = config->bytesPerSector;
		scsiDev.dataPtr = config->bytesPerSector; // TODO FIX scsiDiskPoll()

		// No need for single-block writes atm.  Overhead of the
		// multi-block write is minimal.
		transfer.multiBlock = 1;
		
		sdWriteMultiSectorPrep();
	}
}


static void doRead(uint32 lba, uint32 blocks)
{
	uint32_t capacity = getScsiCapacity();
	if (((uint64) lba) + blocks > capacity)
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

		if ((blocks == 1) ||
			(((uint64) lba) + blocks == capacity)
			)
		{
			// We get errors on reading the last sector using a multi-sector
			// read :-(
			transfer.multiBlock = 0;
		}
		else
		{
			transfer.multiBlock = 1;
			sdReadMultiSectorPrep();
		}
	}
}

static void doSeek(uint32 lba)
{
	if (lba >= getScsiCapacity())
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
		// We don't really do any formatting, but we need to read the correct
		// number of bytes in the DATA_OUT phase to make the SCSI host happy.
		
		int fmtData = (scsiDev.cdb[1] & 0x10) ? 1 : 0;
		if (fmtData)
		{
			// We need to read the parameter list, but we don't know how
			// big it is yet. Start with the header.
			scsiDev.dataLen = 4;
			scsiDev.phase = DATA_OUT;
			scsiDev.postDataOutHook = doFormatUnitHeader;
		}
		else
		{
			// No data to read, we're already finished!
		}
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
	else if (command == 0x2F)
	{
		// VERIFY
		// TODO: When they supply data to verify, we should read the data and
		// verify it. If they don't supply any data, just say success.
		if ((scsiDev.cdb[1] & 0x02) == 0)
		{
			// They are asking us to do a medium verification with no data
			// comparison. Assume success, do nothing.
		}
		else
		{
			// TODO. This means they are supplying data to verify against.
			// Technically we should probably grab the data and compare it.
			scsiDev.status = CHECK_CONDITION;
			scsiDev.sense.code = ILLEGAL_REQUEST;
			scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}
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
		scsiEnterPhase(DATA_IN);

		int totalSDSectors = transfer.blocks * SDSectorsPerSCSISector();
		uint32_t sdLBA = SCSISector2SD(transfer.lba);
		int buffers = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
		int prep = 0;
		int i = 0;
		int scsiActive = 0;
		int sdActive = 0;
		while ((i < totalSDSectors) &&
			(scsiDev.phase == DATA_IN) &&
			!scsiDev.resetFlag)
		{
			if ((sdActive == 1) && sdReadSectorDMAPoll())
			{
				sdActive = 0;
				prep++;
			}
			else if ((sdActive == 0) && (prep - i < buffers) && (prep < totalSDSectors))
			{
				// Start an SD transfer if we have space.
				if (transfer.multiBlock)
				{
					sdReadMultiSectorDMA(&scsiDev.data[SD_SECTOR_SIZE * (prep % buffers)]);
				}
				else
				{
					sdReadSingleSectorDMA(sdLBA + prep, &scsiDev.data[SD_SECTOR_SIZE * (prep % buffers)]);
				}
				sdActive = 1;
			}

			if ((scsiActive == 1) && scsiWriteDMAPoll())
			{
				scsiActive = 0;
				++i;
			}
			else if ((scsiActive == 0) && ((prep - i) > 0))
			{
				int dmaBytes = SD_SECTOR_SIZE;
				if (i % SDSectorsPerSCSISector() == SDSectorsPerSCSISector() - 1)
				{
					dmaBytes = config->bytesPerSector % SD_SECTOR_SIZE;
					if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
				}
				scsiWriteDMA(&scsiDev.data[SD_SECTOR_SIZE * (i % buffers)], dmaBytes);
				scsiActive = 1;
			}
		}
		if (scsiDev.phase == DATA_IN)
		{
			scsiDev.phase = STATUS;
		}
		scsiDiskReset();
	}
	else if (scsiDev.phase == DATA_OUT &&
		transfer.currentBlock != transfer.blocks)
	{
		scsiEnterPhase(DATA_OUT);

		int totalSDSectors = transfer.blocks * SDSectorsPerSCSISector();
		int buffers = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
		int prep = 0;
		int i = 0;
		int scsiDisconnected = 0;
		volatile uint32_t lastActivityTime = getTime_ms();
		int scsiActive = 0;
		int sdActive = 0;
		
		while ((i < totalSDSectors) &&
			(scsiDev.phase == DATA_OUT) && // scsiDisconnect keeps our phase.
			!scsiDev.resetFlag)
		{
			if ((sdActive == 1) && sdWriteSectorDMAPoll())
			{
				sdActive = 0;
				i++;
			}
			else if ((sdActive == 0) && ((prep - i) > 0))
			{
				// Start an SD transfer if we have space.
				sdWriteMultiSectorDMA(&scsiDev.data[SD_SECTOR_SIZE * (i % buffers)]);
				sdActive = 1;
			}

			if ((scsiActive == 1) && scsiReadDMAPoll())
			{
				scsiActive = 0;
				++prep;
				lastActivityTime = getTime_ms();
			}
			else if ((scsiActive == 0) &&
				((prep - i) < buffers) &&
				(prep < totalSDSectors) &&
				!scsiDisconnected)
			{
				int dmaBytes = SD_SECTOR_SIZE;
				if (prep % SDSectorsPerSCSISector() == SDSectorsPerSCSISector() - 1)
				{
					dmaBytes = config->bytesPerSector % SD_SECTOR_SIZE;
					if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
				}
				scsiReadDMA(&scsiDev.data[SD_SECTOR_SIZE * (prep % buffers)], dmaBytes);
				scsiActive = 1;
			}
			else if (
				(scsiActive == 0) &&
				!scsiDisconnected &&
				scsiDev.discPriv &&
				(diffTime_ms(lastActivityTime, getTime_ms()) >= 20) &&
				(scsiDev.phase == DATA_OUT))
			{
				// We're transferring over the SCSI bus faster than the SD card
				// can write.  There is no more buffer space once we've finished
				// this SCSI transfer.
				// The NCR 53C700 interface chips have a 250ms "byte-to-byte"
				// timeout buffer. SD card writes are supposed to complete
				// within 200ms, but sometimes they don't.
				// The NCR 53C700 series is used on HP 9000 workstations.
				scsiDisconnect();
				scsiDisconnected = 1;
				lastActivityTime = getTime_ms();
			}
			else if (scsiDisconnected &&
				(
					(prep == i) || // Buffers empty.
					// Send some messages every 100ms so we don't timeout.
					// At a minimum, a reselection involves an IDENTIFY message.
					(diffTime_ms(lastActivityTime, getTime_ms()) >= 100)
				))
			{
				int reconnected = scsiReconnect();
				if (reconnected)
				{
					scsiDisconnected = 0;
					lastActivityTime = getTime_ms(); // Don't disconnect immediately.
				}
				else if (diffTime_ms(lastActivityTime, getTime_ms()) >= 10000)
				{
					// Give up after 10 seconds of trying to reconnect.
					scsiDev.resetFlag = 1;
				}
			}
		}

		while (
			!scsiDev.resetFlag &&
			scsiDisconnected &&
			(diffTime_ms(lastActivityTime, getTime_ms()) <= 10000))
		{
			scsiDisconnected = !scsiReconnect();
		}
		if (scsiDisconnected)
		{
			// Failed to reconnect
			scsiDev.resetFlag = 1;
		}

		if (scsiDev.phase == DATA_OUT)
		{
			if (scsiDev.parityError && config->enableParity && !scsiDev.compatMode)
			{
				scsiDev.sense.code = ABORTED_COMMAND;
				scsiDev.sense.asc = SCSI_PARITY_ERROR;
				scsiDev.status = CHECK_CONDITION;;
			}
			scsiDev.phase = STATUS;
		}
		scsiDiskReset();
	}
}

void scsiDiskReset()
{
	scsiDev.dataPtr = 0;
	scsiDev.savedDataPtr = 0;
	scsiDev.dataLen = 0;
	// transfer.lba = 0; // Needed in Request Sense to determine failure
	transfer.blocks = 0;
	transfer.currentBlock = 0;

	// Cancel long running commands!
	if (transfer.inProgress == 1)
	{
		if (transfer.dir == TRANSFER_WRITE)
		{
			sdCompleteWrite();
		}
		else
		{
			sdCompleteRead();
		}
	}
	transfer.inProgress = 0;
	transfer.multiBlock = 0;
}

void scsiDiskInit()
{
	transfer.inProgress = 0;
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

	// The Card-detect switches of micro-sd sockets are not standard. Don't make
	// use of SD_CD so we can use sockets from other manufacturers.
	// Detect presence of the card by testing whether it responds to commands.
	// if (SD_CD_Read() == 1)
	{
		int retry;
		blockDev.state = blockDev.state | DISK_PRESENT;

		// Wait up to 5 seconds for the SD card to wake up.
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

