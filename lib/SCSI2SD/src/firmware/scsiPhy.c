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

#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal_dma.h"

#include "scsi.h"
#include "scsiPhy.h"
#include "trace.h"
#include "time.h"
#include "fpga.h"
#include "led.h"

#include <string.h>

// Private DMA variables.
static int dmaInProgress = 0;

static DMA_HandleTypeDef memToFSMC;
static DMA_HandleTypeDef fsmcToMem;


volatile uint8_t scsiRxDMAComplete;
volatile uint8_t scsiTxDMAComplete;

#if 0
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
#endif

uint8_t scsiPhyFifoSel = 0; // global

// scsi IRQ handler is initialised by the STM32 HAL. Connected to
// PE4
// Note: naming is important to ensure this function is listed in the
// vector table.
void EXTI4_IRQHandler()
{
	traceIrq(trace_scsiResetISR);

	// Make sure that interrupt flag is set
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != RESET) {

		// Clear interrupt flag
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);

		scsiDev.resetFlag = scsiDev.resetFlag || scsiStatusRST();
		// TODO grab SEL status as well

	}
}

static void assertFail()
{
	while (1)
	{
		s2s_ledOn();
		s2s_delay_ms(100);
		s2s_ledOff();
		s2s_delay_ms(100);
	}
}

static void
startScsiRx(uint32_t count)
{
	*SCSI_DATA_CNT_HI = count >> 8;
	*SCSI_DATA_CNT_LO = count & 0xff;
	*SCSI_DATA_CNT_SET = 1;
}

uint8_t
scsiReadByte(void)
{
#if FIFODEBUG
	if (!scsiPhyFifoAltEmpty()) {
		// Force a lock-up.
		assertFail();
	}
#endif
	startScsiRx(1);

	trace(trace_spinPhyRxFifo);
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag)) {}
	scsiPhyFifoFlip();
	uint8_t val = scsiPhyRx();
	// TODO scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();

#if FIFODEBUG
	if (!scsiPhyFifoEmpty()) {
		int j = 0;
		uint8_t k __attribute((unused));
		while (!scsiPhyFifoEmpty()) { k = scsiPhyRx(); ++j; }

		// Force a lock-up.
		assertFail();
	}
#endif
	return val;
}


static void
scsiReadPIO(uint8_t* data, uint32_t count)
{
	for (int i = 0; i < count; ++i)
	{
		data[i] = scsiPhyRx();
	}
	// TODO scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();
}

void
scsiReadDMA(uint8_t* data, uint32_t count)
{
	// Prepare DMA transfer
	dmaInProgress = 1;
	trace(trace_doRxSingleDMA);

	scsiTxDMAComplete = 1; // TODO not used much
	scsiRxDMAComplete = 0; // TODO not used much

	HAL_DMA_Start(&fsmcToMem, (uint32_t) SCSI_FIFO_DATA, (uint32_t) data, count);
}

int
scsiReadDMAPoll()
{
	int complete = __HAL_DMA_GET_COUNTER(&fsmcToMem) == 0;
	complete = complete && (HAL_DMA_PollForTransfer(&fsmcToMem, HAL_DMA_FULL_TRANSFER, 0xffffffff) == HAL_OK);
	if (complete)
	{
		scsiTxDMAComplete = 1; // TODO MM FIX IRQ
		scsiRxDMAComplete = 1;

		dmaInProgress = 0;
#if 0
		// TODO MM scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();
#endif
		return 1;

	}
	else
	{
		return 0;
	}
}

void
scsiRead(uint8_t* data, uint32_t count)
{
	int i = 0;
	while (i < count && likely(!scsiDev.resetFlag))
	{
		uint32_t chunk = ((count - i) > SCSI_FIFO_DEPTH)
			? SCSI_FIFO_DEPTH : (count - i);

		if (chunk >= 16)
		{
			// DMA is doing 32bit transfers.
			chunk = chunk & 0xFFFFFFF8;
		}

#if FIFODEBUG
		if (!scsiPhyFifoAltEmpty()) {
			// Force a lock-up.
			assertFail();
		}
#endif

		startScsiRx(chunk);
		// Wait for the next scsi interrupt (or the 1ms systick)
		__WFI();

		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag)) {}
		scsiPhyFifoFlip();

		if (chunk < 16)
		{
			scsiReadPIO(data + i, chunk);
		}
		else
		{
			scsiReadDMA(data + i, chunk);

			// Wait for the next DMA interrupt (or the 1ms systick)
			// It's beneficial to halt the processor to
			// give the DMA controller more memory bandwidth to work with.
#if 0
			__WFI();
#endif
			trace(trace_spinReadDMAPoll);

			while (!scsiReadDMAPoll() && likely(!scsiDev.resetFlag))
			{
#if 0
// TODO NEED SCSI DMA IRQs
			__WFI();
#endif
			};
		}

#if FIFODEBUG
		if (!scsiPhyFifoEmpty()) {
			int j = 0;
			while (!scsiPhyFifoEmpty()) { scsiPhyRx(); ++j; }
			// Force a lock-up.
			assertFail();
		}
#endif
		i += chunk;
	}
}

void
scsiWriteByte(uint8_t value)
{
#if FIFODEBUG
	if (!scsiPhyFifoEmpty()) {
		// Force a lock-up.
		assertFail();
	}
#endif
	trace(trace_spinPhyTxFifo);
	scsiPhyTx(value);
	scsiPhyFifoFlip();

	trace(trace_spinTxComplete);
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag)) {}

#if FIFODEBUG
	if (!scsiPhyFifoAltEmpty()) {
		// Force a lock-up.
		assertFail();
	}
#endif
}

static void
scsiWritePIO(const uint8_t* data, uint32_t count)
{
	for (int i = 0; i < count; ++i)
	{
		scsiPhyTx(data[i]);
	}
}

void
scsiWriteDMA(const uint8_t* data, uint32_t count)
{
	// Prepare DMA transfer
	dmaInProgress = 1;
	trace(trace_doTxSingleDMA);

	scsiTxDMAComplete = 0;
	scsiRxDMAComplete = 1;

	HAL_DMA_Start(
		&memToFSMC,
		(uint32_t) data,
		(uint32_t) SCSI_FIFO_DATA,
		count / 4);
}

int
scsiWriteDMAPoll()
{
	int complete = __HAL_DMA_GET_COUNTER(&memToFSMC) == 0;
	complete = complete && (HAL_DMA_PollForTransfer(&memToFSMC, HAL_DMA_FULL_TRANSFER, 0xffffffff) == HAL_OK);
	if (complete)
	{
		scsiTxDMAComplete = 1; // TODO MM FIX IRQ
		scsiRxDMAComplete = 1;

		dmaInProgress = 0;
		return 1;
	}
	else
	{
		return 0;
	}
}

void
scsiWrite(const uint8_t* data, uint32_t count)
{
	int i = 0;
	while (i < count && likely(!scsiDev.resetFlag))
	{
		uint32_t chunk = ((count - i) > SCSI_FIFO_DEPTH)
			? SCSI_FIFO_DEPTH : (count - i);

#if FIFODEBUG
		if (!scsiPhyFifoEmpty()) {
			// Force a lock-up.
			assertFail();
		}
#endif

		if (chunk < 16)
		{
			scsiWritePIO(data + i, chunk);
		}
		else
		{
			// DMA is doing 32bit transfers.
			chunk = chunk & 0xFFFFFFF8;
			scsiWriteDMA(data + i, chunk);

			// Wait for the next DMA interrupt (or the 1ms systick)
			// It's beneficial to halt the processor to
			// give the DMA controller more memory bandwidth to work with.
#if 0
			__WFI();
#endif
			trace(trace_spinReadDMAPoll);

			while (!scsiWriteDMAPoll() && likely(!scsiDev.resetFlag))
			{
#if 0
// TODO NEED SCSI DMA IRQs
			__WFI();
#endif
			};
		}

		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag)) {}

#if FIFODEBUG
		if (!scsiPhyFifoAltEmpty()) {
			// Force a lock-up.
			assertFail();
		}
#endif

		scsiPhyFifoFlip();
#if 0
// TODO NEED SCSI IRQs
			__WFI();
#endif

		i += chunk;
	}
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag)) {}

#if FIFODEBUG
	if (!scsiPhyFifoAltEmpty()) {
		// Force a lock-up.
		assertFail();
	}
#endif
}

static inline void busSettleDelay(void)
{
	// Data Release time (switching IO) = 400ns
	// + Bus Settle time (switching phase) = 400ns.
	s2s_delay_us(1); // Close enough.
}

void scsiEnterBusFree()
{
	*SCSI_CTRL_BSY = 0x00;
	// We now have a Bus Clear Delay of 800ns to release remaining signals.
	*SCSI_CTRL_PHASE = 0;
}

void scsiEnterPhase(int phase)
{
	// ANSI INCITS 362-2002 SPI-3 10.7.1:
	// Phase changes are not allowed while REQ or ACK is asserted.
	while (likely(!scsiDev.resetFlag) && scsiStatusACK()) {}

	int newPhase = phase > 0 ? phase : 0;
	int oldPhase = *SCSI_CTRL_PHASE;

	if (!scsiPhyFifoEmpty() || !scsiPhyFifoAltEmpty()) {
		// Force a lock-up.
		assertFail();
	}
	if (newPhase != oldPhase)
	{
		*SCSI_CTRL_PHASE = newPhase;
		busSettleDelay();

		if (scsiDev.compatMode < COMPAT_SCSI2)
		{
			s2s_delay_us(100);
		}

	}
}

void scsiPhyReset()
{
	trace(trace_scsiPhyReset);
	if (dmaInProgress)
	{
		trace(trace_spinDMAReset);
		HAL_DMA_Abort(&memToFSMC);
		HAL_DMA_Abort(&fsmcToMem);

		dmaInProgress = 0;
	}
#if 0

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
#endif

	*SCSI_CTRL_PHASE = 0x00;
	*SCSI_CTRL_BSY = 0x00;
	s2s_fpgaReset(); // Clears fifos etc.

	scsiPhyFifoSel = 0;
	*SCSI_FIFO_SEL = 0;

	// DMA Benchmark code
	// Currently 10MB/s. Assume 20MB/s is achievable with 16 bits.
	#ifdef DMA_BENCHMARK
	while(1)
	{
		s2s_ledOn();
		// 100MB
		for (int i = 0; i < (100LL * 1024 * 1024 / SCSI_FIFO_DEPTH); ++i)
		{
			HAL_DMA_Start(
				&memToFSMC,
				(uint32_t) &scsiDev.data[0],
				(uint32_t) SCSI_FIFO_DATA,
				SCSI_FIFO_DEPTH / 4);

			HAL_DMA_PollForTransfer(
				&memToFSMC,
				HAL_DMA_FULL_TRANSFER,
				0xffffffff);

			s2s_fpgaReset();
		}
		s2s_ledOff();

		for(int i = 0; i < 10; ++i) s2s_delay_ms(1000);
	}
	#endif

	// FPGA comms test code
	#ifdef FPGA_TEST

	while(1)
	{
		for (int j = 0; j < SCSI_FIFO_DEPTH; ++j)
		{
			scsiDev.data[j] = j;
		}

		*SCSI_CTRL_PHASE = DATA_IN;
		HAL_DMA_Start(
			&memToFSMC,
			(uint32_t) &scsiDev.data[0],
			(uint32_t) SCSI_FIFO_DATA,
			SCSI_FIFO_DEPTH / 4);

		HAL_DMA_PollForTransfer(
			&memToFSMC,
			HAL_DMA_FULL_TRANSFER,
			0xffffffff);

		memset(&scsiDev.data[0], 0, SCSI_FIFO_DEPTH);

		*SCSI_CTRL_PHASE = DATA_OUT;
		HAL_DMA_Start(
			&fsmcToMem,
			(uint32_t) SCSI_FIFO_DATA,
			(uint32_t) &scsiDev.data[0],
			SCSI_FIFO_DEPTH);

		HAL_DMA_PollForTransfer(
			&fsmcToMem,
			HAL_DMA_FULL_TRANSFER,
			0xffffffff);

		for (int j = 0; j < SCSI_FIFO_DEPTH; ++j)
		{
			if (scsiDev.data[j] != (uint8_t) j)
			{
				assertFail();
			}
		}

		s2s_fpgaReset();

	}
	#endif

}

static void scsiPhyInitDMA()
{
	// One-time init only.
	static uint8_t init = 0;
	if (init == 0)
	{
		init = 1;

		// Memory to memory transfers can only be done using DMA2
		__DMA2_CLK_ENABLE();

		// Transmit SCSI data. The source data is treated as the
		// peripheral (even though this is memory-to-memory)
		memToFSMC.Instance = DMA2_Stream0;
		memToFSMC.Init.Channel = DMA_CHANNEL_0;
		memToFSMC.Init.Direction = DMA_MEMORY_TO_MEMORY;
		memToFSMC.Init.PeriphInc = DMA_PINC_ENABLE;
		memToFSMC.Init.MemInc = DMA_MINC_DISABLE;
		memToFSMC.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
		memToFSMC.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		memToFSMC.Init.Mode = DMA_NORMAL;
		memToFSMC.Init.Priority = DMA_PRIORITY_LOW;
		// FIFO mode is needed to allow conversion from 32bit words to the
		// 8bit FSMC interface.
		memToFSMC.Init.FIFOMode = DMA_FIFOMODE_ENABLE;

		// We only use 1 word (4 bytes) in the fifo at a time. Normally it's
		// better to let the DMA fifo fill up then do burst transfers, but
		// bursting out the FSMC interface will be very slow and may starve
		// other (faster) transfers. We don't want to risk the SDIO transfers
		// from overrun/underrun conditions.
		memToFSMC.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL;
		memToFSMC.Init.MemBurst = DMA_MBURST_SINGLE;
		memToFSMC.Init.PeriphBurst = DMA_PBURST_SINGLE;
		HAL_DMA_Init(&memToFSMC);

		// Receive SCSI data. The source data (fsmc) is treated as the
		// peripheral (even though this is memory-to-memory)
		fsmcToMem.Instance = DMA2_Stream1;
		fsmcToMem.Init.Channel = DMA_CHANNEL_0;
		fsmcToMem.Init.Direction = DMA_MEMORY_TO_MEMORY;
		fsmcToMem.Init.PeriphInc = DMA_PINC_DISABLE;
		fsmcToMem.Init.MemInc = DMA_MINC_ENABLE;
		fsmcToMem.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		fsmcToMem.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
		fsmcToMem.Init.Mode = DMA_NORMAL;
		fsmcToMem.Init.Priority = DMA_PRIORITY_LOW;
		fsmcToMem.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
		fsmcToMem.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_1QUARTERFULL;
		fsmcToMem.Init.MemBurst = DMA_MBURST_SINGLE;
		fsmcToMem.Init.PeriphBurst = DMA_PBURST_SINGLE;
		HAL_DMA_Init(&fsmcToMem);

		// TODO configure IRQs
	}
}


void scsiPhyInit()
{
	scsiPhyInitDMA();

	*SCSI_CTRL_IDMASK = 0x00; // Reset in scsiPhyConfig
	*SCSI_CTRL_PHASE = 0x00;
	*SCSI_CTRL_BSY = 0x00;
	scsiPhyFifoSel = 0;
	*SCSI_FIFO_SEL = 0;

}

void scsiPhyConfig()
{
	if (scsiDev.boardCfg.flags6 & S2S_CFG_ENABLE_TERMINATOR)
	{
		HAL_GPIO_WritePin(nTERM_EN_GPIO_Port, nTERM_EN_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(nTERM_EN_GPIO_Port, nTERM_EN_Pin, GPIO_PIN_SET);
	}


	uint8_t idMask = 0;
	for (int i = 0; i < 8; ++i)
	{
		const S2S_TargetCfg* cfg = s2s_getConfigById(i);
		if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
		{
			idMask |= (1 << i);
		}
	}
	*SCSI_CTRL_IDMASK = idMask;
}


// 1 = DBx error
// 2 = Parity error
// 4 = MSG error
// 8 = CD error
// 16 = IO error
// 32 = other error
int scsiSelfTest()
{
	return 0;
#if 0
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
#endif
}

