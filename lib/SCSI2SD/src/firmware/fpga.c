//	Copyright (C) 2015 Michael McMaster <michael@codesrc.com>
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
#endif

#ifdef STM32F4xx
#include "stm32f4xx.h"
#endif

#include "spi.h"

#include "fpga.h"
#include "led.h"
#include "time.h"

extern uint8_t _fpga_bitmap_start;
extern uint8_t _fpga_bitmap_end;
extern uint8_t _fpga_bitmap_size;

void s2s_fpgaInit()
{
	// FPGA SPI Configuration
	s2s_ledOn();
	HAL_GPIO_WritePin(FPGA_RST_GPIO_Port, FPGA_RST_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(nSPICFG_CS_GPIO_Port, nSPICFG_CS_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(
		nFGPA_CRESET_B_GPIO_Port, nFGPA_CRESET_B_Pin, GPIO_PIN_RESET);
	s2s_delay_us(1); // TODO only need 200 ns

	HAL_GPIO_WritePin(
		nFGPA_CRESET_B_GPIO_Port, nFGPA_CRESET_B_Pin, GPIO_PIN_SET);

	// 800uS for iCE40HX1K. tCR_SCK parameter in datasheet.
	s2s_delay_us(800);

	uint8_t* fpgaData = &_fpga_bitmap_start;
	uint32_t fpgaBytes = (uint32_t) &_fpga_bitmap_size;
	HAL_SPI_Transmit(&hspi1, fpgaData, fpgaBytes, 0xFFFFFFFF);

	// Wait 100 clocks
	uint8_t dummy[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	HAL_SPI_Transmit(&hspi1, dummy, sizeof(dummy), 0xFFFFFFFF);

	while (HAL_GPIO_ReadPin(nFGPA_CDONE_GPIO_Port, nFGPA_CDONE_Pin) ==
		GPIO_PIN_RESET)
	{
		s2s_ledOn();
		s2s_delay_ms(25);
		s2s_ledOff();
		s2s_delay_ms(25);

	}
	s2s_ledOff();

	// We're Done!! Release rst and allow processing to commence.
	s2s_delay_us(1);
	HAL_GPIO_WritePin(FPGA_RST_GPIO_Port, FPGA_RST_Pin, GPIO_PIN_RESET);
}

void s2s_fpgaReset()
{
	HAL_GPIO_WritePin(FPGA_RST_GPIO_Port, FPGA_RST_Pin, GPIO_PIN_SET);
	s2s_delay_clocks(4);
	HAL_GPIO_WritePin(FPGA_RST_GPIO_Port, FPGA_RST_Pin, GPIO_PIN_RESET);
}

