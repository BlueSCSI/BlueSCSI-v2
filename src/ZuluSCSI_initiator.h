// Main state machine for SCSI initiator mode

#pragma once

#include <stdint.h>
#include <stdlib.h>

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
