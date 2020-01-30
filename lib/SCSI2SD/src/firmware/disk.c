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

#include "stm32f2xx.h"

#include <assert.h>

// For SD write direct routines
#include "sdio.h"
#include "bsp_driver_sd.h"


#include "scsi.h"
#include "scsiPhy.h"
#include "config.h"
#include "disk.h"
#include "sd.h"
#include "time.h"
#include "bsp.h"

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
	else
	{
		s2s_delay_ms(10);
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
		uint32_t allocLength = (((uint16_t)scsiDev.cdb[7]) << 8) |
			scsiDev.cdb[8];

		scsiDev.data[0] = 0;
		scsiDev.data[1] = scsiDev.cdb[1];
		scsiDev.data[2] = 0;
		scsiDev.data[3] = 0;
		scsiDev.dataLen = 4;

		if (scsiDev.dataLen > allocLength)
		{
			scsiDev.dataLen = allocLength;
		}

		scsiDev.phase = DATA_IN;
	}
	else
	{
		commandHandled = 0;
	}

	return commandHandled;
}

static uint32_t
calcReadahead(uint32_t totalBytes, uint32_t sdSpeedKBs, uint32_t scsiSpeedKBs)
{
	if (scsiSpeedKBs == 0 || scsiDev.hostSpeedMeasured == 0)
	{
		return totalBytes;
	}

	// uint32_t readAheadBytes = totalBytes * (1 - scsiSpeedKBs / sdSpeedKBs);
	// Won't overflow with 65536 max bytes, 20000 max scsi speed.
	uint32_t readAheadBytes = totalBytes - totalBytes * scsiSpeedKBs / sdSpeedKBs;

	// Round up to nearest FIFO size (* 4 for safety)
	readAheadBytes = ((readAheadBytes / SCSI_FIFO_DEPTH) + 4) * SCSI_FIFO_DEPTH;

	if (readAheadBytes > totalBytes)
	{
		readAheadBytes = totalBytes;
	}

	return readAheadBytes;
}

void scsiDiskPoll()
{
	uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

	if (scsiDev.phase == DATA_IN &&
		transfer.currentBlock != transfer.blocks)
	{
		// Take responsibility for waiting for the phase delays
		uint32_t phaseChangeDelayUs = scsiEnterPhaseImmediate(DATA_IN);

		int totalSDSectors =
			transfer.blocks * SDSectorsPerSCSISector(bytesPerSector);
		uint32_t sdLBA =
			SCSISector2SD(
				scsiDev.target->cfg->sdSectorStart,
				bytesPerSector,
				transfer.lba);

		const int sdPerScsi = SDSectorsPerSCSISector(bytesPerSector);
		const int buffers = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
		int prep = 0;
		int i = 0;
		int scsiActive __attribute__((unused)) = 0; // unused if DMA disabled
		int sdActive = 0;

		// It's highly unlikely that someone is going to use huge transfers
		// per scsi command, but if they do it'll be slower than usual.
		uint32_t totalScsiBytes = transfer.blocks * bytesPerSector;
		int useSlowDataCount = totalScsiBytes >= SCSI_XFER_MAX;
		if (!useSlowDataCount)
		{
			scsiSetDataCount(totalScsiBytes);
		}

		while ((i < totalSDSectors) &&
			likely(scsiDev.phase == DATA_IN) &&
			likely(!scsiDev.resetFlag))
		{
			int completedDmaSectors;
			if (sdActive && (completedDmaSectors = sdReadDMAPoll(sdActive)))
			{
				prep += completedDmaSectors;
				sdActive -= completedDmaSectors;
			} else if (sdActive > 1)
			{
				if ((scsiDev.data[SD_SECTOR_SIZE * (prep % buffers) + 510] != 0xAA) ||
					(scsiDev.data[SD_SECTOR_SIZE * (prep % buffers) + 511] != 0x33))
				{
					prep += 1;
					sdActive -= 1;
				}
			}

			if (!sdActive &&
				(prep - i < buffers) &&
				(prep < totalSDSectors) &&
				((totalSDSectors - prep) >= sdPerScsi) &&
				(likely(!useSlowDataCount) || scsiPhyComplete()))
			{
				// Start an SD transfer if we have space.
				uint32_t startBuffer = prep % buffers;
				uint32_t sectors = totalSDSectors - prep;
				uint32_t freeBuffers = buffers - (prep - i);

				uint32_t contiguousBuffers = buffers - startBuffer;
				freeBuffers = freeBuffers < contiguousBuffers
					? freeBuffers : contiguousBuffers;
				sectors = sectors < freeBuffers ? sectors : freeBuffers;

				if (sectors > 128) sectors = 128; // 65536 DMA limit !!

				// Round-down when we have odd sector sizes.
				if (sdPerScsi != 1)
				{
					sectors = (sectors / sdPerScsi) * sdPerScsi;
				}

				for (int dodgy = 0; dodgy < sectors; dodgy++)
				{
					scsiDev.data[SD_SECTOR_SIZE * (startBuffer + dodgy) + 510] = 0xAA;
					scsiDev.data[SD_SECTOR_SIZE * (startBuffer + dodgy) + 511] = 0x33;
				}

				sdReadDMA(sdLBA + prep, sectors, &scsiDev.data[SD_SECTOR_SIZE * startBuffer]);

				sdActive = sectors;

				if (useSlowDataCount)
				{
					scsiSetDataCount((sectors / sdPerScsi) * bytesPerSector);
				}

				// Wait now that the SD card is busy
				// Chances are we've probably already waited sufficient time,
				// but it's hard to measure microseconds cheaply. So just wait
				// extra just-in-case. Hopefully it's in parallel with dma.
				if (phaseChangeDelayUs > 0)
				{
					s2s_delay_us(phaseChangeDelayUs);
					phaseChangeDelayUs = 0;
				}
			}

			if ((prep - i) > 0)
			{
				int dmaBytes = SD_SECTOR_SIZE;
				if ((i % sdPerScsi) == (sdPerScsi - 1))
				{
					dmaBytes = bytesPerSector % SD_SECTOR_SIZE;
					if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
				}

				uint8_t* scsiDmaData = &(scsiDev.data[SD_SECTOR_SIZE * (i % buffers)]);
				scsiWritePIO(scsiDmaData, dmaBytes);

				++i;
			}
		}

		if (phaseChangeDelayUs > 0 && !scsiDev.resetFlag) // zero bytes ?
		{
			s2s_delay_us(phaseChangeDelayUs);
			phaseChangeDelayUs = 0;
		}

		// We've finished transferring the data to the FPGA, now wait until it's
		// written to he SCSI bus.
		__disable_irq();
		while (!scsiPhyComplete() &&
			likely(scsiDev.phase == DATA_IN) &&
			likely(!scsiDev.resetFlag))
		{
			__WFI();
		}
		__enable_irq();

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
		int i = 0;
		int clearBSY = 0;

		int parityError = 0;
		int enableParity = scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY;

		uint32_t maxSectors = sizeof(scsiDev.data) / SD_SECTOR_SIZE;

		static_assert(SCSI_XFER_MAX >= sizeof(scsiDev.data), "Assumes SCSI_XFER_MAX >= sizeof(scsiDev.data)");

		// Start reading and filling fifos as soon as possible.
		// It's highly unlikely that someone is going to use huge transfers
		// per scsi command, but if they do it'll be slower than usual.
		// Note: Happens in Macintosh FWB HDD Toolkit benchmarks which default
		// to 768kb
		uint32_t totalTransferBytes = transfer.blocks * bytesPerSector;
		int useSlowDataCount = totalTransferBytes >= SCSI_XFER_MAX;
		if (!useSlowDataCount)
		{
			DWT->CYCCNT = 0; // Start counting cycles
			scsiSetDataCount(totalTransferBytes);
		}

		while ((i < totalSDSectors) &&
			likely(scsiDev.phase == DATA_OUT) &&
			likely(!scsiDev.resetFlag))
			// KEEP GOING to ensure FIFOs are in a good state.
			// likely(!parityError || !enableParity))
		{
			uint32_t rem = totalSDSectors - i;
			uint32_t sectors = rem < maxSectors ? rem : maxSectors;

			if (bytesPerSector == SD_SECTOR_SIZE)
			{
				// We assume the SD card is faster than the SCSI interface, but has
				// no flow control. This can be handled if a) the scsi interface
				// doesn't block and b) we read enough SCSI sectors first so that
				// the SD interface cannot catch up.
				uint32_t totalBytes = sectors * SD_SECTOR_SIZE;

				uint32_t sdSpeedKBs = s2s_getSdRateKBs() + (scsiDev.sdUnderrunCount * 256);
				uint32_t readAheadBytes = calcReadahead(
					totalBytes,
					sdSpeedKBs,
					scsiDev.hostSpeedKBs);

				if (useSlowDataCount)
				{
					DWT->CYCCNT = 0; // Start counting cycles
					scsiSetDataCount(totalBytes);
				}

				uint32_t scsiBytesRead = 0;
				if (readAheadBytes > 0)
				{
					scsiReadPIO(
						&scsiDev.data[scsiBytesRead],
						readAheadBytes,
						&parityError);
					scsiBytesRead += readAheadBytes;

					if (i == 0 && !useSlowDataCount)
					{
						uint32_t elapsedCycles = DWT->CYCCNT;

						// uint32_t rateKBs = (readAheadBytes / 1000) / (elapsedCycles / HAL_RCC_GetHCLKFreq());
						// Scaled by 4 to avoid overflow w/ max 65536 at 108MHz.
						uint32_t rateKBs = ((readAheadBytes / 4) * (HAL_RCC_GetHCLKFreq() / 1000) / elapsedCycles) * 4;

						scsiDev.hostSpeedKBs = (scsiDev.hostSpeedKBs + rateKBs) / 2;
						scsiDev.hostSpeedMeasured = 1;

						if (rateKBs < scsiDev.hostSpeedKBs)
						{
							// Our readahead was too slow; assume remaining bytes
							// will be as well.
							if (readAheadBytes < totalBytes)
							{
								uint32_t properReadahead = calcReadahead(
									totalBytes,
									sdSpeedKBs,
									rateKBs);

								if (properReadahead > readAheadBytes)
								{
									uint32_t diff = properReadahead - readAheadBytes;
									readAheadBytes = properReadahead;
									scsiReadPIO(
										&scsiDev.data[scsiBytesRead],
										diff,
										&parityError);
									scsiBytesRead += diff;
								}
							}
						}
					}
				}

				HAL_SD_WriteBlocks_DMA(&hsd, (uint32_t*) (&scsiDev.data[0]), (i + sdLBA) * 512ll, SD_SECTOR_SIZE, sectors);

				int underrun = 0;
				if (scsiBytesRead < totalBytes && !scsiDev.resetFlag)
				{
					scsiReadPIO(
						&scsiDev.data[scsiBytesRead],
						totalBytes - readAheadBytes,
						&parityError);

					// Oh dear, SD finished first.
					underrun = hsd.DmaTransferCplt;

					scsiBytesRead += (totalBytes - readAheadBytes);
				}

				uint32_t dmaFinishTime = s2s_getTime_ms();
				while ((!hsd.SdTransferCplt ||
						__HAL_SD_SDIO_GET_FLAG(&hsd, SDIO_FLAG_TXACT)) &&
					s2s_elapsedTime_ms(dmaFinishTime) < 180)
				{
					// Wait while keeping BSY.
				}

				if (i + sectors >= totalSDSectors &&
					!underrun &&
					(!parityError || !enableParity))
				{
					// We're transferring over the SCSI bus faster than the SD card
					// can write.  All data is buffered, and we're just waiting for
					// the SD card to complete. The host won't let us disconnect.
					// Some drivers set a 250ms timeout on transfers to complete.
					// SD card writes are supposed to complete
					// within 200ms, but sometimes they don't.
					// Just pretend we're finished.
					process_Status();
					clearBSY = process_MessageIn(0); // Will go to BUS_FREE state but keep BSY asserted.
				}

				HAL_SD_CheckWriteOperation(&hsd, (uint32_t)SD_DATATIMEOUT);

				if (underrun && (!parityError || !enableParity))
				{
					// Try again. Data is still in memory.
					sdTmpWrite(&scsiDev.data[0], i + sdLBA, sectors);
					scsiDev.sdUnderrunCount++;
				}

				i += sectors;
			}
			else
			{
				// Well, until we have some proper non-blocking SD code, we must
				// do this in a half-duplex fashion. We need to write as much as
				// possible in each SD card transaction.
				// use sg_dd from sg_utils3 tools to test.

				if (useSlowDataCount)
				{
					scsiSetDataCount(sectors * bytesPerSector);
				}

				for (int scsiSector = i; scsiSector < i + sectors; ++scsiSector)
				{
					int dmaBytes = SD_SECTOR_SIZE;
					if ((scsiSector % sdPerScsi) == (sdPerScsi - 1))
					{
						dmaBytes = bytesPerSector % SD_SECTOR_SIZE;
						if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
					}

					scsiReadPIO(&scsiDev.data[SD_SECTOR_SIZE * (scsiSector - i)], dmaBytes, &parityError);
				}
				if (!parityError || !enableParity)
				{
					sdTmpWrite(&scsiDev.data[0], i + sdLBA, sectors);
				}
				i += sectors;
			}
		}

		// Should already be complete here as we've ready the FIFOs
		// by now. Check anyway.
		__disable_irq();
		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
		{
			__WFI();
		}
		__enable_irq();

		if (clearBSY)
		{
			enter_BusFree();
		}

		if (scsiDev.phase == DATA_OUT)
		{
			if (parityError &&
				(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY))
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
#if 0
	if (
		((scsiDev.boardCfg.flags & S2S_CFG_ENABLE_CACHE) == 0) ||
			(transfer.multiBlock == 0)
		)
#endif
	{
		sdCompleteTransfer();
	}

	transfer.multiBlock = 0;
}

void scsiDiskInit()
{
	scsiDiskReset();

	// Don't require the host to send us a START STOP UNIT command
	blockDev.state = DISK_STARTED;
}

