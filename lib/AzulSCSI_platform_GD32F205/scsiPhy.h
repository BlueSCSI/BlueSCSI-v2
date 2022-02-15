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

//void scsiSetDataCount(uint32_t count);
//int scsiFifoReady(void);

void scsiWrite(const uint8_t* data, uint32_t count);
void scsiRead(uint8_t* data, uint32_t count, int* parityError);
void scsiWriteByte(uint8_t value);
uint8_t scsiReadByte(void);

#define s2s_getScsiRateKBs() 0

#ifdef __cplusplus
}
#endif
