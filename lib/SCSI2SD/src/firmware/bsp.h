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

#ifndef S2S_BSP_h
#define S2S_BSP_h

#include <stdint.h>

// For the STM32F205, DMA bursts may not cross 1KB address boundaries.
// The maximum burst is 16 bytes.
#define S2S_DMA_ALIGN __attribute__((aligned(1024)))

uint32_t s2s_getSdRateKBs();

#endif

