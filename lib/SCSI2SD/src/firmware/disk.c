//    Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//    Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
//
//    This file is part of SCSI2SD.
//
//    SCSI2SD is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    SCSI2SD is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.

#ifdef STM32F2xx
#include "stm32f2xx.h"
#endif
#ifdef STM32F4xx
#include "stm32f4xx.h"
#endif

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
        if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB) ||
            scsiDev.compatMode < COMPAT_SCSI2)
        {
            s2s_delay_ms(10);
        }
        else
        {
            s2s_delay_us(10);
        }
    }
}

static int doTestUnitReady()
{
    int ready = 1;
    if (likely(blockDev.state == (DISK_PRESENT | DISK_INITIALISED) &&
		scsiDev.target->started))
    {
        // nothing to do.
    }
    else if (unlikely(!scsiDev.target->started))
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
        //int immed = scsiDev.cdb[1] & 1;
        int start = scsiDev.cdb[4] & 1;
	int loadEject = scsiDev.cdb[4] & 2;
	
        if (loadEject)
        {
            // Ignore load/eject requests. We can't do that.
        }
        else if (start)
        {
            scsiDev.target->started = 1;
            if (!(blockDev.state & DISK_INITIALISED))
            {
                doSdInit();
            }
        }
        else
        {
            scsiDev.target->started = 0;
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

static void diskDataInBuffered(int totalSDSectors, uint32_t sdLBA, int useSlowDataCount, uint32_t* phaseChangeDelayNs)
{
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

    const int sdPerScsi = SDSectorsPerSCSISector(bytesPerSector);
    const int buffers = sizeof(scsiDev.data) / SD_SECTOR_SIZE;
    int prep = 0;
    int i = 0;
    int scsiActive __attribute__((unused)) = 0; // unused if DMA disabled
    int sdActive = 0;

    int gotHalf = 0;
    int sentHalf = 0;

    while ((i < totalSDSectors) &&
        likely(scsiDev.phase == DATA_IN) &&
        likely(!scsiDev.resetFlag))
    {
        int completedDmaSectors;
        if (sdActive && (completedDmaSectors = sdReadDMAPoll(sdActive)))
        {
            prep += completedDmaSectors;
            sdActive -= completedDmaSectors;
            gotHalf = 0;
        }
        else if (sdActive > 1)
        {
            if ((scsiDev.data[SD_SECTOR_SIZE * (prep % buffers) + 510] != 0xAA) ||
                (scsiDev.data[SD_SECTOR_SIZE * (prep % buffers) + 511] != 0x33))
            {
                prep += 1;
                sdActive -= 1;
                gotHalf = 0;
            }
            else if (scsiDev.data[SD_SECTOR_SIZE * (prep % buffers) + 127] != 0xAA)
            {
                // Half-block
                gotHalf = 1;
            }
        }

        if (!sdActive &&
            (prep - i < buffers) &&
            (prep < totalSDSectors) &&
            ((totalSDSectors - prep) >= sdPerScsi) &&
            (likely(!useSlowDataCount) || scsiPhyComplete()) &&
            (HAL_SD_GetState(&hsd) != HAL_SD_STATE_BUSY)) // rx complete but IRQ not fired yet.
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
                scsiDev.data[SD_SECTOR_SIZE * (startBuffer + dodgy) + 127] = 0xAA;

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
            if (*phaseChangeDelayNs > 0)
            {
                s2s_delay_ns(*phaseChangeDelayNs);
                *phaseChangeDelayNs = 0;
            }
        }

        int fifoReady = scsiFifoReady();
        if (((prep - i) > 0) && fifoReady)
        {
            int dmaBytes = SD_SECTOR_SIZE;
            if ((i % sdPerScsi) == (sdPerScsi - 1))
            {
                dmaBytes = bytesPerSector % SD_SECTOR_SIZE;
                if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
            }

            uint8_t* scsiDmaData = &(scsiDev.data[SD_SECTOR_SIZE * (i % buffers)]);

            if (sentHalf)
            {
                scsiDmaData += SD_SECTOR_SIZE / 2;
                dmaBytes -= (SD_SECTOR_SIZE / 2);
            }
            scsiWritePIO(scsiDmaData, dmaBytes);

            ++i;
            sentHalf = 0;
            gotHalf = 0;
        }
        else if (gotHalf && !sentHalf && fifoReady && bytesPerSector == SD_SECTOR_SIZE)
        {
            uint8_t* scsiDmaData = &(scsiDev.data[SD_SECTOR_SIZE * (i % buffers)]);
            scsiWritePIO(scsiDmaData, SD_SECTOR_SIZE / 2);
            sentHalf = 1;
        }
    }
}

// Transfer from the SD card straight to the SCSI Fifo without storing in memory first for lower latency
// This requires hardware flow control on the SD device (broken on stm32f205)
// Only functional for 512 byte sectors.
static void diskDataInDirect(uint32_t totalSDSectors, uint32_t sdLBA, int useSlowDataCount, uint32_t* phaseChangeDelayNs)
{
    sdReadCmd(sdLBA, totalSDSectors);

    // Wait while the SD card starts buffering data
    if (*phaseChangeDelayNs > 0)
    {
        s2s_delay_ns(*phaseChangeDelayNs);
        *phaseChangeDelayNs = 0;
    }

    for (int i = 0; i < totalSDSectors && !scsiDev.resetFlag; ++i)
    {
        if (i % 128 == 0)
        {
            // SD DPSM has 24 bit limit. Re-use 128 (DMA limit)
            uint32_t chunk = totalSDSectors - i > 128 ? 128 : totalSDSectors - i;
            sdReadPIOData(chunk);

            if (useSlowDataCount)
            {
                while (!scsiDev.resetFlag && !scsiPhyComplete())
                {}
                scsiSetDataCount(chunk * SD_SECTOR_SIZE); // SCSI_XFER_MAX > 65536
            }
        }

        // The SCSI fifo is a full sector so we only need to check once.
        while (!scsiFifoReady() && !scsiDev.resetFlag)
        {}

        int byteCount = 0;
        while(byteCount < SD_SECTOR_SIZE &&
            likely(!scsiDev.resetFlag) &&
            likely(scsiDev.phase == DATA_IN) &&
            !__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT))
        {
            if(__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXFIFOHF))
            {
                // The SDIO fifo is 32 x 32bits. As we're using the "half full" flag we must
                // always read half the FIFO.

                for (int j = 0; j < 4; ++j)
                {
                    uint32_t data[4];
                    data[0] = SDIO_ReadFIFO(hsd.Instance);
                    data[1] = SDIO_ReadFIFO(hsd.Instance);
                    data[2] = SDIO_ReadFIFO(hsd.Instance);
                    data[3] = SDIO_ReadFIFO(hsd.Instance);

                    *((volatile uint32_t*)SCSI_FIFO_DATA) = data[0];
                    *((volatile uint32_t*)SCSI_FIFO_DATA) = data[1];
                    *((volatile uint32_t*)SCSI_FIFO_DATA) = data[2];
                    *((volatile uint32_t*)SCSI_FIFO_DATA) = data[3];
                }

                byteCount += 64;
            }
        }

        int error = 0;
        if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_DTIMEOUT))
        {
            __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_DTIMEOUT);
            error = 1;
        }
        else if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_DCRCFAIL))
        {
            __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_DCRCFAIL);
            error = 1;
        }
        else if (__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXOVERR))
        {
            __HAL_SD_CLEAR_FLAG(&hsd, SDIO_FLAG_RXOVERR);
            error = 1;
        }

        if (error && scsiDev.phase == DATA_IN)
        {
            __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);

            scsiDiskReset();

            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = HARDWARE_ERROR;
            scsiDev.target->sense.asc = LOGICAL_UNIT_COMMUNICATION_FAILURE;
            scsiDev.phase = STATUS;
        }

        // We need the SCSI FIFO count to complete even after the SD read has failed
        while (byteCount < SD_SECTOR_SIZE &&
            likely(!scsiDev.resetFlag))
        {
            scsiPhyTx32(0, 0);
            byteCount += 4;
        }
    }

    /* Send stop transmission command in case of multiblock read */
    if(totalSDSectors > 1U)
    {
        SDMMC_CmdStopTransfer(hsd.Instance);
    }

    // Read remaining data
    uint32_t extraCount = SD_DATATIMEOUT;
    while ((__HAL_SD_GET_FLAG(&hsd, SDIO_FLAG_RXDAVL)) && (extraCount > 0))
    {
        SDIO_ReadFIFO(hsd.Instance);
        extraCount--;
    }

    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_DATA_FLAGS);
    hsd.State = HAL_SD_STATE_READY;
    
    sdCompleteTransfer(); // Probably overkill
}

static void diskDataIn()
{
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

    // Take responsibility for waiting for the phase delays
    uint32_t phaseChangeDelayNs = scsiEnterPhaseImmediate(DATA_IN);

    int totalSDSectors =
        transfer.blocks * SDSectorsPerSCSISector(bytesPerSector);
    uint32_t sdLBA =
        SCSISector2SD(
            scsiDev.target->cfg->sdSectorStart,
            bytesPerSector,
            transfer.lba);

    // It's highly unlikely that someone is going to use huge transfers
    // per scsi command, but if they do it'll be slower than usual.
    uint32_t totalScsiBytes = transfer.blocks * bytesPerSector;
    int useSlowDataCount = totalScsiBytes >= SCSI_XFER_MAX;
    if (!useSlowDataCount)
    {
        scsiSetDataCount(totalScsiBytes);
    }

#ifdef STM32F4xx
    // Direct mode requires hardware flow control to be working on the SD peripheral
    // Code isn't currently working above 128 sectors. TODO investigate
    if (totalSDSectors < 128 && bytesPerSector == SD_SECTOR_SIZE)
    {
        diskDataInDirect(totalSDSectors, sdLBA, useSlowDataCount, &phaseChangeDelayNs);
    }
    else
#endif 
    {
        diskDataInBuffered(totalSDSectors, sdLBA, useSlowDataCount, &phaseChangeDelayNs);
    }

    if (phaseChangeDelayNs > 0 && !scsiDev.resetFlag) // zero bytes ?
    {
        s2s_delay_ns(phaseChangeDelayNs);
        phaseChangeDelayNs = 0;
    }

    if (scsiDev.resetFlag)
    {
        HAL_SD_Abort(&hsd);
    }
    else
    {
        // Wait for the SD transfer to complete before we disable IRQs.
        // (Otherwise some cards will cause an error if we don't sent the
        // stop transfer command via the DMA complete handler in time)
        while (HAL_SD_GetState(&hsd) == HAL_SD_STATE_BUSY)
        {
            // Wait while keeping BSY.
        }
    }

    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd);
    while (cardState == HAL_SD_CARD_PROGRAMMING || cardState == HAL_SD_CARD_SENDING) 
    {
        cardState = HAL_SD_GetCardState(&hsd);
    }

    // We've finished transferring the data to the FPGA, now wait until it's
    // written to he SCSI bus.
    while (!scsiPhyComplete() &&
        likely(scsiDev.phase == DATA_IN) &&
        likely(!scsiDev.resetFlag))
    {
        __disable_irq();
        if (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
        {
            __WFI();
        }
        __enable_irq();
    }

    if (scsiDev.phase == DATA_IN)
    {
        scsiDev.phase = STATUS;
    }
    scsiDiskReset();
}

void diskDataOut_512(int totalSDSectors, uint32_t sdLBA, int useSlowDataCount, int* clearBSY, int* parityError)
{
    int i = 0;
    int disconnected = 0;

    int enableParity = scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY;

    uint32_t maxSectors = sizeof(scsiDev.data) / SD_SECTOR_SIZE;

    int lastWriteSize = 0;

    while ((i < totalSDSectors) &&
        likely(scsiDev.phase == DATA_OUT) &&
        likely(!scsiDev.resetFlag))
        // KEEP GOING to ensure FIFOs are in a good state.
        // likely(!parityError || !enableParity))
    {

        uint32_t maxXferSectors = SCSI_XFER_MAX / SD_SECTOR_SIZE;
        uint32_t rem = totalSDSectors - i;
        uint32_t sectors = rem < maxXferSectors ? rem : maxXferSectors;

        uint32_t totalBytes = sectors * SD_SECTOR_SIZE;

        if (useSlowDataCount)
        {
            scsiSetDataCount(totalBytes);
        }

        lastWriteSize = sectors;
        HAL_SD_WriteBlocks_DMA(&hsd, i + sdLBA, sectors);
        int j = 0;
        int prep = 0;
        int sdActive = 0;
        uint32_t dmaFinishTime = 0;
        while (j < sectors && !scsiDev.resetFlag)
        {
            if (sdActive &&
                HAL_SD_GetState(&hsd) != HAL_SD_STATE_BUSY &&
                !sdIsBusy())
            {
                j += sdActive;
                sdActive = 0;
            }
            if (!sdActive && ((prep - j) > 0))
            {
                // Start an SD transfer if we have space.
                HAL_SD_WriteBlocks_Data(&hsd, &scsiDev.data[SD_SECTOR_SIZE * (j % maxSectors)]);

                sdActive = 1;
            }

            if (((prep - j) < maxSectors) &&
                (prep < sectors) &&
                scsiFifoReady())
            {
                scsiReadPIO(
                    &scsiDev.data[(prep % maxSectors) * SD_SECTOR_SIZE],
                    SD_SECTOR_SIZE,
                    parityError);
                prep++;
                if (prep == sectors)
                {
                    dmaFinishTime = s2s_getTime_ms();
                }
            }
        
            if (i + prep >= totalSDSectors &&
                !disconnected &&
                (!(*parityError) || !enableParity) &&
                s2s_elapsedTime_ms(dmaFinishTime) >= 180)
            {
                // We're transferring over the SCSI bus faster than the SD card
                // can write.  All data is buffered, and we're just waiting for
                // the SD card to complete. The host won't let us disconnect.
                // Some drivers set a 250ms timeout on transfers to complete.
                // SD card writes are supposed to complete
                // within 200ms, but sometimes they don't.
                // Just pretend we're finished.
                process_Status();
                *clearBSY = process_MessageIn(0); // Will go to BUS_FREE state but keep BSY asserted.
                disconnected = 1;
            }
        }

        if (scsiDev.resetFlag)
        {
            HAL_SD_Abort(&hsd);
        }
        else
        {
            while (HAL_SD_GetState(&hsd) == HAL_SD_STATE_BUSY) {} // Waits for DMA to complete
            if (lastWriteSize > 1)
            {
                SDMMC_CmdStopTransfer(hsd.Instance);
            }
        }

        while (sdIsBusy() &&
            s2s_elapsedTime_ms(dmaFinishTime) < 180)
        {
            // Wait while the SD card is writing buffer to flash
            // The card may remain in the RECEIVING state (even though it's programming) if
            // it has buffer space to receive more data available.
        }

        if (!disconnected && 
            i + sectors >= totalSDSectors &&
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
            *clearBSY = process_MessageIn(0); // Will go to BUS_FREE state but keep BSY asserted.
        }

        // Wait while the SD card is writing buffer to flash
        // The card may remain in the RECEIVING state (even though it's programming) if
        // it has buffer space to receive more data available.
        while (sdIsBusy()) {}
        HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd);
        while (cardState == HAL_SD_CARD_PROGRAMMING || cardState == HAL_SD_CARD_RECEIVING) 
        {
            // Wait while the SD card is writing buffer to flash
            // The card may remain in the RECEIVING state (even though it's programming) if
            // it has buffer space to receive more data available.
            cardState = HAL_SD_GetCardState(&hsd);
        }

        i += sectors;
   
    }
}

void diskDataOut_variableSectorSize(int sdPerScsi, int totalSDSectors, uint32_t sdLBA, int useSlowDataCount, int* parityError)
{
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

    int i = 0;

    int enableParity = scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY;

    uint32_t maxSectors = sizeof(scsiDev.data) / SD_SECTOR_SIZE;

    while ((i < totalSDSectors) &&
        likely(scsiDev.phase == DATA_OUT) &&
        likely(!scsiDev.resetFlag))
        // KEEP GOING to ensure FIFOs are in a good state.
        // likely(!parityError || !enableParity))
    {
        // Well, until we have some proper non-blocking SD code, we must
        // do this in a half-duplex fashion. We need to write as much as
        // possible in each SD card transaction.
        // use sg_dd from sg_utils3 tools to test.

        uint32_t rem = totalSDSectors - i;
        uint32_t sectors;
        if (rem <= maxSectors)
        {
            sectors = rem;
        }
        else
        {
            sectors = maxSectors;
            while (sectors % sdPerScsi) sectors--;
        }
        

        if (useSlowDataCount)
        {
            scsiSetDataCount((sectors / sdPerScsi) * bytesPerSector);
        }

        for (int scsiSector = i; scsiSector < i + sectors; ++scsiSector)
        {
            int dmaBytes = SD_SECTOR_SIZE;
            if ((scsiSector % sdPerScsi) == (sdPerScsi - 1))
            {
                dmaBytes = bytesPerSector % SD_SECTOR_SIZE;
                if (dmaBytes == 0) dmaBytes = SD_SECTOR_SIZE;
            }

            scsiReadPIO(&scsiDev.data[SD_SECTOR_SIZE * (scsiSector - i)], dmaBytes, parityError);
        }
        if (!(*parityError) || !enableParity)
        {
            BSP_SD_WriteBlocks_DMA(&scsiDev.data[0], i + sdLBA, sectors);
        }
        i += sectors;
    }
}

void diskDataOut()
{
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;

    scsiEnterPhase(DATA_OUT);

    const int sdPerScsi = SDSectorsPerSCSISector(bytesPerSector);
    int totalSDSectors = transfer.blocks * sdPerScsi;
    uint32_t sdLBA =
        SCSISector2SD(
            scsiDev.target->cfg->sdSectorStart,
            bytesPerSector,
            transfer.lba);
    int clearBSY = 0;

    int parityError = 0;

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

    if (bytesPerSector == SD_SECTOR_SIZE)
    {
        diskDataOut_512(totalSDSectors, sdLBA, useSlowDataCount, &clearBSY, &parityError);
    }
    else
    {
        diskDataOut_variableSectorSize(sdPerScsi, totalSDSectors, sdLBA, useSlowDataCount, &parityError);
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


void scsiDiskPoll()
{
    if (scsiDev.phase == DATA_IN &&
        transfer.currentBlock != transfer.blocks)
    {
        diskDataIn();
     }
    else if (scsiDev.phase == DATA_OUT &&
        transfer.currentBlock != transfer.blocks)
    {
        diskDataOut();
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
}

