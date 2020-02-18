//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
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

#include "led.h"

#include "stm32f2xx.h"

void s2s_ledInit()
{
	// LED GPIO pin is already initialised as an output with pull-down.
	// At this stage we can remove the pull-down
	s2s_ledOff();

	GPIO_InitTypeDef ledDef = {
		LED_IO_Pin, // Pin
		GPIO_MODE_OUTPUT_PP, // Mode
		GPIO_NOPULL, // Pull(ups)
		GPIO_SPEED_FREQ_LOW, // Speed (2MHz)
		0 // Alternate function
	};
	HAL_GPIO_Init(LED_IO_GPIO_Port, &ledDef);
}

void s2s_ledOn()
{
	HAL_GPIO_WritePin(LED_IO_GPIO_Port, LED_IO_Pin, GPIO_PIN_SET);
}

void s2s_ledOff()
{
	HAL_GPIO_WritePin(LED_IO_GPIO_Port, LED_IO_Pin, GPIO_PIN_RESET);
}

