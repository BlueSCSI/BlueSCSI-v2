/*
BlueSCSI
Copyright (c) 2022-2023 the BlueSCSI contributors (CONTRIBUTORS.txt)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Interface to SCSI physical interface.
// This file is derived from scsiPhy.h in SCSI2SD-V6.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read SCSI status signals
bool scsiStatusATN();
bool scsiStatusBSY();
bool scsiStatusSEL();

// Parity not yet implemented
#define scsiParityError() 0

// Get SCSI selection status.
// This is latched by interrupt when BSY is deasserted while SEL is asserted.
// Lowest 3 bits are the selected target id.
// Highest bits are status information.
#define SCSI_STS_SELECTION_SUCCEEDED 0x40
#define SCSI_STS_SELECTION_ATN 0x80
extern volatile uint8_t g_scsi_sts_selection;
#define SCSI_STS_SELECTED (&g_scsi_sts_selection)
extern volatile uint8_t g_scsi_ctrl_bsy;
#define SCSI_CTRL_BSY (&g_scsi_ctrl_bsy)

// Called when SCSI RST signal has been asserted, should release bus.
void scsiPhyReset(void);

// Change MSG / CD / IO signal states and wait for necessary transition time.
// Phase argument is one of SCSI_PHASE enum values.
void scsiEnterPhase(int phase);

// Change state and return nanosecond delay to wait
uint32_t scsiEnterPhaseImmediate(int phase);

// Release all signals
void scsiEnterBusFree(void);

// Blocking data transfer
void scsiWrite(const uint8_t* data, uint32_t count);
void scsiRead(uint8_t* data, uint32_t count, int* parityError);
void scsiWriteByte(uint8_t value);
uint8_t scsiReadByte(void);

// Non-blocking data transfer.
// Depending on platform support the start() function may block.
// The start function can be called multiple times, it may internally
// either combine transfers or block until previous transfer completes.
void scsiStartWrite(const uint8_t* data, uint32_t count);
void scsiFinishWrite();

// Query whether the data at pointer has already been read, i.e. buffer can be reused.
// If data is NULL, checks if all writes have completed.
bool scsiIsWriteFinished(const uint8_t *data);


#define s2s_getScsiRateKBs() 0

#ifdef __cplusplus
}
#endif
