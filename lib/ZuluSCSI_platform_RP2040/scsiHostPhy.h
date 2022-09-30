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
// Returns true if the target answers to selection request.
bool scsiHostPhySelect(int target_id);

// Read the current communication phase as signaled by the target
// Matches SCSI_PHASE enumeration from scsi.h.
int scsiHostPhyGetPhase();

// Returns true if the device has asserted REQ signal, i.e. data waiting
bool scsiHostRequestWaiting();

// Blocking data transfer
bool scsiHostWrite(const uint8_t *data, uint32_t count);
bool scsiHostRead(uint8_t *data, uint32_t count);

// Release all bus signals
void scsiHostPhyRelease();
