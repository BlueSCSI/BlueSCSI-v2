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

#include "bootloader.h"

#ifdef STM32F2xx
#include "stm32f2xx_hal.h"
#endif
#ifdef STM32F4xx
#include "stm32f4xx_hal.h"
#endif

#define MAGIC 0xDEADBEEF

// "System Memory" address of the bootloader. Specific to stm32f2xxxx
#define SYSMEM_RESET_VECTOR            0x1fff0000

extern void OrigSystemInit(void);

// This symbol is in a section of ram that isn't initialised by the
// Reset_Handler that calls SystemInit.
static uint32_t resetMagic __attribute__ ((section("bootloaderMagic")));

// Override STM32CubeMX supplied SystemInit method.
void SystemInit(void)
{
	if (resetMagic == MAGIC)
	{
		void (*bootloader)(void) = (void (*)(void)) (*(uint32_t*)(SYSMEM_RESET_VECTOR + 4));
		resetMagic = 0;
		__set_MSP(* ((uint32_t*)SYSMEM_RESET_VECTOR));
		bootloader();
		while (1) {}
	}
	else
	{
		OrigSystemInit();
	}
}

void s2s_enterBootloader()
{
	resetMagic = MAGIC;
	NVIC_SystemReset();
}

