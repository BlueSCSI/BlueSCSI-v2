//	Copyright (C) 2015 James Laird-Wah <james@laird-wah.net>
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

#include <core_cm3_psoc5.h>
#include <stdint.h>
#include <cyfitter.h>
#include <cytypes.h>
#include <device.h>
#include "trace.h"

// configure desired baud rate on the SWV pin.
// up to the lower of CPU_clk/2 or 33MHz
#define BAUD_RATE 921600

// Cortex-M3 Trace Port Interface Unit (TPIU)
#define TPIU_BASE	0xe0040000
#define MMIO32(addr) *((volatile uint32_t*)(addr))
#define TPIU_SSPSR	MMIO32(TPIU_BASE + 0x000)
#define TPIU_CSPSR	MMIO32(TPIU_BASE + 0x004)
#define TPIU_ACPR	MMIO32(TPIU_BASE + 0x010)
#define TPIU_SPPR	MMIO32(TPIU_BASE + 0x0F0)
#define TPIU_FFSR	MMIO32(TPIU_BASE + 0x300)
#define TPIU_FFCR	MMIO32(TPIU_BASE + 0x304)

#define TPIU_CSPSR_BYTE (1 << 0)
#define TPIU_CSPSR_HALFWORD	(1 << 1)
#define TPIU_CSPSR_WORD	(1 << 3)

#define TPIU_SPPR_SYNC	(0x0)
#define TPIU_SPPR_ASYNC_MANCHESTER	(0x1)
#define TPIU_SPPR_ASYNC_NRZ	(0x2)

#define TPIU_FFCR_ENFCONT	(1 << 1)

void traceInit(void) {
	// enable the trace module clocks
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	
	// set SWV clock = CPU clock / 2, and enable
	CY_SET_REG8(CYDEV_MFGCFG_MLOGIC_DEBUG, 0xc); // swv_clk_sel = CPU_clk / 2, swv_clk enable
	
	// unlock the ETM/TPIU registers
	*((volatile uint32_t*)0xE0000FB0) = 0xC5ACCE55;
	
	// NRZ is "UART mode"
	TPIU_SPPR = TPIU_SPPR_ASYNC_NRZ;
	// prescaler, 0 = divide by 1
	TPIU_ACPR = (BCLK__BUS_CLK__HZ/2/BAUD_RATE) - 1;
	// can write 1, 2 or 4 byte ports
	TPIU_CSPSR = TPIU_CSPSR_BYTE;

	// bypass formatter (puts sync & stuff in otherwise)
	TPIU_FFCR &= ~TPIU_FFCR_ENFCONT;
	// enable ITM, enable the first 2 stimulus ports
	ITM->TCR = ITM_TCR_ITMENA_Msk;
	ITM->TER = 0x3;
	
	trace(trace_begin);
}
