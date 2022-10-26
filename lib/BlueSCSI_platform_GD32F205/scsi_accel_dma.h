// SCSI subroutines that use hardware DMA for transfer in the background.
// Uses either GD32 timer or external GreenPAK to implement REQ pin toggling.

#pragma once

#include <stdint.h>
#include "BlueSCSI_platform.h"
#include "greenpak.h"

#ifdef SCSI_TIMER
#define SCSI_ACCEL_DMA_AVAILABLE 1
#endif

/* Initialization function decides whether to use timer or GreenPAK */
void scsi_accel_greenpak_dma_init();
void scsi_accel_timer_dma_init();

/* Common functions */
void scsi_accel_dma_startWrite(const uint8_t* data, uint32_t count, volatile int *resetFlag);
void scsi_accel_dma_stopWrite();
void scsi_accel_dma_finishWrite(volatile int *resetFlag);

// Query whether the data at pointer has already been read, i.e. buffer can be reused.
// If data is NULL, checks if all writes have completed.
bool scsi_accel_dma_isWriteFinished(const uint8_t* data);


