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
	int result = 0;
	if (blockDev.state & DISK_PRESENT)
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
		// Save the "MODE SELECT savable parameters"
		s2s_configSave(
			scsiDev.target->targetId,
			scsiDev.target->liveCfg.bytesPerSector);
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
	uint32_t lba = (((uint32_t) scsiDev.cdb[2]) << 24) +
		(((uint32_t) scsiDev.cdb[3]) << 16) +
		(((uint32_t) scsiDev.cdb[4]) << 8) +
		scsiDev.cdb[5];
	int pmi = scsiDev.cdb[8] & 1;

	uint32_t capacity = getScsiCapacity(
		scsiDev.target->cfg->sdSectorStart,
		scsiDev.target->liveCfg.bytesPerSector,
		scsiDev.target->cfg->scsiSectors);

	if (!pmi && lba)
	{
		// error.
		// We don't do anything with the "partial medium indicator", and
		// assume that delays are constant across each block. But the spec
		// says we must return this error if pmi is specified incorrectly.
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		scsiDev.phase = STATUS;
	}
	else if (capacity > 0)
	{
		uint32_t highestBlock = capacity - 1;

		scsiDev.data[0] = highestBlock >> 24;
		scsiDev.data[1] = highestBlock >> 16;
		scsiDev.data[2] = highestBlock >> 8;
		scsiDev.data[3] = highestBlock;

		uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
		scsiDev.data[4] = bytesPerSector >> 24;
		scsiDev.data[5] = bytesPerSector >> 16;
		scsiDev.data[6] = bytesPerSector >> 8;
		scsiDev.data[7] = bytesPerSector;
		scsiDev.dataLen = 8;
		scsiDev.phase = DATA_IN;
	}
	else
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = NOT_READY;
		scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
		scsiDev.phase = STATUS;
	}
}

static void doWrite(uint32_t lba, uint32_t blocks)
{
	if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB)) {
		// Floppies are supposed to be slow. Some systems can't handle a floppy
		// without an access time
		s2s_delay_ms(10);
	}

	uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

	if (unlikely(blockDev.state & DISK_WP) ||
		unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL))

	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = WRITE_PROTECTED;
		scsiDev.phase = STATUS;
	}
	else if (unlikely(((uint64_t) lba) + blocks >
		getScsiCapacity(
			scsiDev.target->cfg->sdSectorStart,
			bytesPerSector,
			scsiDev.target->cfg->scsiSectors
			)
		))
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		scsiDev.phase = STATUS;
	}
	else
	{
		transfer.lba = lba;
		transfer.blocks = blocks;
		transfer.currentBlock = 0;
		scsiDev.phase = DATA_OUT;
		scsiDev.dataLen = bytesPerSector;
		scsiDev.dataPtr = bytesPerSector;

		// No need for single-block writes atm.  Overhead of the
		// multi-block write is minimal.
		transfer.multiBlock = 1;


		// TODO uint32_t sdLBA =
// TODO 			SCSISector2SD(
	// TODO 			scsiDev.target->cfg->sdSectorStart,
		// TODO 		bytesPerSector,
			// TODO 	lba);
		// TODO uint32_t sdBlocks = blocks * SDSectorsPerSCSISector(bytesPerSector);
		// TODO sdWriteMultiSectorPrep(sdLBA, sdBlocks);
	}
}


static void doRead(uint32_t lba, uint32_t blocks)
{
	if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB)) {
		// Floppies are supposed to be slow. Some systems can't handle a floppy
		// without an access time
		s2s_delay_ms(10);
	}

	uint32_t capacity = getScsiCapacity(
		scsiDev.target->cfg->sdSectorStart,
		scsiDev.target->liveCfg.bytesPerSector,
		scsiDev.target->cfg->scsiSectors);
	if (unlikely(((uint64_t) lba) + blocks > capacity))
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		scsiDev.phase = STATUS;
	}
	else
	{
		transfer.lba = lba;
		transfer.blocks = blocks;
		transfer.currentBlock = 0;
		scsiDev.phase = DATA_IN;
		scsiDev.dataLen = 0; // No data yet

		uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
		uint32_t sdSectorPerSCSISector = SDSectorsPerSCSISector(bytesPerSector);
		uint32_t sdSectors =
			blocks * sdSectorPerSCSISector;

		if ((
				(sdSectors == 1) &&
				!(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_CACHE)
			) ||
			unlikely(((uint64_t) lba) + blocks == capacity)
			)
		{
			// We get errors on reading the last sector using a multi-sector
			// read :-(
			transfer.multiBlock = 0;
		}
		else
		{
			transfer.multiBlock = 1;

			// uint32_t sdLBA =
				// SCSISector2SD(
					// scsiDev.target->cfg->sdSectorStart,
					// bytesPerSector,
					// lba);

			// TODO sdReadMultiSectorPrep(sdLBA, sdSectors);
		}
	}
}

static void doSeek(uint32_t lba)
{
	if (lba >=
		getScsiCapacity(
			scsiDev.target->cfg->sdSectorStart,
			scsiDev.target->liveCfg.bytesPerSector,
			scsiDev.target->cfg->scsiSectors)
		)
	{
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		scsiDev.phase = STATUS;
	}
}

static int doTestUnitReady()
{
	int ready = 1;
	if (likely(blockDev.state == (DISK_STARTED | DISK_PRESENT | DISK_INITIALISED)))
	{
		// nothing to do.
	}
	else if (unlikely(!(blockDev.state & DISK_STARTED)))
	{
		ready = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = NOT_READY;
		scsiDev.target->sense.asc = LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED;
		scsiDev.phase = STATUS;
	}
	else if (unlikely(!(blockDev.state & DISK_PRESENT)))
	{
		ready = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = NOT_READY;
		scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
		scsiDev.phase = STATUS;
	}
	else if (unlikely(!(blockDev.state & DISK_INITIALISED)))
	{
		ready = 0;
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = NOT_READY;
		scsiDev.target->sense.asc = LOGICAL_UNIT_NOT_READY_CAUSE_NOT_REPORTABLE;
		scsiDev.phase = STATUS;
	}
	return ready;
}

// Handle direct-access scsi device commands
int scsiDiskCommand()
{
	int commandHandled = 1;

	uint8_t command = scsiDev.cdb[0];
	if (unlikely(command == 0x1B))
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
	else if (unlikely(command == 0x00))
	{
		// TEST UNIT READY
		doTestUnitReady();
	}
	else if (unlikely(!doTestUnitReady()))
	{
		// Status and sense codes already set by doTestUnitReady
	}
	else if (likely(command == 0x08))
	{
		// READ(6)
		uint32_t lba =
			(((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
			(((uint32_t) scsiDev.cdb[2]) << 8) +
			scsiDev.cdb[3];
		uint32_t blocks = scsiDev.cdb[4];
		if (unlikely(blocks == 0)) blocks = 256;
		doRead(lba, blocks);
	}
	else if (likely(command == 0x28))
	{
		// READ(10)
		// Ignore all cache control bits - we don't support a memory cache.

		uint32_t lba =
			(((uint32_t) scsiDev.cdb[2]) << 24) +
			(((uint32_t) scsiDev.cdb[3]) << 16) +
			(((uint32_t) scsiDev.cdb[4]) << 8) +
			scsiDev.cdb[5];
		uint32_t blocks =
			(((uint32_t) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];

		doRead(lba, blocks);
	}
	else if (likely(command == 0x0A))
	{
		// WRITE(6)
		uint32_t lba =
			(((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
			(((uint32_t) scsiDev.cdb[2]) << 8) +
			scsiDev.cdb[3];
		uint32_t blocks = scsiDev.cdb[4];
		if (unlikely(blocks == 0)) blocks = 256;
		doWrite(lba, blocks);
	}
	else if (likely(command == 0x2A) || // WRITE(10)
		unlikely(command == 0x2E)) // WRITE AND VERIFY
	{
		// Ignore all cache control bits - we don't support a memory cache.
		// Don't bother verifying either. The SD card likely stores ECC
		// along with each flash row.

		uint32_t lba =
			(((uint32_t) scsiDev.cdb[2]) << 24) +
			(((uint32_t) scsiDev.cdb[3]) << 16) +
			(((uint32_t) scsiDev.cdb[4]) << 8) +
			scsiDev.cdb[5];
		uint32_t blocks =
			(((uint32_t) scsiDev.cdb[7]) << 8) +
			scsiDev.cdb[8];

		doWrite(lba, blocks);
	}
	else if (unlikely(command == 0x04))
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
	else if (unlikely(command == 0x25))
	{
		// READ CAPACITY
		doReadCapacity();
	}
	else if (unlikely(command == 0x0B))
	{
		// SEEK(6)
		uint32_t lba =
			(((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
			(((uint32_t) scsiDev.cdb[2]) << 8) +
			scsiDev.cdb[3];

		doSeek(lba);
	}

	else if (unlikely(command == 0x2B))
	{
		// SEEK(10)
		uint32_t lba =
			(((uint32_t) scsiDev.cdb[2]) << 24) +
			(((uint32_t) scsiDev.cdb[3]) << 16) +
			(((uint32_t) scsiDev.cdb[4]) << 8) +
			scsiDev.cdb[5];

		doSeek(lba);
	}
	else if (unlikely(command == 0x36))
	{
		// LOCK UNLOCK CACHE
		// We don't have a cache to lock data into. do nothing.
	}
	else if (unlikely(command == 0x34))
	{
		// PRE-FETCH.
		// We don't have a cache to pre-fetch into. do nothing.
	}
	else if (unlikely(command == 0x1E))
	{
		// PREVENT ALLOW MEDIUM REMOVAL
		// Not much we can do to prevent the user removing the SD card.
		// do nothing.
	}
	else if (unlikely(command == 0x01))
	{
		// REZERO UNIT
		// Set the lun to a vendor-specific state. Ignore.
	}
	else if (unlikely(command == 0x35))
	{
		// SYNCHRONIZE CACHE
		// We don't have a cache. do nothing.
	}
	else if (unlikely(command == 0x2F))
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
			scsiDev.target->sense.code = ILLEGAL_REQUEST;
			scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
			scsiDev.phase = STATUS;
		}
	}
	else if (unlikely(command == 0x37))
	{
		// READ DEFECT DATA
		scsiDev.status = CHECK_CONDITION;
		scsiDev.target->sense.code = NO_SENSE;
		scsiDev.target->sense.asc = DEFECT_LIST_NOT_FOUND;
		scsiDev.phase = STATUS;

	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}

void scsiDiskPoll()
{
	uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

	if (scsiDev.phase == DATA_IN &&
		transfer.currentBlock != transfer.blocks)
	{
		scsiEnterPhase(DATA_IN);

		int totalSDSectors =
			transfer.blocks * SDSectorsPerSCSISector(bytesPerSector);
		uint32_t sdLBA =
			SCSISector2SD(
				scsiDev.target->cfg->sdSectorStart,
				bytesPerSector,
				transfer.lba);

		const int sdPerScsi = SDSectorsPerSCSISector(bytesPerSector);
		int buffers = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
		int prep = 0;
		int i = 0;
		int scsiActive = 0;
		// int sdActive = 0;
		while ((i < totalSDSectors) &&
			likely(scsiDev.phase == DATA_IN) &&
			likely(!scsiDev.resetFlag))
		{
			// Wait for the next DMA interrupt. It's beneficial to halt the
			// processor to give the DMA controller more memory bandwidth to
			// work with.
#if 0
			int scsiBusy = 1;
			int sdBusy = 1;
			while (scsiBusy && sdBusy)
			{
				uint8_t intr = CyEnterCriticalSection();
				scsiBusy = scsiDMABusy();
				sdBusy = sdDMABusy();
				if (scsiBusy && sdBusy)
				{
					__WFI();
				}
				CyExitCriticalSection(intr);
			}
#endif

#if 0
			if (sdActive && !sdBusy && sdReadSectorDMAPoll())
			{
				sdActive = 0;
				prep++;
			}

			// Usually SD is slower than the SCSI interface.
			// Prioritise starting the read of the next sector over starting a
			// SCSI transfer for the last sector
			// ie. NO "else" HERE.
			if (!sdActive &&
				(prep - i < buffers) &&
				(prep < totalSDSectors))
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
#endif
#if 0
			uint32_t maxSectors = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
			uint32_t rem = totalSDSectors - i;
			uint32_t sectors =
				rem < maxSectors ? rem : maxSectors;
			sdTmpRead(&scsiDev.data[0], i + sdLBA, sectors);
			scsiWrite(&scsiDev.data[0], sectors * SD_SECTOR_SIZE);
			i += sectors;
#endif
			if ((prep - i < buffers) &&
				(prep < totalSDSectors))
			{
				// TODO MM Blocking reads are bad mmkay
				sdTmpRead(&scsiDev.data[SD_SECTOR_SIZE * (prep % buffers)], sdLBA + prep, 1);
				prep++;
			}

			if (scsiActive && scsiPhyComplete() && scsiWriteDMAPoll())
			{
				scsiActive = 0;
				i++;
				scsiPhyFifoFlip();
			}
			if (!scsiActive && ((prep - i) > 0))
			{
				int dmaBytes = SD_SECTOR_SIZE;
				if ((i % sdPerScsi) == (sdPerScsi - 1))
				{
					dmaBytes = bytesPerSector % SD_SECTOR_SIZE;
					if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
				}
				scsiWriteDMA(&scsiDev.data[SD_SECTOR_SIZE * (i % buffers)], dmaBytes);
				scsiActive = 1;
			}
		}

		// We've finished transferring the data to the FPGA, now wait until it's
		// written to he SCSI bus.
		while (!scsiPhyComplete() &&
			likely(scsiDev.phase == DATA_IN) &&
			likely(!scsiDev.resetFlag))
		{
#if 0
		_WFI();
#endif
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

		const int sdPerScsi = SDSectorsPerSCSISector(bytesPerSector);
		int totalSDSectors = transfer.blocks * sdPerScsi;
		uint32_t sdLBA =
			SCSISector2SD(
				scsiDev.target->cfg->sdSectorStart,
				bytesPerSector,
				transfer.lba);
		// int buffers = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
		// int prep = 0;
		int i = 0;
		// int scsiDisconnected = 0;
		int scsiComplete = 0;
		// uint32_t lastActivityTime = s2s_getTime_ms();
		// int scsiActive = 0;
		// int sdActive = 0;

		while ((i < totalSDSectors) &&
			(likely(scsiDev.phase == DATA_OUT) || // scsiDisconnect keeps our phase.
				scsiComplete) &&
			likely(!scsiDev.resetFlag))
		{
			// Well, until we have some proper non-blocking SD code, we must
			// do this in a half-duplex fashion. We need to write as much as
			// possible in each SD card transaction.
			uint32_t maxSectors = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
			uint32_t rem = totalSDSectors - i;
			uint32_t sectors =
				rem < maxSectors ? rem : maxSectors;
			scsiRead(&scsiDev.data[0], sectors * SD_SECTOR_SIZE);
			sdTmpWrite(&scsiDev.data[0], i + sdLBA, sectors);
			i += sectors;
#if 0
			// Wait for the next DMA interrupt. It's beneficial to halt the
			// processor to give the DMA controller more memory bandwidth to
			// work with.
			int scsiBusy = 1;
			int sdBusy = 1;
			while (scsiBusy && sdBusy)
			{
				uint8_t intr = CyEnterCriticalSection();
				scsiBusy = scsiDMABusy();
				sdBusy = sdDMABusy();
				if (scsiBusy && sdBusy)
				{
					__WFI();
				}
				CyExitCriticalSection(intr);
			}

			if (sdActive && !sdBusy && sdWriteSectorDMAPoll())
			{
				sdActive = 0;
				i++;
			}
			if (!sdActive && ((prep - i) > 0))
			{
				// Start an SD transfer if we have space.
				sdWriteMultiSectorDMA(&scsiDev.data[SD_SECTOR_SIZE * (i % buffers)]);
				sdActive = 1;
			}

			uint32_t now = getTime_ms();

			if (scsiActive && !scsiBusy && scsiReadDMAPoll())
			{
				scsiActive = 0;
				++prep;
				lastActivityTime = now;
			}
			if (!scsiActive &&
				((prep - i) < buffers) &&
				(prep < totalSDSectors) &&
				likely(!scsiDisconnected))
			{
				int dmaBytes = SD_SECTOR_SIZE;
				if ((prep % sdPerScsi) == (sdPerScsi - 1))
				{
					dmaBytes = bytesPerSector % SD_SECTOR_SIZE;
					if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
				}
				scsiReadDMA(&scsiDev.data[SD_SECTOR_SIZE * (prep % buffers)], dmaBytes);
				scsiActive = 1;
			}
			else if (
				(scsiDev.boardCfg.flags & CONFIG_ENABLE_DISCONNECT) &&
				(scsiActive == 0) &&
				likely(!scsiDisconnected) &&
				unlikely(scsiDev.discPriv) &&
				unlikely(diffTime_ms(lastActivityTime, now) >= 20) &&
				likely(scsiDev.phase == DATA_OUT))
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
			else if (unlikely(scsiDisconnected) &&
				(
					(prep == i) || // Buffers empty.
					// Send some messages every 100ms so we don't timeout.
					// At a minimum, a reselection involves an IDENTIFY message.
					unlikely(diffTime_ms(lastActivityTime, now) >= 100)
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
			else if (
				likely(!scsiComplete) &&
				(sdActive == 1) &&
				(prep == totalSDSectors) && // All scsi data read and buffered
				likely(!scsiDev.discPriv) && // Prefer disconnect where possible.
				unlikely(diffTime_ms(lastActivityTime, now) >= 150) &&

				likely(scsiDev.phase == DATA_OUT) &&
				!(scsiDev.cdb[scsiDev.cdbLen - 1] & 0x01) // Not linked command
				)
			{
				// We're transferring over the SCSI bus faster than the SD card
				// can write.  All data is buffered, and we're just waiting for
				// the SD card to complete. The host won't let us disconnect.
				// Some drivers set a 250ms timeout on transfers to complete.
				// SD card writes are supposed to complete
				// within 200ms, but sometimes they don'to.
				// Just pretend we're finished.
				scsiComplete = 1;

				process_Status();
				process_MessageIn(); // Will go to BUS_FREE state

				// Try and prevent anyone else using the SCSI bus while we're not ready.
				SCSI_SetPin(SCSI_Out_BSY); 
			}
#endif
		}

#if 0
		if (scsiComplete)
		{
			SCSI_ClearPin(SCSI_Out_BSY);
		}
		while (
			!scsiDev.resetFlag &&
			unlikely(scsiDisconnected) &&
			(s2s_elapsedTime_ms(lastActivityTime) <= 10000))
		{
			scsiDisconnected = !scsiReconnect();
		}
		if (scsiDisconnected)
		{
			// Failed to reconnect
			scsiDev.resetFlag = 1;
		}
#endif

		if (scsiDev.phase == DATA_OUT)
		{
			if (scsiDev.parityError &&
				(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY) &&
				(scsiDev.compatMode >= COMPAT_SCSI2))
			{
				scsiDev.target->sense.code = ABORTED_COMMAND;
				scsiDev.target->sense.asc = SCSI_PARITY_ERROR;
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
	if (
		((scsiDev.boardCfg.flags & S2S_CFG_ENABLE_CACHE) == 0) ||
			(transfer.multiBlock == 0)
		)
	{
#if 0
		sdCompleteTransfer();
#endif
	}

	transfer.multiBlock = 0;
}

void scsiDiskInit()
{
	scsiDiskReset();

	// Don't require the host to send us a START STOP UNIT command
	blockDev.state = DISK_STARTED;
}

