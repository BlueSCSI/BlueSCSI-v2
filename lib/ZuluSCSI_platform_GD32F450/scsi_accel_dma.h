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

// SCSI subroutines that use hardware DMA for transfer in the background.
// Uses either GD32 timer or external GreenPAK to implement REQ pin toggling.

#pragma once

#include <stdint.h>
#include "ZuluSCSI_platform.h"
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


