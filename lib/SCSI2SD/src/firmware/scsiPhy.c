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

#ifdef STM32F2xx
#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal_dma.h"
#endif

#ifdef STM32F4xx
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_dma.h"
#endif

#include "gpio.h"

#include "scsi.h"
#include "scsiPhy.h"
#include "time.h"
#include "fpga.h"
#include "led.h"

#include <string.h>

#define SCSI_ASYNC_15 0
#define SCSI_ASYNC_33 1
#define SCSI_ASYNC_50 2
#define SCSI_ASYNC_SAFE 3
#define SCSI_ASYNC_TURBO 4

#ifdef STM32F2xx
#include "scsiPhyTiming108MHz.h"
#endif

#ifdef STM32F4xx
#include "scsiPhyTiming90MHz.h"
#endif

// Private DMA variables.
static int dmaInProgress = 0;

static DMA_HandleTypeDef memToFSMC;
static DMA_HandleTypeDef fsmcToMem;


volatile uint8_t scsiRxDMAComplete;
volatile uint8_t scsiTxDMAComplete;

// scsi IRQ handler is initialised by the STM32 HAL. Connected to
// PE4
// Note: naming is important to ensure this function is listed in the
// vector table.
void EXTI4_IRQHandler()
{
	// Make sure that interrupt flag is set
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != RESET) {

		// Clear interrupt flag
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);

		uint8_t statusFlags = *SCSI_STS_SCSI;

		scsiDev.resetFlag = scsiDev.resetFlag || (statusFlags & 0x04);

		// selFlag is required for Philips P2000C which releases it after 600ns
		// without waiting for BSY.
		// Also required for some early Mac Plus roms
		if (statusFlags & 0x08) // Check SEL flag
		{
			scsiDev.selFlag = *SCSI_STS_SELECTED;
		}
	}
}

void
scsiSetDataCount(uint32_t count)
{
	*SCSI_DATA_CNT_HI = (count >> 16) & 0xff;
	*SCSI_DATA_CNT_MID = (count >> 8) & 0xff;
	*SCSI_DATA_CNT_LO = count & 0xff;
	*SCSI_DATA_CNT_SET = 1;

#ifdef STM32F4xx
	__NOP();
	__NOP();
#endif
}

int scsiFifoReady(void)
{
	__NOP();
#ifdef STM32F4xx
	__NOP();
#endif
	HAL_GPIO_ReadPin(GPIOE, FPGA_GPIO3_Pin);
	__NOP();
#ifdef STM32F4xx
	__NOP();
	__NOP();
	__NOP();
#endif
	return HAL_GPIO_ReadPin(GPIOE, FPGA_GPIO3_Pin) != 0;
}

uint8_t
scsiReadByte(void)
{
	scsiSetDataCount(1);

	// Ready immediately. setDataCount resets fifos

	__disable_irq();
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
	{
		__WFI(); // Wait for interrupt
	}
	__enable_irq();

	uint8_t val = scsiPhyRx();
	// TODO scsiDev.parityError = scsiDev.parityError || SCSI_Parity_Error_Read();

	return val;
}


void
scsiReadPIO(uint8_t* data, uint32_t count, int* parityError)
{
	uint16_t* fifoData = (uint16_t*)data;
	uint32_t count16 = (count + 1) / 2;

	int i = 0;
	while ((i  < count16) && likely(!scsiDev.resetFlag))
	{
		// Wait until FIFO is full (or complete)
		while (!scsiFifoReady() && likely(!scsiDev.resetFlag))
		{
			// spin
		}

		if (count16 - i >= SCSI_FIFO_DEPTH16)
		{
			uint32_t chunk16 = SCSI_FIFO_DEPTH16;

			// Let gcc unroll the loop as much as possible.
			for (uint32_t k = 0; k + 128 <= chunk16; k += 128)
			{
				fifoData[i + k] = scsiPhyRx();
				fifoData[i + k + 1] = scsiPhyRx();
				fifoData[i + k + 2] = scsiPhyRx();
				fifoData[i + k + 3] = scsiPhyRx();
				fifoData[i + k + 4] = scsiPhyRx();
				fifoData[i + k + 5] = scsiPhyRx();
				fifoData[i + k + 6] = scsiPhyRx();
				fifoData[i + k + 7] = scsiPhyRx();
				fifoData[i + k + 8] = scsiPhyRx();
				fifoData[i + k + 9] = scsiPhyRx();
				fifoData[i + k + 10] = scsiPhyRx();
				fifoData[i + k + 11] = scsiPhyRx();
				fifoData[i + k + 12] = scsiPhyRx();
				fifoData[i + k + 13] = scsiPhyRx();
				fifoData[i + k + 14] = scsiPhyRx();
				fifoData[i + k + 15] = scsiPhyRx();
				fifoData[i + k + 16] = scsiPhyRx();
				fifoData[i + k + 17] = scsiPhyRx();
				fifoData[i + k + 18] = scsiPhyRx();
				fifoData[i + k + 19] = scsiPhyRx();
				fifoData[i + k + 20] = scsiPhyRx();
				fifoData[i + k + 21] = scsiPhyRx();
				fifoData[i + k + 22] = scsiPhyRx();
				fifoData[i + k + 23] = scsiPhyRx();
				fifoData[i + k + 24] = scsiPhyRx();
				fifoData[i + k + 25] = scsiPhyRx();
				fifoData[i + k + 26] = scsiPhyRx();
				fifoData[i + k + 27] = scsiPhyRx();
				fifoData[i + k + 28] = scsiPhyRx();
				fifoData[i + k + 29] = scsiPhyRx();
				fifoData[i + k + 30] = scsiPhyRx();
				fifoData[i + k + 31] = scsiPhyRx();
				fifoData[i + k + 32] = scsiPhyRx();
				fifoData[i + k + 33] = scsiPhyRx();
				fifoData[i + k + 34] = scsiPhyRx();
				fifoData[i + k + 35] = scsiPhyRx();
				fifoData[i + k + 36] = scsiPhyRx();
				fifoData[i + k + 37] = scsiPhyRx();
				fifoData[i + k + 38] = scsiPhyRx();
				fifoData[i + k + 39] = scsiPhyRx();
				fifoData[i + k + 40] = scsiPhyRx();
				fifoData[i + k + 41] = scsiPhyRx();
				fifoData[i + k + 42] = scsiPhyRx();
				fifoData[i + k + 43] = scsiPhyRx();
				fifoData[i + k + 44] = scsiPhyRx();
				fifoData[i + k + 45] = scsiPhyRx();
				fifoData[i + k + 46] = scsiPhyRx();
				fifoData[i + k + 47] = scsiPhyRx();
				fifoData[i + k + 48] = scsiPhyRx();
				fifoData[i + k + 49] = scsiPhyRx();
				fifoData[i + k + 50] = scsiPhyRx();
				fifoData[i + k + 51] = scsiPhyRx();
				fifoData[i + k + 52] = scsiPhyRx();
				fifoData[i + k + 53] = scsiPhyRx();
				fifoData[i + k + 54] = scsiPhyRx();
				fifoData[i + k + 55] = scsiPhyRx();
				fifoData[i + k + 56] = scsiPhyRx();
				fifoData[i + k + 57] = scsiPhyRx();
				fifoData[i + k + 58] = scsiPhyRx();
				fifoData[i + k + 59] = scsiPhyRx();
				fifoData[i + k + 60] = scsiPhyRx();
				fifoData[i + k + 61] = scsiPhyRx();
				fifoData[i + k + 62] = scsiPhyRx();
				fifoData[i + k + 63] = scsiPhyRx();
				fifoData[i + k + 64] = scsiPhyRx();
				fifoData[i + k + 65] = scsiPhyRx();
				fifoData[i + k + 66] = scsiPhyRx();
				fifoData[i + k + 67] = scsiPhyRx();
				fifoData[i + k + 68] = scsiPhyRx();
				fifoData[i + k + 69] = scsiPhyRx();
				fifoData[i + k + 70] = scsiPhyRx();
				fifoData[i + k + 71] = scsiPhyRx();
				fifoData[i + k + 72] = scsiPhyRx();
				fifoData[i + k + 73] = scsiPhyRx();
				fifoData[i + k + 74] = scsiPhyRx();
				fifoData[i + k + 75] = scsiPhyRx();
				fifoData[i + k + 76] = scsiPhyRx();
				fifoData[i + k + 77] = scsiPhyRx();
				fifoData[i + k + 78] = scsiPhyRx();
				fifoData[i + k + 79] = scsiPhyRx();
				fifoData[i + k + 80] = scsiPhyRx();
				fifoData[i + k + 81] = scsiPhyRx();
				fifoData[i + k + 82] = scsiPhyRx();
				fifoData[i + k + 83] = scsiPhyRx();
				fifoData[i + k + 84] = scsiPhyRx();
				fifoData[i + k + 85] = scsiPhyRx();
				fifoData[i + k + 86] = scsiPhyRx();
				fifoData[i + k + 87] = scsiPhyRx();
				fifoData[i + k + 88] = scsiPhyRx();
				fifoData[i + k + 89] = scsiPhyRx();
				fifoData[i + k + 90] = scsiPhyRx();
				fifoData[i + k + 91] = scsiPhyRx();
				fifoData[i + k + 92] = scsiPhyRx();
				fifoData[i + k + 93] = scsiPhyRx();
				fifoData[i + k + 94] = scsiPhyRx();
				fifoData[i + k + 95] = scsiPhyRx();
				fifoData[i + k + 96] = scsiPhyRx();
				fifoData[i + k + 97] = scsiPhyRx();
				fifoData[i + k + 98] = scsiPhyRx();
				fifoData[i + k + 99] = scsiPhyRx();
				fifoData[i + k + 100] = scsiPhyRx();
				fifoData[i + k + 101] = scsiPhyRx();
				fifoData[i + k + 102] = scsiPhyRx();
				fifoData[i + k + 103] = scsiPhyRx();
				fifoData[i + k + 104] = scsiPhyRx();
				fifoData[i + k + 105] = scsiPhyRx();
				fifoData[i + k + 106] = scsiPhyRx();
				fifoData[i + k + 107] = scsiPhyRx();
				fifoData[i + k + 108] = scsiPhyRx();
				fifoData[i + k + 109] = scsiPhyRx();
				fifoData[i + k + 110] = scsiPhyRx();
				fifoData[i + k + 111] = scsiPhyRx();
				fifoData[i + k + 112] = scsiPhyRx();
				fifoData[i + k + 113] = scsiPhyRx();
				fifoData[i + k + 114] = scsiPhyRx();
				fifoData[i + k + 115] = scsiPhyRx();
				fifoData[i + k + 116] = scsiPhyRx();
				fifoData[i + k + 117] = scsiPhyRx();
				fifoData[i + k + 118] = scsiPhyRx();
				fifoData[i + k + 119] = scsiPhyRx();
				fifoData[i + k + 120] = scsiPhyRx();
				fifoData[i + k + 121] = scsiPhyRx();
				fifoData[i + k + 122] = scsiPhyRx();
				fifoData[i + k + 123] = scsiPhyRx();
				fifoData[i + k + 124] = scsiPhyRx();
				fifoData[i + k + 125] = scsiPhyRx();
				fifoData[i + k + 126] = scsiPhyRx();
				fifoData[i + k + 127] = scsiPhyRx();
			}

			i += chunk16;
		}
		else
		{
			uint32_t chunk16 = count16 - i;

			uint32_t k = 0;
			for (; k + 4 <= chunk16; k += 4)
			{
				fifoData[i + k] = scsiPhyRx();
				fifoData[i + 1 + k] = scsiPhyRx();
				fifoData[i + 2 + k] = scsiPhyRx();
				fifoData[i + 3 + k] = scsiPhyRx();
			}
			for (; k < chunk16; ++k)
			{
				fifoData[i + k] = scsiPhyRx();
			}
			i += chunk16;
		}
	}

	*parityError |= scsiParityError();
}

void
scsiRead(uint8_t* data, uint32_t count, int* parityError)
{
	int i = 0;
	*parityError = 0;

	while (i < count && likely(!scsiDev.resetFlag))
	{
		uint32_t chunk = ((count - i) > SCSI_XFER_MAX)
			? SCSI_XFER_MAX : (count - i);
		scsiSetDataCount(chunk);

		scsiReadPIO(data + i, chunk, parityError);

		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
		{
		    __disable_irq();
            if (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
            {
    			__WFI();
            }
		    __enable_irq();
		}

		i += chunk;
	}
}

void
scsiWriteByte(uint8_t value)
{
	scsiSetDataCount(1);
	scsiPhyTx(value);

	__disable_irq();
	while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
	{
		__WFI();
	}
	__enable_irq();
}

void
scsiWritePIO(const uint8_t* data, uint32_t count)
{
	uint16_t* fifoData = (uint16_t*)data;
	uint32_t count16 = (count + 1) / 2;

	int i = 0;
	while ((i  < count16) && likely(!scsiDev.resetFlag))
	{
		while (!scsiFifoReady() && likely(!scsiDev.resetFlag))
		{
			// Spin
		}

		if (count16 - i >= SCSI_FIFO_DEPTH16)
		{
			uint32_t chunk16 = SCSI_FIFO_DEPTH16;

			// Let gcc unroll the loop as much as possible.
			for (uint32_t k = 0; k + 128 <= chunk16; k += 128)
			{
				scsiPhyTx32(fifoData[i + k], fifoData[i + k + 1]);
				scsiPhyTx32(fifoData[i + 2 + k], fifoData[i + k + 3]);
				scsiPhyTx32(fifoData[i + 4 + k], fifoData[i + k + 5]);
				scsiPhyTx32(fifoData[i + 6 + k], fifoData[i + k + 7]);
				scsiPhyTx32(fifoData[i + 8 + k], fifoData[i + k + 9]);
				scsiPhyTx32(fifoData[i + 10 + k], fifoData[i + k + 11]);
				scsiPhyTx32(fifoData[i + 12 + k], fifoData[i + k + 13]);
				scsiPhyTx32(fifoData[i + 14 + k], fifoData[i + k + 15]);
				scsiPhyTx32(fifoData[i + 16 + k], fifoData[i + k + 17]);
				scsiPhyTx32(fifoData[i + 18 + k], fifoData[i + k + 19]);
				scsiPhyTx32(fifoData[i + 20 + k], fifoData[i + k + 21]);
				scsiPhyTx32(fifoData[i + 22 + k], fifoData[i + k + 23]);
				scsiPhyTx32(fifoData[i + 24 + k], fifoData[i + k + 25]);
				scsiPhyTx32(fifoData[i + 26 + k], fifoData[i + k + 27]);
				scsiPhyTx32(fifoData[i + 28 + k], fifoData[i + k + 29]);
				scsiPhyTx32(fifoData[i + 30 + k], fifoData[i + k + 31]);

				scsiPhyTx32(fifoData[i + 32 + k], fifoData[i + k + 33]);
				scsiPhyTx32(fifoData[i + 34 + k], fifoData[i + k + 35]);
				scsiPhyTx32(fifoData[i + 36 + k], fifoData[i + k + 37]);
				scsiPhyTx32(fifoData[i + 38 + k], fifoData[i + k + 39]);
				scsiPhyTx32(fifoData[i + 40 + k], fifoData[i + k + 41]);
				scsiPhyTx32(fifoData[i + 42 + k], fifoData[i + k + 43]);
				scsiPhyTx32(fifoData[i + 44 + k], fifoData[i + k + 45]);
				scsiPhyTx32(fifoData[i + 46 + k], fifoData[i + k + 47]);
				scsiPhyTx32(fifoData[i + 48 + k], fifoData[i + k + 49]);
				scsiPhyTx32(fifoData[i + 50 + k], fifoData[i + k + 51]);
				scsiPhyTx32(fifoData[i + 52 + k], fifoData[i + k + 53]);
				scsiPhyTx32(fifoData[i + 54 + k], fifoData[i + k + 55]);
				scsiPhyTx32(fifoData[i + 56 + k], fifoData[i + k + 57]);
				scsiPhyTx32(fifoData[i + 58 + k], fifoData[i + k + 59]);
				scsiPhyTx32(fifoData[i + 60 + k], fifoData[i + k + 61]);
				scsiPhyTx32(fifoData[i + 62 + k], fifoData[i + k + 63]);

				scsiPhyTx32(fifoData[i + 64 + k], fifoData[i + k + 65]);
				scsiPhyTx32(fifoData[i + 66 + k], fifoData[i + k + 67]);
				scsiPhyTx32(fifoData[i + 68 + k], fifoData[i + k + 69]);
				scsiPhyTx32(fifoData[i + 70 + k], fifoData[i + k + 71]);
				scsiPhyTx32(fifoData[i + 72 + k], fifoData[i + k + 73]);
				scsiPhyTx32(fifoData[i + 74 + k], fifoData[i + k + 75]);
				scsiPhyTx32(fifoData[i + 76 + k], fifoData[i + k + 77]);
				scsiPhyTx32(fifoData[i + 78 + k], fifoData[i + k + 79]);
				scsiPhyTx32(fifoData[i + 80 + k], fifoData[i + k + 81]);
				scsiPhyTx32(fifoData[i + 82 + k], fifoData[i + k + 83]);
				scsiPhyTx32(fifoData[i + 84 + k], fifoData[i + k + 85]);
				scsiPhyTx32(fifoData[i + 86 + k], fifoData[i + k + 87]);
				scsiPhyTx32(fifoData[i + 88 + k], fifoData[i + k + 89]);
				scsiPhyTx32(fifoData[i + 90 + k], fifoData[i + k + 91]);
				scsiPhyTx32(fifoData[i + 92 + k], fifoData[i + k + 93]);
				scsiPhyTx32(fifoData[i + 94 + k], fifoData[i + k + 95]);

				scsiPhyTx32(fifoData[i + 96 + k], fifoData[i + k + 97]);
				scsiPhyTx32(fifoData[i + 98 + k], fifoData[i + k + 99]);
				scsiPhyTx32(fifoData[i + 100 + k], fifoData[i + k + 101]);
				scsiPhyTx32(fifoData[i + 102 + k], fifoData[i + k + 103]);
				scsiPhyTx32(fifoData[i + 104 + k], fifoData[i + k + 105]);
				scsiPhyTx32(fifoData[i + 106 + k], fifoData[i + k + 107]);
				scsiPhyTx32(fifoData[i + 108 + k], fifoData[i + k + 109]);
				scsiPhyTx32(fifoData[i + 110 + k], fifoData[i + k + 111]);
				scsiPhyTx32(fifoData[i + 112 + k], fifoData[i + k + 113]);
				scsiPhyTx32(fifoData[i + 114 + k], fifoData[i + k + 115]);
				scsiPhyTx32(fifoData[i + 116 + k], fifoData[i + k + 117]);
				scsiPhyTx32(fifoData[i + 118 + k], fifoData[i + k + 119]);
				scsiPhyTx32(fifoData[i + 120 + k], fifoData[i + k + 121]);
				scsiPhyTx32(fifoData[i + 122 + k], fifoData[i + k + 123]);
				scsiPhyTx32(fifoData[i + 124 + k], fifoData[i + k + 125]);
				scsiPhyTx32(fifoData[i + 126 + k], fifoData[i + k + 127]);

			}

			i += chunk16;
		}
		else
		{
			uint32_t chunk16 = count16 - i;

			uint32_t k = 0;
			for (; k + 4 <= chunk16; k += 4)
			{
				scsiPhyTx32(fifoData[i + k], fifoData[i + k + 1]);
				scsiPhyTx32(fifoData[i + k + 2], fifoData[i + k + 3]);
			}
			for (; k < chunk16; ++k)
			{
				scsiPhyTx(fifoData[i + k]);
			}
			i += chunk16;
		}
	}
}


void
scsiWrite(const uint8_t* data, uint32_t count)
{
	int i = 0;
	while (i < count && likely(!scsiDev.resetFlag))
	{
		uint32_t chunk = ((count - i) > SCSI_XFER_MAX)
			? SCSI_XFER_MAX : (count - i);
		scsiSetDataCount(chunk);

		scsiWritePIO(data + i, chunk);

		while (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
		{
		    __disable_irq();
		    if (!scsiPhyComplete() && likely(!scsiDev.resetFlag))
            {
    			__WFI();
            }
		    __enable_irq();
		}

		i += chunk;
	}
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
	const uint8_t* asyncTiming = asyncTimings[0];
	scsiSetTiming(
		asyncTiming[0],
		asyncTiming[1],
		asyncTiming[2],
		asyncTiming[3]);
}

void scsiEnterPhase(int newPhase)
{
	uint32_t delay = scsiEnterPhaseImmediate(newPhase);
	if (delay > 0)
	{
		s2s_delay_us(delay);
	}
}

// Returns microsecond delay
uint32_t scsiEnterPhaseImmediate(int newPhase)
{
	// ANSI INCITS 362-2002 SPI-3 10.7.1:
	// Phase changes are not allowed while REQ or ACK is asserted.
	while (likely(!scsiDev.resetFlag) && scsiStatusACK()) {}

	int oldPhase = *SCSI_CTRL_PHASE;

	if (newPhase != oldPhase)
	{
		if ((newPhase == DATA_IN || newPhase == DATA_OUT) &&
			scsiDev.target->syncOffset)
		{
			if (scsiDev.target->syncPeriod < 23)
			{
				scsiSetTiming(SCSI_FAST20_ASSERT, SCSI_FAST20_DESKEW, SCSI_FAST20_HOLD, 1);
			}
			else if (scsiDev.target->syncPeriod <= 25)
			{
				if (newPhase == DATA_IN)
				{
					scsiSetTiming(SCSI_FAST10_WRITE_ASSERT, SCSI_FAST10_DESKEW, SCSI_FAST10_HOLD, 1);
				}
				else
				{
					scsiSetTiming(SCSI_FAST10_READ_ASSERT, SCSI_FAST10_DESKEW, SCSI_FAST10_HOLD, 1);
				}
			}
			else
			{
				// Amiga A3000 OS3.9 sets period to 35 and fails with
				// glitch == 1.
				int glitch =
					scsiDev.target->syncPeriod < 35 ? 1 :
						(scsiDev.target->syncPeriod < 45 ? 2 : 5);
				int deskew = syncDeskew(scsiDev.target->syncPeriod);
				int assertion;
				if (newPhase == DATA_IN)
				{
					assertion = syncAssertionWrite(scsiDev.target->syncPeriod, deskew);
				}
				else
				{
					assertion = syncAssertionRead(scsiDev.target->syncPeriod);
				}
				scsiSetTiming(
					assertion,
					deskew,
					syncHold(scsiDev.target->syncPeriod),
					glitch);
			}

			*SCSI_CTRL_SYNC_OFFSET = scsiDev.target->syncOffset;
		}
		else if (newPhase >= 0)
		{

			*SCSI_CTRL_SYNC_OFFSET = 0;
			const uint8_t* asyncTiming;

			if (scsiDev.boardCfg.scsiSpeed == S2S_CFG_SPEED_NoLimit)
			{
				asyncTiming = asyncTimings[SCSI_ASYNC_SAFE];
			}
			else if (scsiDev.boardCfg.scsiSpeed >= S2S_CFG_SPEED_TURBO)
			{
				asyncTiming = asyncTimings[SCSI_ASYNC_TURBO];
			}
			else if (scsiDev.boardCfg.scsiSpeed >= S2S_CFG_SPEED_ASYNC_50)
			{
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

		uint32_t delayUs = 0;
		if (newPhase >= 0)
		{
			*SCSI_CTRL_PHASE = newPhase;
			delayUs += 1; // busSettleDelay

			if (scsiDev.compatMode < COMPAT_SCSI2)
			{
				// EMU EMAX needs 100uS ! 10uS is not enough.
				delayUs += 100;
			}
		}
		else
		{
			*SCSI_CTRL_PHASE = 0;
		}

		return delayUs;
	}

	return 0; // No change
}

// Returns a "safe" estimate of the host SCSI speed of
// theoretical speed / 2
uint32_t s2s_getScsiRateKBs()
{
	if (scsiDev.target->syncOffset)
	{
		if (scsiDev.target->syncPeriod < 23)
		{
			return 20 / 2;
		}
		else if (scsiDev.target->syncPeriod <= 25)
		{
			return 10 / 2;
		}
		else
		{
			// 1000000000 / (scsiDev.target->syncPeriod * 4) bytes per second
			// (1000000000 / (scsiDev.target->syncPeriod * 4)) / 1000  kB/s
			return (1000000 / (scsiDev.target->syncPeriod * 4)) / 2;
		}
	}
	else
	{
		return 0;
	}
}

void scsiPhyReset()
{
	if (dmaInProgress)
	{
		HAL_DMA_Abort(&memToFSMC);
		HAL_DMA_Abort(&fsmcToMem);

		dmaInProgress = 0;
	}

	s2s_fpgaReset(); // Clears fifos etc.

	*SCSI_CTRL_PHASE = 0x00;
	*SCSI_CTRL_BSY = 0x00;
	*SCSI_CTRL_DBX = 0;

	*SCSI_CTRL_SYNC_OFFSET = 0;
	scsiSetDefaultTiming();

	// DMA Benchmark code
	// Currently 14.9MB/s.
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

	// PIO Benchmark code
	// Currently 16.7MB/s.
	//#define PIO_BENCHMARK 1
	#ifdef PIO_BENCHMARK
	while(1)
	{
		s2s_ledOn();

		scsiEnterPhase(DATA_IN); // Need IO flag set for fifo ready flag

		// 100MB
		for (int i = 0; i < (100LL * 1024 * 1024 / SCSI_FIFO_DEPTH); ++i)
		{
			scsiSetDataCount(1); // Resets fifos.

			// Shouldn't block
			scsiDev.resetFlag = 0;
			scsiWritePIO(&scsiDev.data[0], SCSI_FIFO_DEPTH);
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
	s2s_delay_ms(1);
	if (! scsiStatusBSY())
	{
		*SCSI_CTRL_BSY = 0;

		// Error, BSY doesn't work.
		return 32;
	}

	// Should be safe to use the bus now.

	int result = 0;

	*SCSI_CTRL_DBX = 0;
	busSettleDelay();
	if ((*SCSI_STS_DBX & 0xff) != 0)
	{
		result = 1;
	}

	*SCSI_CTRL_BSY = 0;

	return result;
}

