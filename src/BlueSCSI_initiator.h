// Main state machine for SCSI initiator mode

#pragma once

#include "SdFat.h"
#include <stdint.h>
#include <stdlib.h>

#define DEVICE_TYPE_CD 5
#define DEVICE_TYPE_DIRECT_ACCESS 0

void scsiInitiatorInit();

void scsiInitiatorMainLoop();

// Select target and execute SCSI command
int scsiInitiatorRunCommand(int target_id,
                            const uint8_t *command, size_t cmdLen,
                            uint8_t *bufIn, size_t bufInLen,
                            const uint8_t *bufOut, size_t bufOutLen,
                            bool returnDataPhase = false);

// Execute READ CAPACITY command
bool scsiInitiatorReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize);

// Execute REQUEST SENSE command to get more information about error status
bool scsiRequestSense(int target_id, uint8_t *sense_key);

// Execute UNIT START STOP command to load/unload media
bool scsiStartStopUnit(int target_id, bool start);

// Execute INQUIRY command
bool scsiInquiry(int target_id, uint8_t inquiry_data[36]);

// Execute TEST UNIT READY command and handle unit attention state
bool scsiTestUnitReady(int target_id);

// Read a block of data from SCSI device and write to file on SD card
class FsFile;
bool scsiInitiatorReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
                                 FsFile &file);

/*************************************
 * High level initiator mode logic   *
 *************************************/

static struct {
    // Bitmap of all drives that have been imaged
    uint32_t drives_imaged;

    uint8_t initiator_id;

    // Is imaging a drive in progress, or are we scanning?
    bool imaging;

    // Information about currently selected drive
    int target_id;
    uint32_t sectorsize;
    uint32_t sectorcount;
    uint32_t sectorcount_all;
    uint32_t sectors_done;
    uint32_t max_sector_per_transfer;
    uint32_t badSectorCount;
    uint8_t ansiVersion;
    uint8_t maxRetryCount;
    uint8_t deviceType;

    // Retry information for sector reads.
    // If a large read fails, retry is done sector-by-sector.
    int retrycount;
    uint32_t failposition;
    bool ejectWhenDone;

    FsFile target_file;
} g_initiator_state;