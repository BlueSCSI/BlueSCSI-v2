//	Copyright (C) 2020 Michael McMaster <michael@codesrc.com>
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

#include "hwversion.h"

#ifdef STM32F2xx
#include "stm32f2xx.h"
#endif

#ifdef STM32F4xx
#include "stm32f4xx.h"
#endif

#include "gpio.h"

#include "config.h"
#include "time.h"

// Store hardware version details to the "One Time Programmable" flash memory
// This is 512 bytes that can only be written to ONCE and once only.
// It can be read by dfu-util, so once we write the marker it can be
// detected even when the firmware isn't running.

// Values for STM32F401RE
const size_t OTP_SIZE = 512;
const size_t OTP_BLOCKS = 16;
const size_t OTP_BLOCK_SIZE = OTP_SIZE / OTP_BLOCKS;

const size_t OTP_BLOCK_NUM = 0;

// Define some pointers for writing, but also to allow easy reading back values
const uint8_t *otp = (uint8_t*)(FLASH_OTP_BASE + OTP_BLOCK_NUM * OTP_BLOCK_SIZE);
const uint32_t *otp32 = (uint32_t*)(FLASH_OTP_BASE + OTP_BLOCK_NUM * OTP_BLOCK_SIZE);
const uint8_t *lock = (uint8_t*)(FLASH_OTP_BASE + OTP_SIZE + OTP_BLOCK_NUM);

const uint32_t marker = 0x06002020;

static void
checkHwSensePins()
{
	// Check the board version is correct.
	// Sense pins are configued as pullup, and connected to GND for 2020 hw,
	// or N/C for v6 ref F or older
	if (HAL_GPIO_ReadPin(VER_ID1_GPIO_Port, VER_ID1_Pin) ||
		HAL_GPIO_ReadPin(VER_ID2_GPIO_Port, VER_ID2_Pin))
	{
		// Oh dear, wrong version. Do not pass go.
		while (1) {}
	}
}

void
s2s_checkHwVersion()
{
return; // TODO FIX FOR 2021
	checkHwSensePins();

	// Write a marker to flash that can be read by dfu-util now that we know
	// the version is correct.
	if (*otp32 != marker)
	{
		// Double-check the pins are good.
		s2s_delay_ms(10);
		checkHwSensePins();

		// Well, pins are good. Make sure marker isn't set at all
		if (*otp32 != 0xffffffff)
		{
			// Some other version was set.
			while (1) {}
		}

		// Write the marker to flash.
		if (HAL_FLASH_Unlock() != HAL_OK)
		{
			return;
		}

		// Write 4 bytes to the start of OTP.
		if (HAL_FLASH_Program(
			FLASH_TYPEPROGRAM_WORD,
			(uint32_t)otp,
			marker) != HAL_OK)
		{
			HAL_FLASH_Lock();
			return;
		}

		// Lock OTP page
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)lock, 0x00)) {
			HAL_FLASH_Lock();
			return;
		}

		HAL_FLASH_Lock();
	}
}

