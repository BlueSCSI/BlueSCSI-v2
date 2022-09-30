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

// Read a block of data from SCSI device and write to file on SD card
class FsFile;
bool scsiInitiatorReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
                                 FsFile &file);
