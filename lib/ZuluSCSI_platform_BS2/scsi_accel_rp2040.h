/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
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

// Accelerated SCSI subroutines using RP2040 hardware PIO peripheral.

#pragma once

#include <stdint.h>

void scsi_accel_rp2040_init();

// Set SCSI access mode for synchronous transfers
// Setting syncOffset = 0 enables asynchronous SCSI.
// Setting syncOffset > 0 enables synchronous SCSI.
void scsi_accel_rp2040_setSyncMode(int syncOffset, int syncPeriod);

// Queue a request to write data from the buffer to SCSI bus.
// This function typically returns immediately and the request will complete in background.
// If there are too many queued requests, this function will block until previous request finishes.
void scsi_accel_rp2040_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag);

// Query whether the data at pointer has already been read, i.e. buffer can be reused.
// If data is NULL, checks if all writes have completed.
bool scsi_accel_rp2040_isWriteFinished(const uint8_t* data);

// Wait for all write requests to finish and release the bus.
// If resetFlag is non-zero, aborts write immediately.
void scsi_accel_rp2040_finishWrite(volatile int *resetFlag);

// Queue a request to read data from SCSI bus to the buffer.
// This function typically returns immediately and the request will complete in background.
// If there are too many queued requests, this function will block until previous request finishes.
void scsi_accel_rp2040_startRead(uint8_t *data, uint32_t count, int *parityError, volatile int *resetFlag);

// Query whether data at address is part of a queued read request.
// Returns true if there is no outstanding request.
// If data is NULL, checks if all reads have completed.
bool scsi_accel_rp2040_isReadFinished(const uint8_t* data);

// Wait for a read request to complete.
// If buf is not NULL, waits only until the data at data[0] .. data[count-1] is valid.
// If buf is NULL, waits for all read requests to complete.
// If there are no further read requests, releases the bus.
// If resetFlag is non-zero, aborts read immediately.
// If a parity error has been noticed in any buffer since starting the read, parityError is set to 1.
void scsi_accel_rp2040_finishRead(const uint8_t *data, uint32_t count, int *parityError, volatile int *resetFlag);

