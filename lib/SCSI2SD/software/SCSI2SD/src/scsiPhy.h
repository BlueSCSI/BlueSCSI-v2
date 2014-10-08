//	Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.
#ifndef SCSIPHY_H
#define SCSIPHY_H

// Definitions to match the scsiTarget status register.
typedef enum
{
	SCSI_PHY_TX_FIFO_NOT_FULL =  0x01,
	SCSI_PHY_RX_FIFO_NOT_EMPTY = 0x02,

	// The TX FIFO is empty and the state machine is in the idle state
	SCSI_PHY_TX_COMPLETE = 0x10
} SCSI_PHY_STATE;

#define scsiPhyStatus() CY_GET_REG8(scsiTarget_StatusReg__STATUS_REG)
#define scsiPhyTxFifoFull() ((scsiPhyStatus() & SCSI_PHY_TX_FIFO_NOT_FULL) == 0)
#define scsiPhyRxFifoEmpty() ((scsiPhyStatus() & SCSI_PHY_RX_FIFO_NOT_EMPTY) == 0)

// Clear 4 byte fifo
#define scsiPhyRxFifoClear() scsiPhyRx(); scsiPhyRx(); scsiPhyRx(); scsiPhyRx();

#define scsiPhyTx(val) CY_SET_REG8(scsiTarget_datapath__F0_REG, (val))
#define scsiPhyRx() CY_GET_REG8(scsiTarget_datapath__F1_REG)

#define SCSI_SetPin(pin) \
	CyPins_SetPin((pin));

#define SCSI_ClearPin(pin) \
	CyPins_ClearPin((pin));

// Active low: we interpret a 0 as "true", and non-zero as "false"
#define SCSI_ReadPin(pin) \
	(CyPins_ReadPin((pin)) == 0)

// These signals go through a glitch filter - we do not access the pin
// directly
enum FilteredInputs
{
	SCSI_Filt_ATN = 0x01,
	SCSI_Filt_BSY = 0x02,
	SCSI_Filt_SEL = 0x04,
	SCSI_Filt_RST = 0x08,
	SCSI_Filt_ACK = 0x10		
};
#define SCSI_ReadFilt(filt) \
	((SCSI_Filtered_Read() & (filt)) == 0)

// SCSI delays, as referenced to the cpu clock
#define CPU_CLK_PERIOD_NS (1000000000U / BCLK__BUS_CLK__HZ)
#define scsiDeskewDelay() CyDelayCycles((55 / CPU_CLK_PERIOD_NS) + 1)

// Contains the odd-parity flag for a given 8-bit value.
extern const uint8_t Lookup_OddParity[256];

void scsiPhyReset(void);
void scsiPhyInit(void);

uint8_t scsiReadByte(void);
void scsiRead(uint8_t* data, uint32_t count);
void scsiReadDMA(uint8_t* data, uint32_t count);
int scsiReadDMAPoll();

void scsiWriteByte(uint8_t value);
void scsiWrite(uint8_t* data, uint32_t count);
void scsiWriteDMA(uint8_t* data, uint32_t count);
int scsiWriteDMAPoll();

uint8_t scsiReadDBxPins(void);

void scsiEnterPhase(int phase);

#endif
