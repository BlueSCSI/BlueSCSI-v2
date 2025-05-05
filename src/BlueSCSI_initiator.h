/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// Main state machine for SCSI initiator mode

#pragma once

#include <stdint.h>
#include <stdlib.h>

#define SCSI_DEVICE_TYPE_CD 0x5
#define SCSI_DEVICE_TYPE_MO 0x7
#define SCSI_DEVICE_TYPE_DIRECT_ACCESS 0x0

void scsiInitiatorInit();

void scsiInitiatorMainLoop();

// Get the SCSI ID used by the initiator itself
int scsiInitiatorGetOwnID();

// Select target and execute SCSI command
// If timeout is non-zero, it is added to the default watchdog timeout.
int scsiInitiatorRunCommand(int target_id,
                            const uint8_t *command, size_t cmdLen,
                            uint8_t *bufIn, size_t bufInLen,
                            const uint8_t *bufOut, size_t bufOutLen,
                            bool returnDataPhase = false,
                            uint32_t timeout = 30000);

// Detect support of read10 commands.
// Returns true if supported.
// Return value can be overridden by .ini file, in which case test is not done.
bool scsiInitiatorTestSupportsRead10(int target_id, uint32_t sectorsize);

// Execute READ CAPACITY command
bool scsiInitiatorReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize);

// Execute REQUEST SENSE command to get more information about error status
bool scsiRequestSense(int target_id, uint8_t *sense_key, uint8_t *sense_asc = nullptr, uint8_t *sense_ascq = nullptr);

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
