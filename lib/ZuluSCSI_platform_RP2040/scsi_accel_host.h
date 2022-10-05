// Accelerated SCSI subroutines for SCSI initiator/host side communication

#pragma once

#include <stdint.h>

void scsi_accel_host_init();

// Read data from SCSI bus.
// Number of bytes to read must be divisible by two.
uint32_t scsi_accel_host_read(uint8_t *buf, uint32_t count, int *parityError, volatile int *resetFlag);
