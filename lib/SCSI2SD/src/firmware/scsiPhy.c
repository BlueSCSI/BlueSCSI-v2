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

static uint8_t asyncTimings[][4] =
{
/* Speed,    Assert,    Deskew,    Hold,    Glitch */
{/*1.5MB/s*/ 28,        18,        13,      13},
{/*3.3MB/s*/ 13,        6,         6,       13},
{/*5MB/s*/   9,         6,         6,       6} // 80ns
};

#define SCSI_ASYNC_15 0
#define SCSI_ASYNC_33 1
#define SCSI_ASYNC_50 2

// 5MB/s synchronous timing
#define SCSI_FAST5_DESKEW 6 // 55ns
#define SCSI_FAST5_HOLD 6 // 53ns

// 10MB/s synchronous timing
// 2:0 Deskew count, 25ns
// 6:4 Hold count, 33ns
// 3:0 Assertion count, 30ns
// We want deskew + hold + assert + 3 to add up to 11 clocks
// the fpga code has 1 clock of overhead when transitioning from deskew to
// assert to hold

#define SCSI_FAST10_DESKEW 2 // 25ns
#define SCSI_FAST10_HOLD 3 // 33ns
#define SCSI_FAST10_ASSERT 3 // 30ns

#define syncDeskew(period) ((period) < 45 ? \
	SCSI_FAST10_DESKEW : SCSI_FAST5_DESKEW)

#define syncHold(period) ((period) < 45 ? \
	((period) == 25 ? SCSI_FAST10_HOLD : 4) /* 25ns/33ns */\
	: SCSI_FAST5_HOLD)


// 3.125MB/s (80 period) to < 10MB/s sync
// Assumes a 108MHz fpga clock. (9 ns)
// (((period * 4) / 2) * 0.8) / 9
// Done using 3 fixed point math.
// 3:0 Assertion count, variable
#define syncAssertion(period) ((((((int)period) * 177) + 750)/1000) & 0xF)

// Time until we consider ourselves selected
// 400ns at 108MHz
#define SCSI_DEFAULT_SELECTION 43
#define SCSI_FAST_SELECTION 5

// Private DMA variables.
static int dmaInProgress = 0;

static DMA_HandleTypeDef memToFSMC;
static DMA_HandleTypeDef fsmcToMem;


volatile uint8_t scsiRxDMAComplete;
volatile uint8_t scsiTxDMAComplete;

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

		// selFlag is required for Philips P2000C which releases it after 600ns
		// without waiting for BSY.
		// Also required for some early Mac Plus roms
		scsiDev.selFlag = *SCSI_STS_SELECTED;
	}

	__SEV(); // Set event. See corresponding __WFE() calls.
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

void
scsiSetDataCount(uint32_t count)
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
	scsiSetDataCount(1);

	trace(trace_spinPhyRxFifo);
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
	{
		__WFE(); // Wait for event
	}
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
	uint16_t* fifoData = (uint16_t*)data;

	for (int i = 0; i < (count + 1) / 2; ++i)
	{
		fifoData[i] = scsiPhyRx(); // TODO ASSUMES LITTLE ENDIAN
	}
}

void
scsiReadDMA(uint8_t* data, uint32_t count)
{
	// Prepare DMA transfer
	dmaInProgress = 1;
	trace(trace_doRxSingleDMA);

	scsiTxDMAComplete = 1; // TODO not used much
	scsiRxDMAComplete = 0; // TODO not used much

	HAL_DMA_Start(
		&fsmcToMem,
		(uint32_t) SCSI_FIFO_DATA,
		(uint32_t) data,
		(count + 1) / 2);
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
scsiRead(uint8_t* data, uint32_t count, int* parityError)
{
	int i = 0;
	*parityError = 0;


	uint32_t chunk = ((count - i) > SCSI_FIFO_DEPTH)
		? SCSI_FIFO_DEPTH : (count - i);
#ifdef SCSI_FSMC_DMA
	if (chunk >= 16)
	{
		// DMA is doing 32bit transfers.
		chunk = chunk & 0xFFFFFFF8;
	}
#endif
	scsiSetDataCount(chunk);

	while (i < count && likely(!scsiDev.resetFlag))
	{
		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
		{
			__WFE(); // Wait for event
		}
		*parityError |= scsiParityError();
		scsiPhyFifoFlip();

		uint32_t nextChunk = ((count - i - chunk) > SCSI_FIFO_DEPTH)
			? SCSI_FIFO_DEPTH : (count - i - chunk);
#ifdef SCSI_FSMC_DMA
		if (nextChunk >= 16)
		{
			nextChunk = nextChunk & 0xFFFFFFF8;
		}
#endif
		if (nextChunk > 0)
		{
			scsiSetDataCount(nextChunk);
		}

#ifdef SCSI_FSMC_DMA
		if (chunk < 16)
#endif
		{
			scsiReadPIO(data + i, chunk);
		}
#ifdef SCSI_FSMC_DMA
		else
		{
			scsiReadDMA(data + i, chunk);

			trace(trace_spinReadDMAPoll);

			while (!scsiReadDMAPoll() && likely(!scsiDev.resetFlag))
			{
			};
		}
#endif


		i += chunk;
		chunk = nextChunk;
	}
#if FIFODEBUG
		if (!scsiPhyFifoEmpty() || !scsiPhyFifoAltEmpty()) {
			int j = 0;
			while (!scsiPhyFifoEmpty()) { scsiPhyRx(); ++j; }
			scsiPhyFifoFlip();
			int k = 0;
			while (!scsiPhyFifoEmpty()) { scsiPhyRx(); ++k; }
			// Force a lock-up.
			assertFail();
		}
#endif
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

	scsiSetDataCount(1);

	trace(trace_spinTxComplete);
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
	{
		__WFE(); // Wait for event
	}

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
	uint16_t* fifoData = (uint16_t*)data;
	for (int i = 0; i < (count + 1) / 2; ++i)
	{
		scsiPhyTx(fifoData[i]);
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

#ifdef SCSI_FSMC_DMA
		if (chunk < 16)
#endif
		{
			scsiWritePIO(data + i, chunk);
		}
#ifdef SCSI_FSMC_DMA
		else
		{
			// DMA is doing 32bit transfers.
			chunk = chunk & 0xFFFFFFF8;
			scsiWriteDMA(data + i, chunk);

			trace(trace_spinReadDMAPoll);

			while (!scsiWriteDMAPoll() && likely(!scsiDev.resetFlag))
			{
			}
		}
#endif

		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
		{
			__WFE(); // Wait for event
		}

#if FIFODEBUG
		if (!scsiPhyFifoAltEmpty()) {
			// Force a lock-up.
			assertFail();
		}
#endif

		scsiPhyFifoFlip();
		scsiSetDataCount(chunk);
		i += chunk;
	}
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
	{
		__WFE(); // Wait for event
	}

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

static void
scsiSetTiming(
	uint8_t assertClocks,
	uint8_t deskew,
	uint8_t hold,
	uint8_t glitch)
{
	*SCSI_CTRL_DESKEW = ((hold & 7) << 5) | (deskew & 0x1F);
	*SCSI_CTRL_TIMING = (assertClocks & 0x3F);
	*SCSI_CTRL_TIMING3 = (glitch & 0xF);
}

static void
scsiSetDefaultTiming()
{
	const uint8_t* asyncTiming = asyncTimings[3];
	scsiSetTiming(
		asyncTiming[0],
		asyncTiming[1],
		asyncTiming[2],
		asyncTiming[3]);
}

void scsiEnterPhase(int phase)
{
	// ANSI INCITS 362-2002 SPI-3 10.7.1:
	// Phase changes are not allowed while REQ or ACK is asserted.
	while (likely(!scsiDev.resetFlag) && scsiStatusACK()) {}

	int newPhase = phase > 0 ? phase : 0;
	int oldPhase = *SCSI_CTRL_PHASE;

	if (!scsiDev.resetFlag && (!scsiPhyFifoEmpty() || !scsiPhyFifoAltEmpty())) {
		// Force a lock-up.
		assertFail();
	}
	if (newPhase != oldPhase)
	{
		if ((newPhase == DATA_IN || newPhase == DATA_OUT) &&
			scsiDev.target->syncOffset)
		{
			
			if (scsiDev.target->syncPeriod <= 25)
			{
				scsiSetTiming(SCSI_FAST10_ASSERT, SCSI_FAST10_DESKEW, SCSI_FAST10_HOLD, 1);
			}
			else
			{
				scsiSetTiming(
					syncAssertion(scsiDev.target->syncPeriod),
					syncDeskew(scsiDev.target->syncPeriod),
					syncHold(scsiDev.target->syncPeriod),
					scsiDev.target->syncPeriod < 45 ? 1 : 5);
			}

			// See note 26 in SCSI 2 standard: SCSI 1 implementations may assume
			// "leading edge of the first REQ pulse beyond the REQ/ACK offset
			// agreement would not occur until after the trailing edge of the
			// last ACK pulse within the agreement."
			// We simply subtract 1 from the offset to meet this requirement.
			if (scsiDev.target->syncOffset >= 2)
			{
				*SCSI_CTRL_SYNC_OFFSET = scsiDev.target->syncOffset - 1;
			} else {
				*SCSI_CTRL_SYNC_OFFSET = scsiDev.target->syncOffset;
			}
		}
		else
		{

			*SCSI_CTRL_SYNC_OFFSET = 0;
			const uint8_t* asyncTiming;

			if (scsiDev.boardCfg.scsiSpeed == S2S_CFG_SPEED_NoLimit ||
				scsiDev.boardCfg.scsiSpeed >= S2S_CFG_SPEED_ASYNC_50) {

				asyncTiming = asyncTimings[SCSI_ASYNC_50];
			} else if (scsiDev.boardCfg.scsiSpeed >= S2S_CFG_SPEED_ASYNC_33) {

				asyncTiming = asyncTimings[SCSI_ASYNC_33];
	
			} else {
				asyncTiming = asyncTimings[SCSI_ASYNC_15];
			}
			scsiSetTiming(
				asyncTiming[0],
				asyncTiming[1],
				asyncTiming[2],
				asyncTiming[3]);
		}

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

	*SCSI_CTRL_PHASE = 0x00;
	*SCSI_CTRL_BSY = 0x00;
	s2s_fpgaReset(); // Clears fifos etc.

	scsiPhyFifoSel = 0;
	*SCSI_FIFO_SEL = 0;
	*SCSI_CTRL_DBX = 0;

	*SCSI_CTRL_SYNC_OFFSET = 0;
	scsiSetDefaultTiming();

	// DMA Benchmark code
	// Currently 11MB/s.
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

	#ifdef SCSI_FREQ_TEST
	while(1)
	{
		*SCSI_CTRL_DBX = 0xAA;
		*SCSI_CTRL_DBX = 0x55;
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
		memToFSMC.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
		memToFSMC.Init.Mode = DMA_NORMAL;
		memToFSMC.Init.Priority = DMA_PRIORITY_LOW;
		// FIFO mode is needed to allow conversion from 32bit words to the
		// 16bit FSMC interface.
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
		fsmcToMem.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
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
	*SCSI_CTRL_DBX = 0;

	*SCSI_CTRL_SYNC_OFFSET = 0;
	scsiSetDefaultTiming();

	*SCSI_CTRL_SEL_TIMING = SCSI_DEFAULT_SELECTION;

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

	*SCSI_CTRL_FLAGS =
		((scsiDev.boardCfg.flags & S2S_CFG_DISABLE_GLITCH) ?
			SCSI_CTRL_FLAGS_DISABLE_GLITCH : 0) |
		((scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY) ?
			SCSI_CTRL_FLAGS_ENABLE_PARITY : 0);

	*SCSI_CTRL_SEL_TIMING =
		(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_SEL_LATCH) ?
			SCSI_FAST_SELECTION : SCSI_DEFAULT_SELECTION;
}


// 1 = DBx error
// 2 = Parity error
// 4 = MSG error
// 8 = CD error
// 16 = IO error
// 32 = other error
// 64 = fpga comms error
int scsiSelfTest()
{
	if (scsiDev.phase != BUS_FREE)
	{
		return 32;
	}

	// Acquire the SCSI bus.
	for (int i = 0; i < 100; ++i)
	{
		if (scsiStatusBSY())
		{
			s2s_delay_ms(1);
		}
	}
	if (scsiStatusBSY())
	{
		// Error, couldn't acquire scsi bus
		return 32;
	}
	*SCSI_CTRL_BSY = 1;
	if (! scsiStatusBSY())
	{
		// Error, BSY doesn't work.
		return 32;
	}

	// Should be safe to use the bus now.

	int result = 0;

	// TEST DBx
	// TODO test DBp
	int i;
	for (i = 0; i < 256; ++i)
	{
		*SCSI_CTRL_DBX = i;
		busSettleDelay();
		// STS_DBX is 16 bit!
		if ((*SCSI_STS_DBX & 0xff) != (i & 0xff))
		{
			result |= 1;
		}
		/*if (Lookup_OddParity[i & 0xff] != SCSI_ReadPin(SCSI_In_DBP))
		{
			result |= 2;
		}*/
	}
	*SCSI_CTRL_DBX = 0;

	// TEST MSG, CD, IO
	/* TODO
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
	*/


	// FPGA comms test code
	for(i = 0; i < 10000; ++i)
	{
		for (int j = 0; j < SCSI_FIFO_DEPTH; ++j)
		{
			scsiDev.data[j] = j;
		}

		if (!scsiPhyFifoEmpty())
		{
			assertFail();
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

		if (!scsiPhyFifoFull())
		{
			assertFail();
		}

		memset(&scsiDev.data[0], 0, SCSI_FIFO_DEPTH);

		*SCSI_CTRL_PHASE = DATA_OUT;
		HAL_DMA_Start(
			&fsmcToMem,
			(uint32_t) SCSI_FIFO_DATA,
			(uint32_t) &scsiDev.data[0],
			SCSI_FIFO_DEPTH / 2);

		HAL_DMA_PollForTransfer(
			&fsmcToMem,
			HAL_DMA_FULL_TRANSFER,
			0xffffffff);

		if (!scsiPhyFifoEmpty())
		{
			assertFail();
		}


		for (int j = 0; j < SCSI_FIFO_DEPTH; ++j)
		{
			if (scsiDev.data[j] != (uint8_t) j)
			{
				result |= 64;
			}
		}

		s2s_fpgaReset();

	}

	*SCSI_CTRL_BSY = 0;
	return result;
}

