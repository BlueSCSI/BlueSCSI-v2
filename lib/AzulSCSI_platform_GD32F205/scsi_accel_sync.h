// SCSI subroutines that implement synchronous mode SCSI.
// Uses DMA for data transfer, EXMC for data input and
// GD32 timer for the REQ pin toggling.

#pragma once

#include <stdint.h>
#include "AzulSCSI_platform.h"

#ifdef SCSI_IN_ACK_EXMC_NWAIT_PORT
#define SCSI_SYNC_MODE_AVAILABLE
#endif

void scsi_accel_sync_init();

void scsi_accel_sync_read(uint8_t *data, uint32_t count, int* parityError, volatile int *resetFlag);
void scsi_accel_sync_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag);
void scsi_accel_sync_stopWrite();
void scsi_accel_sync_finishWrite(volatile int *resetFlag);
bool scsi_accel_sync_isWriteFinished(const uint8_t* data);
