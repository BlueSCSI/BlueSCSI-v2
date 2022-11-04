// Accelerated SCSI subroutines using RP2040 hardware PIO peripheral.

#pragma once

#include <stdint.h>

void scsi_accel_rp2040_init();

// Set SCSI access mode for write requests.
// Setting syncOffset = 0 enables asynchronous SCSI.
// Setting syncOffset > 0 enables synchronous SCSI.
void scsi_accel_rp2040_setWriteMode(int syncOffset, int syncPeriod);

void scsi_accel_rp2040_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag);
void scsi_accel_rp2040_stopWrite(volatile int *resetFlag);
void scsi_accel_rp2040_finishWrite(volatile int *resetFlag);

// Query whether the data at pointer has already been read, i.e. buffer can be reused.
// If data is NULL, checks if all writes have completed.
bool scsi_accel_rp2040_isWriteFinished(const uint8_t* data);

// Read data from SCSI bus.
// Works for both asynchronous and synchronous modes.
void scsi_accel_rp2040_read(uint8_t *buf, uint32_t count, int *parityError, volatile int *resetFlag);
