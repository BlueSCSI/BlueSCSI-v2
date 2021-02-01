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
#ifndef S2S_SPINLOCK_H
#define S2S_SPINLOCK_H

#ifdef STM32F2xx
#include "stm32f2xx.h"
#endif

#ifdef STM32F4xx
#include "stm32f4xx.h"
#endif

#define s2s_lock_t volatile uint32_t
#define s2s_lock_init 0

// Spinlock functions for Cortex-M3, based on ARM Application Note 321,
// ARM Cortex-M Programming Guide to Memory Barrier Instructions, 4.19
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dai0321a/BIHEJCHB.html
//
// s2s_spin_lock must NOT be used when mixing the main loop with a ISR, since
// the main code will never get a chance to unlock while the ISR is active.
// Use trylock in the ISR instead.

int s2s_spin_trylock(s2s_lock_t* lock);
void s2s_spin_lock(s2s_lock_t* lock);
void s2s_spin_unlock(s2s_lock_t* lock);

#endif
