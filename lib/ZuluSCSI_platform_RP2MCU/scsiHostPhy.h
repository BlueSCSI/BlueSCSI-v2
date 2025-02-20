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

// Host side SCSI physical interface.
// Used in initiator to interface to an SCSI drive.

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Request to stop activity and reset the bus
extern volatile int g_scsiHostPhyReset;

// Release bus and pulse RST signal, initialize PHY to host mode.
void scsiHostPhyReset(void);

// Select a device, id 0-7.
// target_id - target device id 0-7
// initiator_id - host device id 0-7
// Returns true if the target answers to selection request.
bool scsiHostPhySelect(int target_id, uint8_t initiator_id);

// Read the current communication phase as signaled by the target
// Matches SCSI_PHASE enumeration from scsi.h.
int scsiHostPhyGetPhase();

// Returns true if the device has asserted REQ signal, i.e. data waiting
bool scsiHostRequestWaiting();

// Blocking data transfer
// These return the actual number of bytes transferred.
uint32_t scsiHostWrite(const uint8_t *data, uint32_t count);
uint32_t scsiHostRead(uint8_t *data, uint32_t count);

// Release bus signals and expect the target to do the same.
// Cycles ACK in case target still holds BSY and REQ.
void scsiHostWaitBusFree();

// Release all bus signals
void scsiHostPhyRelease();
