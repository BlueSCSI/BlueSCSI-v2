/*
BlueSCSI
Copyright (c) 2022-2023 the BlueSCSI contributors (CONTRIBUTORS.txt)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
