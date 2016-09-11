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
#define SCSI_CTRL_PHASE ((volatile uint8_t*)0x60000002)
#define SCSI_CTRL_BSY ((volatile uint8_t*)0x60000004)
#define SCSI_FIFO_SEL ((volatile uint8_t*)0x60000006)
#define SCSI_DATA_CNT_HI ((volatile uint8_t*)0x60000008)
#define SCSI_DATA_CNT_LO ((volatile uint8_t*)0x6000000A)
#define SCSI_DATA_CNT_SET ((volatile uint8_t*)0x6000000C)
#define SCSI_CTRL_DBX ((volatile uint8_t*)0x6000000E)
#define SCSI_CTRL_SYNC_OFFSET ((volatile uint8_t*)0x60000010)
#define SCSI_CTRL_TIMING ((volatile uint8_t*)0x60000012)
#define SCSI_CTRL_TIMING2 ((volatile uint8_t*)0x60000014)

#define SCSI_STS_FIFO ((volatile uint8_t*)0x60000020)
#define SCSI_STS_ALTFIFO ((volatile uint8_t*)0x60000022)
#define SCSI_STS_FIFO_COMPLETE ((volatile uint8_t*)0x60000024)
#define SCSI_STS_SELECTED ((volatile uint8_t*)0x60000026)
#define SCSI_STS_SCSI ((volatile uint8_t*)0x60000028)
#define SCSI_STS_DBX ((volatile uint8_t*)0x6000002A)

#define SCSI_FIFO_DATA ((volatile uint16_t*)0x60000040)
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

// Disable DMA due to errate with the STM32F205 DMA2 controller when
// concurrently transferring FSMC (with FIFO) and APB (ie. sdio)
// peripherals.
#undef SCSI_FSMC_DMA

extern uint8_t scsiPhyFifoSel;

void scsiPhyInit(void);
void scsiPhyConfig(void);
void scsiPhyReset(void);

void scsiEnterPhase(int phase);
void scsiEnterBusFree(void);

void scsiSetDataCount(uint32_t count);

void scsiWrite(const uint8_t* data, uint32_t count);
void scsiRead(uint8_t* data, uint32_t count);
void scsiWriteByte(uint8_t value);
uint8_t scsiReadByte(void);


void sdTmpRead(uint8_t* data, uint32_t lba, int sectors);
void sdTmpWrite(uint8_t* data, uint32_t lba, int sectors);

extern volatile uint8_t scsiRxDMAComplete;
extern volatile uint8_t scsiTxDMAComplete;
#define scsiDMABusy() (!(scsiRxDMAComplete && scsiTxDMAComplete))

void scsiReadDMA(uint8_t* data, uint32_t count);
int scsiReadDMAPoll();

void scsiWriteDMA(const uint8_t* data, uint32_t count);
int scsiWriteDMAPoll();

int scsiSelfTest(void);

#endif
