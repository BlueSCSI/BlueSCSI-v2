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

#define SCSI_CTRL_IDMASK ((volatile uint8_t*)0x60000000)
#define SCSI_CTRL_PHASE ((volatile uint8_t*)0x60000001)
#define SCSI_CTRL_BSY ((volatile uint8_t*)0x60000002)
#define SCSI_FIFO_SEL ((volatile uint8_t*)0x60000003)
#define SCSI_DATA_CNT_HI ((volatile uint8_t*)0x60000004)
#define SCSI_DATA_CNT_LO ((volatile uint8_t*)0x60000005)
#define SCSI_DATA_CNT_SET ((volatile uint8_t*)0x60000006)

#define SCSI_STS_FIFO ((volatile uint8_t*)0x60000010)
#define SCSI_STS_ALTFIFO ((volatile uint8_t*)0x60000011)
#define SCSI_STS_FIFO_COMPLETE ((volatile uint8_t*)0x60000012)
#define SCSI_STS_SELECTED ((volatile uint8_t*)0x60000013)
#define SCSI_STS_SCSI ((volatile uint8_t*)0x60000014)
#define SCSI_STS_DBX ((volatile uint8_t*)0x60000015)

#define SCSI_FIFO_DATA ((volatile uint8_t*)0x60000020)
#define SCSI_FIFO_DEPTH 512


#define scsiPhyFifoFull() ((*SCSI_STS_FIFO & 0x01) == 0x01)
#define scsiPhyFifoEmpty() ((*SCSI_STS_FIFO & 0x02) == 0x02)
#define scsiPhyFifoAltEmpty() ((*SCSI_STS_ALTFIFO & 0x02) == 0x02)

#define scsiPhyFifoFlip() \
{\
	scsiPhyFifoSel ^= 1; \
	*SCSI_FIFO_SEL = scsiPhyFifoSel; \
}

#define scsiPhyTx(val) *SCSI_FIFO_DATA = (val)
#define scsiPhyRx() *SCSI_FIFO_DATA
#define scsiPhyComplete() ((*SCSI_STS_FIFO_COMPLETE & 0x01) == 0x01)

#define scsiStatusATN() ((*SCSI_STS_SCSI & 0x01) == 0x01)
#define scsiStatusBSY() ((*SCSI_STS_SCSI & 0x02) == 0x02)
#define scsiStatusRST() ((*SCSI_STS_SCSI & 0x04) == 0x04)
#define scsiStatusSEL() ((*SCSI_STS_SCSI & 0x08) == 0x08)
#define scsiStatusACK() ((*SCSI_STS_SCSI & 0x10) == 0x10)

extern uint8_t scsiPhyFifoSel;

void scsiPhyInit(void);
void scsiPhyConfig(void);
void scsiPhyReset(void);

void scsiEnterPhase(int phase);
void scsiEnterBusFree(void);

void scsiWrite(const uint8_t* data, uint32_t count);
void scsiRead(uint8_t* data, uint32_t count);
void scsiWriteByte(uint8_t value);
uint8_t scsiReadByte(void);


void sdTmpRead(uint8_t* data, uint32_t lba, int sectors);
void sdTmpWrite(uint8_t* data, uint32_t lba, int sectors);
#if 0





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

#endif

extern volatile uint8_t scsiRxDMAComplete;
extern volatile uint8_t scsiTxDMAComplete;
#define scsiDMABusy() (!(scsiRxDMAComplete && scsiTxDMAComplete))

void scsiReadDMA(uint8_t* data, uint32_t count);
int scsiReadDMAPoll();

void scsiWriteDMA(const uint8_t* data, uint32_t count);
int scsiWriteDMAPoll();

#if 0
uint8_t scsiReadDBxPins(void);


int scsiSelfTest(void);

#endif

#endif
