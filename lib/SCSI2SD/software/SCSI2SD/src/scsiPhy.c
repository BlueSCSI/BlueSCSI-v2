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
#pragma GCC push_options
#pragma GCC optimize("-flto")

#include "device.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "bits.h"
#include "trace.h"

#define scsiTarget_AUX_CTL (* (reg8 *) scsiTarget_datapath__DP_AUX_CTL_REG)

// DMA controller can't handle any more bytes.
#define MAX_DMA_BYTES 4095

// Private DMA variables.
static int dmaInProgress = 0;
// used when transferring > MAX_DMA_BYTES.
static uint8_t* dmaBuffer = NULL;
static uint32_t dmaSentCount = 0;
static uint32_t dmaTotalCount = 0;

static uint8 scsiDmaRxChan = CY_DMA_INVALID_CHANNEL;
static uint8 scsiDmaTxChan = CY_DMA_INVALID_CHANNEL;

// DMA descriptors
static uint8 scsiDmaRxTd[1] = { CY_DMA_INVALID_TD };
static uint8 scsiDmaTxTd[1] = { CY_DMA_INVALID_TD };

// Source of dummy bytes for DMA reads
static uint8 dummyBuffer = 0xFF;

volatile uint8_t scsiRxDMAComplete;
volatile uint8_t scsiTxDMAComplete;

CY_ISR_PROTO(scsiRxCompleteISR);
CY_ISR(scsiRxCompleteISR)
{
	traceIrq(trace_scsiRxCompleteISR);
	scsiRxDMAComplete = 1;
}

CY_ISR_PROTO(scsiTxCompleteISR);
CY_ISR(scsiTxCompleteISR)
{
	traceIrq(trace_scsiTxCompleteISR);
	scsiTxDMAComplete = 1;
}

CY_ISR_PROTO(scsiResetISR);
CY_ISR(scsiResetISR)
{
	traceIrq(trace_scsiResetISR);
	scsiDev.resetFlag = 1;
}

uint8_t
scsiReadDBxPins()
{
	return
		(SCSI_ReadPin(SCSI_In_DBx_DB7) << 7) |
		(SCSI_ReadPin(SCSI_In_DBx_DB6) << 6) |
		(SCSI_ReadPin(SCSI_In_DBx_DB5) << 5) |
		(SCSI_ReadPin(SCSI_In_DBx_DB4) << 4) |
		(SCSI_ReadPin(SCSI_In_DBx_DB3) << 3) |
		(SCSI_ReadPin(SCSI_In_DBx_DB2) << 2) |
		(SCSI_ReadPin(SCSI_In_DBx_DB1) << 1) |
		SCSI_ReadPin(SCSI_In_DBx_DB0);
}

uint8_t
scsiReadByte(void)
{
	trace(trace_spinPhyTxFifo);
	while (unlikely(scsiPhyTxFifoFull()) && likely(!scsiDev.resetFlag)) {}
	scsiPhyTx(0);

	trace(trace_spinPhyRxFifo);
	while (scsiPhyRxFifoEmpty() && likely(!scsiDev.resetFlag)) {}
	uint8_t val = scsiPhyRx();
	scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();

	trace(trace_spinTxComplete);
	while (!(scsiPhyStatus() & SCSI_PHY_TX_COMPLETE) && likely(!scsiDev.resetFlag)) {}

	return val;
}

static void
scsiReadPIO(uint8* data, uint32 count)
{
	int prep = 0;
	int i = 0;

	while (i < count && likely(!scsiDev.resetFlag))
	{
		uint8_t status = scsiPhyStatus();

		if (prep < count && (status & SCSI_PHY_TX_FIFO_NOT_FULL))
		{
			scsiPhyTx(0);
			++prep;
		}
		if (status & SCSI_PHY_RX_FIFO_NOT_EMPTY)
		{
			data[i] = scsiPhyRx();
			++i;
		}
	}
	scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();
	while (!(scsiPhyStatus() & SCSI_PHY_TX_COMPLETE) && likely(!scsiDev.resetFlag)) {}
}

static void
doRxSingleDMA(uint8* data, uint32 count)
{
	// Prepare DMA transfer
	dmaInProgress = 1;
	trace(trace_doRxSingleDMA);

	CyDmaTdSetConfiguration(
		scsiDmaTxTd[0],
		count,
		CY_DMA_DISABLE_TD, // Disable the DMA channel when TD completes count bytes
		SCSI_TX_DMA__TD_TERMOUT_EN // Trigger interrupt when complete
		);
	CyDmaTdSetConfiguration(
		scsiDmaRxTd[0],
		count,
		CY_DMA_DISABLE_TD, // Disable the DMA channel when TD completes count bytes
		TD_INC_DST_ADR |
			SCSI_RX_DMA__TD_TERMOUT_EN // Trigger interrupt when complete
		);

	CyDmaTdSetAddress(
		scsiDmaTxTd[0],
		LO16((uint32)&dummyBuffer),
		LO16((uint32)scsiTarget_datapath__F0_REG));
	CyDmaTdSetAddress(
		scsiDmaRxTd[0],
		LO16((uint32)scsiTarget_datapath__F1_REG),
		LO16((uint32)data)
		);

	CyDmaChSetInitialTd(scsiDmaTxChan, scsiDmaTxTd[0]);
	CyDmaChSetInitialTd(scsiDmaRxChan, scsiDmaRxTd[0]);

	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaClearPendingDrq(scsiDmaTxChan);
	CyDmaClearPendingDrq(scsiDmaRxChan);

	scsiTxDMAComplete = 0;
	scsiRxDMAComplete = 0;

	CyDmaChEnable(scsiDmaRxChan, 1);
	CyDmaChEnable(scsiDmaTxChan, 1);
}

void
scsiReadDMA(uint8* data, uint32 count)
{
	dmaSentCount = 0;
	dmaTotalCount = count;
	dmaBuffer = data;

	uint32_t singleCount = (count > MAX_DMA_BYTES) ? MAX_DMA_BYTES : count;
	doRxSingleDMA(data, singleCount);
	dmaSentCount += count;
}

int
scsiReadDMAPoll()
{
	if (scsiTxDMAComplete && scsiRxDMAComplete)
	{
		// Wait until our scsi signals are consistent. This should only be
		// a few cycles.
		trace(trace_spinTxComplete);
		while (!(scsiPhyStatus() & SCSI_PHY_TX_COMPLETE)) {}

		if (likely(dmaSentCount == dmaTotalCount))
		{
			dmaInProgress = 0;
			scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();
			return 1;
		}
		else
		{
			// Transfer was too large for a single DMA transfer. Continue
			// to send remaining bytes.
			uint32_t count = dmaTotalCount - dmaSentCount;
			if (unlikely(count > MAX_DMA_BYTES)) count = MAX_DMA_BYTES;
			doRxSingleDMA(dmaBuffer + dmaSentCount, count);
			dmaSentCount += count;
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

void
scsiRead(uint8_t* data, uint32_t count)
{
	if (count < 8)
	{
		scsiReadPIO(data, count);
	}
	else
	{
		scsiReadDMA(data, count);

		// Wait for the next DMA interrupt (or the 1ms systick)
		// It's beneficial to halt the processor to
		// give the DMA controller more memory bandwidth to work with.
		__WFI();

		trace(trace_spinReadDMAPoll);
		while (!scsiReadDMAPoll() && likely(!scsiDev.resetFlag)) {};
	}
}

void
scsiWriteByte(uint8 value)
{
	trace(trace_spinPhyTxFifo);
	while (unlikely(scsiPhyTxFifoFull()) && likely(!scsiDev.resetFlag)) {}
	scsiPhyTx(value);

	trace(trace_spinTxComplete);
	while (!(scsiPhyStatus() & SCSI_PHY_TX_COMPLETE) && likely(!scsiDev.resetFlag)) {}
	scsiPhyRxFifoClear();
}

static void
scsiWritePIO(const uint8_t* data, uint32_t count)
{
	int i = 0;

	while (i < count && likely(!scsiDev.resetFlag))
	{
		if (!scsiPhyTxFifoFull())
		{
			scsiPhyTx(data[i]);
			++i;
		}
	}

	trace(trace_spinTxComplete);
	while (!(scsiPhyStatus() & SCSI_PHY_TX_COMPLETE) && likely(!scsiDev.resetFlag)) {}
	scsiPhyRxFifoClear();
}

static void
doTxSingleDMA(const uint8* data, uint32 count)
{
	// Prepare DMA transfer
	dmaInProgress = 1;
	trace(trace_doTxSingleDMA);

	CyDmaTdSetConfiguration(
		scsiDmaTxTd[0],
		count,
		CY_DMA_DISABLE_TD, // Disable the DMA channel when TD completes count bytes
		TD_INC_SRC_ADR |
			SCSI_TX_DMA__TD_TERMOUT_EN // Trigger interrupt when complete
		);
	CyDmaTdSetAddress(
		scsiDmaTxTd[0],
		LO16((uint32)data),
		LO16((uint32)scsiTarget_datapath__F0_REG));
	CyDmaChSetInitialTd(scsiDmaTxChan, scsiDmaTxTd[0]);

	// The DMA controller is a bit trigger-happy. It will retain
	// a drq request that was triggered while the channel was
	// disabled.
	CyDmaClearPendingDrq(scsiDmaTxChan);

	scsiTxDMAComplete = 0;
	scsiRxDMAComplete = 1;

	CyDmaChEnable(scsiDmaTxChan, 1);
}

void
scsiWriteDMA(const uint8* data, uint32 count)
{
	dmaSentCount = 0;
	dmaTotalCount = count;
	dmaBuffer = data;

	uint32_t singleCount = (count > MAX_DMA_BYTES) ? MAX_DMA_BYTES : count;
	doTxSingleDMA(data, singleCount);
	dmaSentCount += count;
}

int
scsiWriteDMAPoll()
{
	if (scsiTxDMAComplete)
	{
		// Wait until our scsi signals are consistent. This should only be
		// a few cycles.
		trace(trace_spinTxComplete);
		while (!(scsiPhyStatus() & SCSI_PHY_TX_COMPLETE)) {}

		if (likely(dmaSentCount == dmaTotalCount))
		{
			scsiPhyRxFifoClear();
			dmaInProgress = 0;
			return 1;
		}
		else
		{
			// Transfer was too large for a single DMA transfer. Continue
			// to send remaining bytes.
			uint32_t count = dmaTotalCount - dmaSentCount;
			if (unlikely(count > MAX_DMA_BYTES)) count = MAX_DMA_BYTES;
			doTxSingleDMA(dmaBuffer + dmaSentCount, count);
			dmaSentCount += count;
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

void
scsiWrite(const uint8_t* data, uint32_t count)
{
	if (count < 8)
	{
		scsiWritePIO(data, count);
	}
	else
	{
		scsiWriteDMA(data, count);

		// Wait for the next DMA interrupt (or the 1ms systick)
		// It's beneficial to halt the processor to
		// give the DMA controller more memory bandwidth to work with.
		__WFI();

		trace(trace_spinWriteDMAPoll);
		while (!scsiWriteDMAPoll() && likely(!scsiDev.resetFlag)) {};
	}
}

static inline void busSettleDelay(void)
{
	// Data Release time (switching IO) = 400ns
	// + Bus Settle time (switching phase) = 400ns.
	CyDelayUs(1); // Close enough.
}

void scsiEnterPhase(int phase)
{
	int newPhase = phase > 0 ? phase : 0;
	if (newPhase != SCSI_CTL_PHASE_Read())
	{
		SCSI_CTL_PHASE_Write(phase > 0 ? phase : 0);
		busSettleDelay();
	}
}

void scsiPhyReset()
{
	trace(trace_scsiPhyReset);
	if (dmaInProgress)
	{
		dmaInProgress = 0;
		dmaBuffer = NULL;
		dmaSentCount = 0;
		dmaTotalCount = 0;
		CyDmaChSetRequest(scsiDmaTxChan, CY_DMA_CPU_TERM_CHAIN);
		CyDmaChSetRequest(scsiDmaRxChan, CY_DMA_CPU_TERM_CHAIN);
		
		// CyDmaChGetRequest returns 0 for the relevant bit once the
		// request is completed.
		trace(trace_spinDMAReset);
		while (CyDmaChGetRequest(scsiDmaTxChan) & CY_DMA_CPU_TERM_CHAIN) {}
		while (CyDmaChGetRequest(scsiDmaRxChan) & CY_DMA_CPU_TERM_CHAIN) {}

		CyDmaChDisable(scsiDmaTxChan);
		CyDmaChDisable(scsiDmaRxChan);
	}

	// Set the Clear bits for both SCSI device FIFOs
	scsiTarget_AUX_CTL = scsiTarget_AUX_CTL | 0x03;

	// Trigger RST outselves.  It is connected to the datapath and will
	// ensure it returns to the idle state.  The datapath runs at the BUS clk
	// speed (ie. same as the CPU), so we can be sure it is active for a sufficient
	// duration.
	SCSI_RST_ISR_Disable();
	SCSI_SetPin(SCSI_Out_RST);

	SCSI_CTL_PHASE_Write(0);
	SCSI_ClearPin(SCSI_Out_ATN);
	SCSI_ClearPin(SCSI_Out_BSY);
	SCSI_ClearPin(SCSI_Out_ACK);
	SCSI_ClearPin(SCSI_Out_RST);
	SCSI_ClearPin(SCSI_Out_SEL);
	SCSI_ClearPin(SCSI_Out_REQ);

	// Allow the FIFOs to fill up again.
	SCSI_ClearPin(SCSI_Out_RST);
	SCSI_RST_ISR_Enable();
	scsiTarget_AUX_CTL = scsiTarget_AUX_CTL & ~(0x03);

	SCSI_Parity_Error_Read(); // clear sticky bits
}

static void scsiPhyInitDMA()
{
	// One-time init only.
	if (scsiDmaTxChan == CY_DMA_INVALID_CHANNEL)
	{
		scsiDmaRxChan =
			SCSI_RX_DMA_DmaInitialize(
				1, // Bytes per burst
				1, // request per burst
				HI16(CYDEV_PERIPH_BASE),
				HI16(CYDEV_SRAM_BASE)
				);

		scsiDmaTxChan =
			SCSI_TX_DMA_DmaInitialize(
				1, // Bytes per burst
				1, // request per burst
				HI16(CYDEV_SRAM_BASE),
				HI16(CYDEV_PERIPH_BASE)
				);
		
		CyDmaChDisable(scsiDmaRxChan);
		CyDmaChDisable(scsiDmaTxChan);

		scsiDmaRxTd[0] = CyDmaTdAllocate();
		scsiDmaTxTd[0] = CyDmaTdAllocate();

		SCSI_RX_DMA_COMPLETE_StartEx(scsiRxCompleteISR);
		SCSI_TX_DMA_COMPLETE_StartEx(scsiTxCompleteISR);
	}
}


void scsiPhyInit()
{
	scsiPhyInitDMA();

	SCSI_RST_ISR_StartEx(scsiResetISR);
}

// 1 = DBx error
// 2 = Parity error
// 4 = MSG error
// 8 = CD error
// 16 = IO error
// 32 = other error
int scsiSelfTest()
{
	int result = 0;

	// TEST DBx and DBp
	int i;
	SCSI_Out_Ctl_Write(1); // Write bits manually.
	SCSI_CTL_PHASE_Write(__scsiphase_io); // Needed for parity generation
	for (i = 0; i < 256; ++i)
	{
		SCSI_Out_Bits_Write(i);
		scsiDeskewDelay();
		if (scsiReadDBxPins() != (i & 0xff))
		{
			result |= 1;
		}
		if (Lookup_OddParity[i & 0xff] != SCSI_ReadPin(SCSI_In_DBP))
		{
			result |= 2;
		}
	}
	SCSI_Out_Ctl_Write(0); // Write bits normally.

	// TEST MSG, CD, IO
	for (i = 0; i < 8; ++i)
	{
		SCSI_CTL_PHASE_Write(i);
		scsiDeskewDelay();

		if (SCSI_ReadPin(SCSI_In_MSG) != !!(i & __scsiphase_msg))
		{
			result |= 4;
		}
		if (SCSI_ReadPin(SCSI_In_CD) != !!(i & __scsiphase_cd))
		{
			result |= 8;
		}
		if (SCSI_ReadPin(SCSI_In_IO) != !!(i & __scsiphase_io))
		{
			result |= 16;
		}
	}
	SCSI_CTL_PHASE_Write(0);

	uint32_t signalsOut[] = { SCSI_Out_ATN, SCSI_Out_BSY, SCSI_Out_RST, SCSI_Out_SEL };
	uint32_t signalsIn[] = { SCSI_Filt_ATN, SCSI_Filt_BSY, SCSI_Filt_RST, SCSI_Filt_SEL };

	for (i = 0; i < 4; ++i)
	{
		SCSI_SetPin(signalsOut[i]);
		scsiDeskewDelay();

		int j;
		for (j = 0; j < 4; ++j)
		{
			if (i == j)
			{
				if (! SCSI_ReadFilt(signalsIn[j]))
				{
					result |= 32;
				}
			}
			else
			{
				if (SCSI_ReadFilt(signalsIn[j]))
				{
					result |= 32;
				}
			}
		}
		SCSI_ClearPin(signalsOut[i]);
	}
	return result;
}


#pragma GCC pop_options
