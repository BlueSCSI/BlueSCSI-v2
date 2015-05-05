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

// Trace event IDs to be output. 1 and 9 are generated as headers on ports 0
// and 1 respectively, and should not be used.
enum trace_event {
	trace_begin = 0,

	// function entries - SCSI
	trace_scsiRxCompleteISR = 0x50,
	trace_scsiTxCompleteISR,
	trace_scsiResetISR,
	trace_doRxSingleDMA,
	trace_doTxSingleDMA,
	trace_scsiPhyReset,

	// spin loops - SCSI
	trace_spinTxComplete = 0x20,
	trace_spinReadDMAPoll,
	trace_spinWriteDMAPoll,
	trace_spinPhyTxFifo,
	trace_spinPhyRxFifo,
	trace_spinDMAReset,

	// SD
	trace_spinSpiByte = 0x30,
	trace_spinSDRxFIFO,
	trace_spinSDBusy,
	trace_spinSDDMA,
	trace_spinSDCompleteWrite,
	trace_spinSDCompleteRead,

	// completion
	trace_sdSpiByte = 0x40,
};

void traceInit(void);

#ifdef TRACE
// normally the code spins waiting for the trace FIFO to be ready for each event
// if you are debugging a timing-sensitive problem, define TRACE_IMPATIENT and
// expect some dropped packets
#ifdef TRACE_IMPATIENT
    #define wait_fifo(port)  ;
#else
    #define wait_fifo(port)  while (!(ITM->PORT[port].u32));
#endif

	#include <core_cm3_psoc5.h>
	static inline void trace(enum trace_event ch) {
		wait_fifo(0);
		ITM->PORT[0].u8 = ch;
	}
	// use a different stimulus port for ISRs to avoid a race
	static inline void traceIrq(enum trace_event ch) {
		wait_fifo(1);
		ITM->PORT[1].u8 = ch;
	}
#else
	#define trace(ev)
	#define traceIrq(ev)
#endif
