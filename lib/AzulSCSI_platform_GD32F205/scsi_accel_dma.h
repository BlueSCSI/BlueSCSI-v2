// SCSI subroutines that use hardware DMA for transfer in the background.

#pragma once

#include <stdint.h>
#include "AzulSCSI_platform.h"

#ifdef SCSI_TIMER
// TODO: This works, but without external logic does not improve performance compared
// to software bitbang.
// #define SCSI_ACCEL_DMA_AVAILABLE 1

void scsi_accel_dma_init();
void scsi_accel_dma_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag);
void scsi_accel_dma_stopWrite();
void scsi_accel_dma_finishWrite(volatile int *resetFlag);

// Query whether the data at pointer has already been read, i.e. buffer can be reused.
// If data is NULL, checks if all writes have completed.
bool scsi_accel_dma_isWriteFinished(const uint8_t* data);


#endif
