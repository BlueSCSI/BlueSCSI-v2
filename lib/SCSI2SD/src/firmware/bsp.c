//	Copyright (C) 2016 Michael McMaster <michael@codesrc.com>
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

#include "bsp.h"
#include "stm32f2xx_hal.h"


static int usingFastClock = 0;

// TODO keep clock routines consistent with those in STM32Cubemx main.c

uint32_t s2s_getSdRateKBs()
{
	if (usingFastClock)
	{
		return 18000; // ((72MHz / 2) / 8bits) * 4bitparallel
	}
	else
	{
		return 12000; // ((48MHz / 2) / 8bits) * 4bitparallel
	}
}

// The standard clock is 108MHz with 48MHz SDIO clock
void s2s_setNormalClock()
{
	if (usingFastClock)
	{
		usingFastClock = 0;

		// Stop using PLL as system clock
		RCC_ClkInitTypeDef RCC_ClkInitStruct;
		RCC_ClkInitStruct.ClockType =
			RCC_CLOCKTYPE_SYSCLK |
			RCC_CLOCKTYPE_PCLK1 |
			RCC_CLOCKTYPE_PCLK2;
		RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
		RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
		HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);

		// Change PLL
		RCC_OscInitTypeDef RCC_OscInitStruct;
		RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
		RCC_OscInitStruct.HSEState = RCC_HSE_ON;
		RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
		RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
		RCC_OscInitStruct.PLL.PLLM = 20;
		RCC_OscInitStruct.PLL.PLLN = 432;
		RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
		RCC_OscInitStruct.PLL.PLLQ = 9; // 48MHz.
		HAL_RCC_OscConfig(&RCC_OscInitStruct);

		// Resume using PLL for system clock
		RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
		HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
	}
}

// The fast clock is 108MHz with 72MHz SDIO clock
// PLL needs to be between 67MHz and 75MHz.
// USB will NOT work in this mode.
// Unfortunately this is the only way to get faster SDIO transfers
// on STM32F205 due to errata on the SDIO Bypass Clock mode.
void s2s_setFastClock()
{
	if (!usingFastClock)
	{
		usingFastClock = 1;

		// Stop using PLL as system clock
		RCC_ClkInitTypeDef RCC_ClkInitStruct;
		RCC_ClkInitStruct.ClockType =
			RCC_CLOCKTYPE_SYSCLK |
			RCC_CLOCKTYPE_PCLK1 |
			RCC_CLOCKTYPE_PCLK2;
		RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
		RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
		RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
		RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
		HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);

		// Change PLL
		RCC_OscInitTypeDef RCC_OscInitStruct;
		RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
		RCC_OscInitStruct.HSEState = RCC_HSE_ON;
		RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
		RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
		RCC_OscInitStruct.PLL.PLLM = 20;
		RCC_OscInitStruct.PLL.PLLN = 432;
		RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
		RCC_OscInitStruct.PLL.PLLQ = 6; // 72MHz.
		HAL_RCC_OscConfig(&RCC_OscInitStruct);

		// Resume using PLL for system clock
		RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
		HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
	}
}


