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
#ifndef SCSI_H
#define SCSI_H

// SCSI documentation goes here
// SCSI-2.
// Single LUN
// No tagged-queuing support - single command at a time.
// All read/write commands disconnect. State SD card latency.
// Fixed 512 byte sector size.
// 2TB limit, based on 32bit LBA (read16/write16 not supported)

// Set this to true to log SCSI commands and status information via
// USB HID packets.  The can be captured and viewed in wireshark.
// For windows users, capture using USBPcap http://desowin.org/usbpcap/
#define MM_DEBUG 0

#include "geometry.h"
#include "sense.h"

typedef enum
{
	// internal bits
	__scsiphase_msg = 1,
	__scsiphase_cd = 2,
	__scsiphase_io = 4,

	BUS_FREE = -1,
	BUS_BUSY = -2,
	ARBITRATION = -3,
	SELECTION = -4,
	RESELECTION = -5,
	STATUS = __scsiphase_cd | __scsiphase_io,
	COMMAND = __scsiphase_cd,
	DATA_IN = __scsiphase_io,
	DATA_OUT = 0,
	MESSAGE_IN = __scsiphase_msg | __scsiphase_cd | __scsiphase_io,
	MESSAGE_OUT = __scsiphase_msg | __scsiphase_cd
} SCSI_PHASE;

typedef enum
{
	GOOD = 0,
	CHECK_CONDITION = 2,
	BUSY = 0x8,
	CONFLICT = 0x18
} SCSI_STATUS;

typedef enum
{
	MSG_COMMAND_COMPLETE = 0,
	MSG_REJECT = 0x7

} SCSI_MESSAGE;

typedef struct
{
	uint8_t scsiIdMask;

	// Set to true (1) if the ATN flag was set, and we need to
	// enter the MESSAGE_OUT phase.
	volatile int atnFlag;

	// Set to true (1) if the RST flag was set.
	volatile int resetFlag;

	// Set to true (1) if a parity error was observed.
	int parityError;

	int phase;

	uint8 data[SCSI_BLOCK_SIZE];
	int dataPtr; // Index into data, reset on [re]selection to savedDataPtr
	int savedDataPtr; // Index into data, initially 0.
	int dataLen;

	uint8 cdb[12]; // command descriptor block

	// Only let the reserved initiator talk to us.
	// A 3rd party may be sending the RESERVE/RELEASE commands
	int initiatorId; // 0 -> 7. Set during the selection phase.
	int reservedId; // 0 -> 7 if reserved. -1 if not reserved.
	int reserverId; // 0 -> 7 if reserved. -1 if not reserved.

	// SCSI_STATUS value.
	// Change to SCSI_STATUS_CHECK_CONDITION when setting a SENSE value
	uint8 status;

	ScsiSense sense;

	uint16 unitAttention; // Set to the sense qualifier key to be returned.

	uint8 msgIn;
	uint8 msgOut;

#ifdef MM_DEBUG
	uint8 cmdCount;
	uint8 selCount;
	uint8 rstCount;
	uint8 msgCount;
	uint8 watchdogTick;
	uint8 lastStatus;
	uint8 lastSense;
#endif
} ScsiDevice;

extern ScsiDevice scsiDev;

void scsiInit(void);
void scsiPoll(void);


#endif
