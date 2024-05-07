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

#include "geometry.h"
#include "sense.h"
#include "vendor.h"

#include <stdint.h>

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
	INTERMEDIATE = 0x10,
	CONFLICT = 0x18
} SCSI_STATUS;

typedef enum
{
	MSG_COMMAND_COMPLETE = 0,
	MSG_REJECT = 0x7,
	MSG_LINKED_COMMAND_COMPLETE = 0x0A,
	MSG_LINKED_COMMAND_COMPLETE_WITH_FLAG = 0x0B
} SCSI_MESSAGE;

typedef enum
{
	COMPAT_UNKNOWN,
	COMPAT_SCSI1,

	// Messages are being used, yet SCSI 2 mode is disabled.
	// This impacts interpretation of INQUIRY commands.
	COMPAT_SCSI2_DISABLED,

	COMPAT_SCSI2
} SCSI_COMPAT_MODE;

// Maximum value for bytes-per-sector.
#ifndef MAX_SECTOR_SIZE
#define MAX_SECTOR_SIZE 8192
#endif

#ifndef MIN_SECTOR_SIZE
#define MIN_SECTOR_SIZE 64
#endif

#ifndef SCSI2SD_BUFFER_SIZE
#define SCSI2SD_BUFFER_SIZE (MAX_SECTOR_SIZE * 8)
#endif

// Shadow parameters, possibly not saved to flash yet.
// Set via Mode Select
typedef struct
{
	uint16_t bytesPerSector;
} LiveCfg;

typedef struct
{
	uint8_t targetId;

	const S2S_TargetCfg* cfg;

	LiveCfg liveCfg;

	ScsiSense sense;

	uint16_t unitAttention; // Set to the sense qualifier key to be returned.

	// Only let the reserved initiator talk to us.
	// A 3rd party may be sending the RESERVE/RELEASE commands
	int reservedId; // 0 -> 7 if reserved. -1 if not reserved.
	int reserverId; // 0 -> 7 if reserved. -1 if not reserved.

	uint8_t syncOffset;
	uint8_t syncPeriod;

	uint8_t started; // Controlled by START STOP UNIT
} TargetState;

typedef struct
{
	// TODO reduce this buffer size and add a proper cache
	// Must be aligned for DMA
	// 65536 bytes is the DMA limit
	uint8_t data[SCSI2SD_BUFFER_SIZE];

	TargetState targets[S2S_MAX_TARGETS];
	TargetState* target;
	S2S_BoardCfg boardCfg;


	// Set to true (1) if the ATN flag was set, and we need to
	// enter the MESSAGE_OUT phase.
	int atnFlag;

	// Set to true (1) if the RST flag was set.
	volatile int resetFlag;

	// Set to sel register if the SEL flag was set.
	volatile int selFlag;

	// Set to true (1) if a parity error was observed.
	int parityError;

	int phase;

	int dataPtr; // Index into data, reset on [re]selection to savedDataPtr
	int savedDataPtr; // Index into data, initially 0.
	int dataLen;

	uint8_t cdb[20]; // command descriptor block
	uint8_t cdbLen; // 6, 10, or 12 byte message.
	int8_t lun; // Target lun, set by IDENTIFY message.
	uint8_t discPriv; // Disconnect priviledge.
	uint8_t compatMode; // SCSI_COMPAT_MODE

	// Only let the reserved initiator talk to us.
	// A 3rd party may be sending the RESERVE/RELEASE commands
	int initiatorId; // 0 -> 7. Set during the selection phase.

	// SCSI_STATUS value.
	// Change to CHECK_CONDITION when setting a SENSE value
	uint8_t status;

	uint8_t msgIn;
	uint8_t msgOut;

	void (*postDataOutHook)(void);

	uint8_t cmdCount;
	uint8_t selCount;
	uint8_t rstCount;
	uint8_t msgCount;
	uint8_t watchdogTick;
	uint8_t lastStatus;
	uint8_t lastSense;
	uint16_t lastSenseASC;
	uint8_t minSyncPeriod; // Debug use only.

	int needSyncNegotiationAck;
	int sdUnderrunCount;

	// Estimate of the SCSI host actual speed
	uint32_t hostSpeedKBs;
	int hostSpeedMeasured;
} ScsiDevice;

extern ScsiDevice scsiDev;

void process_Status(void);
int process_MessageIn(int releaseBusFree);
void enter_BusFree(void);

void scsiInit(void);
void scsiPoll(void);
void scsiDisconnect(void);
int scsiReconnect(void);


// Utility macros, consistent with the Linux Kernel code.
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
//#define likely(x)       (x)
//#define unlikely(x)     (x)
#endif
