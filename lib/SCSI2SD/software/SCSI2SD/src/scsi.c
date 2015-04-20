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
#pragma GCC push_options
#pragma GCC optimize("-flto")

#include "device.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "config.h"
#include "bits.h"
#include "diagnostic.h"
#include "disk.h"
#include "inquiry.h"
#include "led.h"
#include "mode.h"
#include "disk.h"
#include "time.h"
#include "cdrom.h"
#include "debug.h"

#include <string.h>

// Global SCSI device state.
ScsiDevice scsiDev;

static void enter_SelectionPhase(void);
static void process_SelectionPhase(void);
static void enter_BusFree(void);
static void enter_MessageIn(uint8 message);
static void enter_Status(uint8 status);
static void enter_DataIn(int len);
static void process_DataIn(void);
static void process_DataOut(void);
static void process_Command(void);

static void doReserveRelease(void);

static void enter_BusFree()
{
	// This delay probably isn't needed for most SCSI hosts, but it won't
	// hurt either. It's possible some of the samplers needed this delay.
	if (scsiDev.compatMode < COMPAT_SCSI2)
	{
		CyDelayUs(2);
	}

	if (scsiDev.status != GOOD && isDebugEnabled())
	{
		// We want to capture debug information for failure cases.
		CyDelay(64);
	}

	SCSI_ClearPin(SCSI_Out_BSY);
	// We now have a Bus Clear Delay of 800ns to release remaining signals.
	SCSI_CTL_PHASE_Write(0);

	// Wait for the initiator to cease driving signals
	// Bus settle delay + bus clear delay = 1200ns
	CyDelayUs(2);

	ledOff();
	scsiDev.phase = BUS_FREE;
}

static void enter_MessageIn(uint8 message)
{
	scsiDev.msgIn = message;
	scsiDev.phase = MESSAGE_IN;
}

void process_MessageIn()
{
	scsiEnterPhase(MESSAGE_IN);
	scsiWriteByte(scsiDev.msgIn);

	if (unlikely(scsiDev.atnFlag))
	{
		// If there was a parity error, we go
		// back to MESSAGE_OUT first, get out parity error message, then come
		// back here.
	}
	else if ((scsiDev.msgIn == MSG_LINKED_COMMAND_COMPLETE) ||
		(scsiDev.msgIn == MSG_LINKED_COMMAND_COMPLETE_WITH_FLAG))
	{
		// Go back to the command phase and start again.
		scsiDev.phase = COMMAND;
		scsiDev.parityError = 0;
		scsiDev.dataPtr = 0;
		scsiDev.savedDataPtr = 0;
		scsiDev.dataLen = 0;
		scsiDev.status = GOOD;
		transfer.blocks = 0;
		transfer.currentBlock = 0;
	}
	else /*if (scsiDev.msgIn == MSG_COMMAND_COMPLETE)*/
	{
		enter_BusFree();
	}
}

static void messageReject()
{
	scsiEnterPhase(MESSAGE_IN);
	scsiWriteByte(MSG_REJECT);
}

static void enter_Status(uint8 status)
{
	scsiDev.status = status;
	scsiDev.phase = STATUS;

	scsiDev.lastStatus = scsiDev.status;
	scsiDev.lastSense = scsiDev.target->sense.code;
	scsiDev.lastSenseASC = scsiDev.target->sense.asc;
}

void process_Status()
{
	scsiEnterPhase(STATUS);

	uint8 message;

	uint8 control = scsiDev.cdb[scsiDev.cdbLen - 1];
	if ((scsiDev.status == GOOD) && (control & 0x01))
	{
		// Linked command.
		scsiDev.status = INTERMEDIATE;
		if (control & 0x02)
		{
			message = MSG_LINKED_COMMAND_COMPLETE_WITH_FLAG;
		}
		else
		{
			message = MSG_LINKED_COMMAND_COMPLETE;
		}
	}
	else
	{
		message = MSG_COMMAND_COMPLETE;
	}
	scsiWriteByte(scsiDev.status);

	scsiDev.lastStatus = scsiDev.status;
	scsiDev.lastSense = scsiDev.target->sense.code;
	scsiDev.lastSenseASC = scsiDev.target->sense.asc;


	// Command Complete occurs AFTER a valid status has been
	// sent. then we go bus-free.
	enter_MessageIn(message);
}

static void enter_DataIn(int len)
{
	scsiDev.dataLen = len;
	scsiDev.phase = DATA_IN;
}

static void process_DataIn()
{
	uint32 len;

	if (scsiDev.dataLen > sizeof(scsiDev.data))
	{
		scsiDev.dataLen = sizeof(scsiDev.data);
	}

	len = scsiDev.dataLen - scsiDev.dataPtr;
	if (len > 0)
	{
		scsiEnterPhase(DATA_IN);
		scsiWrite(scsiDev.data + scsiDev.dataPtr, len);
		scsiDev.dataPtr += len;
	}

	if ((scsiDev.dataPtr >= scsiDev.dataLen) &&
		(transfer.currentBlock == transfer.blocks))
	{
		enter_Status(GOOD);
	}
}

static void process_DataOut()
{
	uint32 len;

	if (scsiDev.dataLen > sizeof(scsiDev.data))
	{
		scsiDev.dataLen = sizeof(scsiDev.data);
	}

	scsiDev.parityError = 0;
	len = scsiDev.dataLen - scsiDev.dataPtr;
	if (len > 0)
	{
		scsiEnterPhase(DATA_OUT);

		scsiRead(scsiDev.data + scsiDev.dataPtr, len);
		scsiDev.dataPtr += len;

		if (scsiDev.parityError &&
			(scsiDev.target->cfg->flags & CONFIG_ENABLE_PARITY) &&
			(scsiDev.compatMode >= COMPAT_SCSI2))
		{
			scsiDev.target->sense.code = ABORTED_COMMAND;
			scsiDev.target->sense.asc = SCSI_PARITY_ERROR;
			enter_Status(CHECK_CONDITION);
		}
	}

	if ((scsiDev.dataPtr >= scsiDev.dataLen) &&
		(transfer.currentBlock == transfer.blocks))
	{
		if (scsiDev.postDataOutHook != NULL)
		{
			scsiDev.postDataOutHook();
		}
		else
		{
			enter_Status(GOOD);
		}
	}
}

static const uint8 CmdGroupBytes[8] = {6, 10, 10, 6, 6, 12, 6, 6};
static void process_Command()
{
	int group;
	uint8 command;
	uint8 control;

	scsiEnterPhase(COMMAND);
	scsiDev.parityError = 0;

	memset(scsiDev.cdb, 0, sizeof(scsiDev.cdb));
	scsiDev.cdb[0] = scsiReadByte();

	group = scsiDev.cdb[0] >> 5;
	scsiDev.cdbLen = CmdGroupBytes[group];
	scsiRead(scsiDev.cdb + 1, scsiDev.cdbLen - 1);

	command = scsiDev.cdb[0];

	// Prefer LUN's set by IDENTIFY messages for newer hosts.
	if (scsiDev.lun < 0)
	{
		scsiDev.lun = scsiDev.cdb[1] >> 5;
	}

	control = scsiDev.cdb[scsiDev.cdbLen - 1];

	scsiDev.cmdCount++;

	if (unlikely(scsiDev.resetFlag))
	{
		// Don't log bogus commands
		scsiDev.cmdCount--;
		memset(scsiDev.cdb, 0xff, sizeof(scsiDev.cdb));
		return;
	}
	else if (scsiDev.parityError &&
		(scsiDev.target->cfg->flags & CONFIG_ENABLE_PARITY) &&
		(scsiDev.compatMode >= COMPAT_SCSI2))
	{
		scsiDev.target->sense.code = ABORTED_COMMAND;
		scsiDev.target->sense.asc = SCSI_PARITY_ERROR;
		enter_Status(CHECK_CONDITION);
	}
	else if ((control & 0x02) && ((control & 0x01) == 0))
	{
		// FLAG set without LINK flag.
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		enter_Status(CHECK_CONDITION);
	}
	else if (command == 0x12)
	{
		scsiInquiry();
	}
	else if (command == 0x03)
	{
		// REQUEST SENSE
		uint32 allocLength = scsiDev.cdb[4];

		// As specified by the SASI and SCSI1 standard.
		// Newer initiators won't be specifying 0 anyway.
		if (allocLength == 0) allocLength = 4;

		memset(scsiDev.data, 0, 256); // Max possible alloc length
		scsiDev.data[0] = 0xF0;
		scsiDev.data[2] = scsiDev.target->sense.code & 0x0F;

		scsiDev.data[3] = transfer.lba >> 24;
		scsiDev.data[4] = transfer.lba >> 16;
		scsiDev.data[5] = transfer.lba >> 8;
		scsiDev.data[6] = transfer.lba;

		// Additional bytes if there are errors to report
		scsiDev.data[7] = 10; // additional length
		scsiDev.data[12] = scsiDev.target->sense.asc >> 8;
		scsiDev.data[13] = scsiDev.target->sense.asc;

		// Silently truncate results. SCSI-2 spec 8.2.14.
		enter_DataIn(allocLength);

		// This is a good time to clear out old sense information.
		scsiDev.target->sense.code = NO_SENSE;
		scsiDev.target->sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	}
	// Some old SCSI drivers do NOT properly support
	// unitAttention. eg. the Mac Plus would trigger a SCSI reset
	// on receiving the unit attention response on boot, thus
	// triggering another unit attention condition.
	else if (scsiDev.target->unitAttention &&
		(scsiDev.target->cfg->flags & CONFIG_ENABLE_UNIT_ATTENTION))
	{
		scsiDev.target->sense.code = UNIT_ATTENTION;
		scsiDev.target->sense.asc = scsiDev.target->unitAttention;

		// If initiator doesn't do REQUEST SENSE for the next command, then
		// data is lost.
		scsiDev.target->unitAttention = 0;

		enter_Status(CHECK_CONDITION);
	}
	else if (scsiDev.lun)
	{
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = LOGICAL_UNIT_NOT_SUPPORTED;
		enter_Status(CHECK_CONDITION);
	}
	else if (command == 0x17 || command == 0x16)
	{
		doReserveRelease();
	}
	else if ((scsiDev.target->reservedId >= 0) &&
		(scsiDev.target->reservedId != scsiDev.initiatorId))
	{
		enter_Status(CONFLICT);
	}
	else if (scsiDiskCommand())
	{
		// Already handled.
		// check for the performance-critical read/write
		// commands ASAP.
	}
	else if (command == 0x1C)
	{
		scsiReceiveDiagnostic();
	}
	else if (command == 0x1D)
	{
		scsiSendDiagnostic();
	}
	else if (command == 0x3B)
	{
		scsiWriteBuffer();
	}
	else if (command == 0x3C)
	{
		scsiReadBuffer();
	}
	else if (
		!scsiCDRomCommand() &&
		!scsiModeCommand())
	{
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_COMMAND_OPERATION_CODE;
		enter_Status(CHECK_CONDITION);
	}

	// Successful
	if (scsiDev.phase == COMMAND) // No status set, and not in DATA_IN
	{
		enter_Status(GOOD);
	}

}

static void doReserveRelease()
{
	int extentReservation = scsiDev.cdb[1] & 1;
	int thirdPty = scsiDev.cdb[1] & 0x10;
	int thirdPtyId = (scsiDev.cdb[1] >> 1) & 0x7;
	uint8 command = scsiDev.cdb[0];

	int canRelease =
		(!thirdPty && (scsiDev.initiatorId == scsiDev.target->reservedId)) ||
			(thirdPty &&
				(scsiDev.target->reserverId == scsiDev.initiatorId) &&
				(scsiDev.target->reservedId == thirdPtyId)
			);

	if (extentReservation)
	{
		// Not supported.
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		enter_Status(CHECK_CONDITION);
	}
	else if (command == 0x17) // release
	{
		if ((scsiDev.target->reservedId < 0) || canRelease)
		{
			scsiDev.target->reservedId = -1;
			scsiDev.target->reserverId = -1;
		}
		else
		{
			enter_Status(CONFLICT);
		}
	}
	else // assume reserve.
	{
		if ((scsiDev.target->reservedId < 0) || canRelease)
		{
			scsiDev.target->reserverId = scsiDev.initiatorId;
			if (thirdPty)
			{
				scsiDev.target->reservedId = thirdPtyId;
			}
			else
			{
				scsiDev.target->reservedId = scsiDev.initiatorId;
			}
		}
		else
		{
			// Already reserved by someone else!
			enter_Status(CONFLICT);
		}
	}
}

static void scsiReset()
{
	scsiDev.rstCount++;
	ledOff();

	scsiPhyReset();
	SCSI_Out_Ctl_Write(0);

	scsiDev.parityError = 0;
	scsiDev.phase = BUS_FREE;
	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 0;
	scsiDev.lun = -1;
	scsiDev.compatMode = COMPAT_UNKNOWN;

	if (scsiDev.target)
	{
		if (scsiDev.target->unitAttention != POWER_ON_RESET)
		{
			scsiDev.target->unitAttention = SCSI_BUS_RESET;
		}
		scsiDev.target->reservedId = -1;
		scsiDev.target->reserverId = -1;
		scsiDev.target->sense.code = NO_SENSE;
		scsiDev.target->sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	}
	scsiDev.target = NULL;
	scsiDiskReset();

	scsiDev.postDataOutHook = NULL;

	// Sleep to allow the bus to settle down a bit.
	// We must be ready again within the "Reset to selection time" of
	// 250ms.
	// There is no guarantee that the RST line will be negated by then.
	// NOTE: We could be connected and powered by USB for configuration,
	// in which case TERMPWR cannot be supplied, and reset will ALWAYS
	// be true. Therefore, the sleep here must be slow to avoid slowing
	// USB comms
	CyDelay(1); // 1ms.
}

static void enter_SelectionPhase()
{
	// Ignore stale versions of this flag, but ensure we know the
	// current value if the flag is still set.
	scsiDev.atnFlag = 0;
	scsiDev.parityError = 0;
	scsiDev.dataPtr = 0;
	scsiDev.savedDataPtr = 0;
	scsiDev.dataLen = 0;
	scsiDev.status = GOOD;
	scsiDev.phase = SELECTION;
	scsiDev.lun = -1;
	scsiDev.discPriv = 0;

	scsiDev.initiatorId = -1;
	scsiDev.target = NULL;

	transfer.blocks = 0;
	transfer.currentBlock = 0;

	scsiDev.postDataOutHook = NULL;
}

static void process_SelectionPhase()
{
	if (scsiDev.compatMode < COMPAT_SCSI2)
	{
		// Required for some older SCSI1 devices using a 5380 chip.
		CyDelayUs(100);
	}

	int sel = SCSI_ReadFilt(SCSI_Filt_SEL);
	int bsy = SCSI_ReadFilt(SCSI_Filt_BSY);

	// Only read these pins AFTER SEL and BSY - we don't want to catch them
	// during a transition period.
	uint8 mask = scsiReadDBxPins();
	int maskBitCount = countBits(mask);
	int goodParity = (Lookup_OddParity[mask] == SCSI_ReadPin(SCSI_In_DBP));
	int atnFlag = SCSI_ReadFilt(SCSI_Filt_ATN);

	int tgtIndex;
	TargetState* target = NULL;
	for (tgtIndex = 0; tgtIndex < MAX_SCSI_TARGETS; ++tgtIndex)
	{
		if (mask & (1 << scsiDev.targets[tgtIndex].targetId))
		{
			target = &scsiDev.targets[tgtIndex];
			break;
		}
	}
	if (!bsy && sel &&
		target &&
		(goodParity || !(target->cfg->flags & CONFIG_ENABLE_PARITY) || !atnFlag) &&
		likely(maskBitCount <= 2))
	{
		scsiDev.target = target;

		// Do we enter MESSAGE OUT immediately ? SCSI 1 and 2 standards says
		// move to MESSAGE OUT if ATN is true before we assert BSY.
		// The initiator should assert ATN with SEL.
		scsiDev.atnFlag = atnFlag;

		// Unit attention breaks many older SCSI hosts. Disable it completely
		// for SCSI-1 (and older) hosts, regardless of our configured setting.
		// Enable the compatability mode also as many SASI and SCSI1
		// controllers don't generate parity bits.
		if (!scsiDev.atnFlag)
		{
			target->unitAttention = 0;
			scsiDev.compatMode = COMPAT_SCSI1;
		}
		else if (scsiDev.compatMode == COMPAT_UNKNOWN)
		{
			scsiDev.compatMode = COMPAT_SCSI2;
		}

		// We've been selected!
		// Assert BSY - Selection success!
		// must happen within 200us (Selection abort time) of seeing our
		// ID + SEL.
		// (Note: the initiator will be waiting the "Selection time-out delay"
		// for our BSY response, which is actually a very generous 250ms)
		SCSI_SetPin(SCSI_Out_BSY);
		ledOn();

		scsiDev.selCount++;

		// Wait until the end of the selection phase.
		while (likely(!scsiDev.resetFlag))
		{
			if (!SCSI_ReadFilt(SCSI_Filt_SEL))
			{
				break;
			}
		}

		// Save our initiator now that we're no longer in a time-critical
		// section.
		// SCSI1/SASI initiators may not set their own ID.
		{
			int i;
			uint8_t initiatorMask = mask ^ (1 << target->targetId);
			scsiDev.initiatorId = -1;
			for (i = 0; i < 8; ++i)
			{
				if (initiatorMask & (1 << i))
				{
					scsiDev.initiatorId = i;
					break;
				}
			}
		}

		scsiDev.phase = COMMAND;
	}
	else if (!sel)
	{
		scsiDev.phase = BUS_BUSY;
	}
}

static void process_MessageOut()
{
	scsiEnterPhase(MESSAGE_OUT);

	scsiDev.atnFlag = 0;
	scsiDev.parityError = 0;
	scsiDev.msgOut = scsiReadByte();
	scsiDev.msgCount++;

	if (scsiDev.parityError &&
		(scsiDev.target->cfg->flags & CONFIG_ENABLE_PARITY) &&
		(scsiDev.compatMode >= COMPAT_SCSI2))
	{
		// Skip the remaining message bytes, and then start the MESSAGE_OUT
		// phase again from the start. The initiator will re-send the
		// same set of messages.
		while (SCSI_ReadFilt(SCSI_Filt_ATN) && !scsiDev.resetFlag)
		{
			scsiReadByte();
		}

		// Go-back and try the message again.
		scsiDev.atnFlag = 1;
		scsiDev.parityError = 0;
	}
	else if (scsiDev.msgOut == 0x00)
	{
		// COMMAND COMPLETE. but why would the target be receiving this ? nfi.
		enter_BusFree();
	}
	else if (scsiDev.msgOut == 0x06)
	{
		// ABORT
		scsiDiskReset();
		enter_BusFree();
	}
	else if (scsiDev.msgOut == 0x0C)
	{
		// BUS DEVICE RESET

		scsiDiskReset();

		scsiDev.target->unitAttention = SCSI_BUS_RESET;

		// ANY initiator can reset the reservation state via this message.
		scsiDev.target->reservedId = -1;
		scsiDev.target->reserverId = -1;
		enter_BusFree();
	}
	else if (scsiDev.msgOut == 0x05)
	{
		// Initiate Detected Error
		// Ignore for now
	}
	else if (scsiDev.msgOut == 0x0F)
	{
		// INITIATE RECOVERY
		// Ignore for now
	}
	else if (scsiDev.msgOut == 0x10)
	{
		// RELEASE RECOVERY
		// Ignore for now
		enter_BusFree();
	}
	else if (scsiDev.msgOut == MSG_REJECT)
	{
		// Message Reject
		// Oh well.
		scsiDev.resetFlag = 1;
	}
	else if (scsiDev.msgOut == 0x08)
	{
		// NOP
	}
	else if (scsiDev.msgOut == 0x09)
	{
		// Message Parity Error
		// Go back and re-send the last message.
		scsiDev.phase = MESSAGE_IN;
	}
	else if (scsiDev.msgOut & 0x80) // 0x80 -> 0xFF
	{
		// IDENTIFY
		if ((scsiDev.msgOut & 0x18) || // Reserved bits set.
			(scsiDev.msgOut & 0x20))  // We don't have any target routines!
		{
			messageReject();
		}

		scsiDev.lun = scsiDev.msgOut & 0x7;
		scsiDev.discPriv = 
			((scsiDev.msgOut & 0x40) && (scsiDev.initiatorId >= 0))
				? 1 : 0;
	}
	else if (scsiDev.msgOut >= 0x20 && scsiDev.msgOut <= 0x2F)
	{
		// Two byte message. We don't support these. read and discard.
		scsiReadByte();
	}
	else if (scsiDev.msgOut == 0x01)
	{
		int i;

		// Extended message.
		int msgLen = scsiReadByte();
		if (msgLen == 0) msgLen = 256;
		uint8_t extmsg[256];
		for (i = 0; i < msgLen && !scsiDev.resetFlag; ++i)
		{
			// Discard bytes.
			extmsg[i] = scsiReadByte();
		}
		
		if (extmsg[0] == 3 && msgLen == 2) // Wide Data Request
		{
			// Negotiate down to 8bit
			scsiEnterPhase(MESSAGE_IN);
			static const uint8_t WDTR[] = {0x01, 0x02, 0x03, 0x00};
			scsiWrite(WDTR, sizeof(WDTR));
		}
		else if (extmsg[0] == 1 && msgLen == 5) // Synchronous data request
		{
			// Negotiate back to async
			scsiEnterPhase(MESSAGE_IN);
			static const uint8_t SDTR[] = {0x01, 0x03, 0x01, 0x00, 0x00};
			scsiWrite(SDTR, sizeof(SDTR));
		}
		else
		{
			// Not supported
			messageReject();
		}
	}
	else
	{
		messageReject();
	}

	// Re-check the ATN flag in case it stays asserted.
	scsiDev.atnFlag |= SCSI_ReadFilt(SCSI_Filt_ATN);
}

void scsiPoll(void)
{
	if (unlikely(scsiDev.resetFlag))
	{
		scsiReset();
		if ((scsiDev.resetFlag = SCSI_ReadFilt(SCSI_Filt_RST)))
		{
			// Still in reset phase. Do not try and process any commands.
			return;
		}
	}

	switch (scsiDev.phase)
	{
	case BUS_FREE:
		if (SCSI_ReadFilt(SCSI_Filt_BSY))
		{
			scsiDev.phase = BUS_BUSY;
		}
		// The Arbitration phase is optional for SCSI1/SASI hosts if there is only
		// one initiator in the chain. Support this by moving
		// straight to selection if SEL is asserted.
		// ie. the initiator won't assert BSY and it's own ID before moving to selection.
		else if (SCSI_ReadFilt(SCSI_Filt_SEL))
		{
			enter_SelectionPhase();
		}
	break;

	case BUS_BUSY:
		// Someone is using the bus. Perhaps they are trying to
		// select us.
		if (SCSI_ReadFilt(SCSI_Filt_SEL))
		{
			enter_SelectionPhase();
		}
		else if (!SCSI_ReadFilt(SCSI_Filt_BSY))
		{
			scsiDev.phase = BUS_FREE;
		}
	break;

	case ARBITRATION:
		// TODO Support reselection.
		break;

	case SELECTION:
		process_SelectionPhase();
	break;

	case RESELECTION:
		// Not currently supported!
	break;

	case COMMAND:
		// Do not check ATN here. SCSI 1 & 2 initiators must set ATN
		// and SEL together upon entering the selection phase if they
		// want to send a message (IDENTIFY) immediately.
		if (scsiDev.atnFlag)
		{
			process_MessageOut();
		}
		else
		{
			process_Command();
		}
	break;

	case DATA_IN:
		scsiDev.atnFlag |= SCSI_ReadFilt(SCSI_Filt_ATN);
		if (scsiDev.atnFlag)
		{
			process_MessageOut();
		}
		else
		{
			process_DataIn();
		}
	break;

	case DATA_OUT:
		scsiDev.atnFlag |= SCSI_ReadFilt(SCSI_Filt_ATN);
		if (scsiDev.atnFlag)
		{
			process_MessageOut();
		}
		else
		{
			process_DataOut();
		}
	break;

	case STATUS:
		scsiDev.atnFlag |= SCSI_ReadFilt(SCSI_Filt_ATN);
		if (scsiDev.atnFlag)
		{
			process_MessageOut();
		}
		else
		{
			process_Status();
		}
	break;

	case MESSAGE_IN:
		scsiDev.atnFlag |= SCSI_ReadFilt(SCSI_Filt_ATN);
		if (scsiDev.atnFlag)
		{
			process_MessageOut();
		}
		else
		{
			process_MessageIn();
		}

	break;

	case MESSAGE_OUT:
		process_MessageOut();
	break;
	}
}

void scsiInit()
{
	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 1;
	scsiDev.phase = BUS_FREE;
	scsiDev.target = NULL;
	scsiDev.compatMode = COMPAT_UNKNOWN;

	int i;
	for (i = 0; i < MAX_SCSI_TARGETS; ++i)
	{
		const TargetConfig* cfg = getConfigByIndex(i);
		if (cfg && (cfg->scsiId & CONFIG_TARGET_ENABLED))
		{
			scsiDev.targets[i].targetId = cfg->scsiId & CONFIG_TARGET_ID_BITS;
			scsiDev.targets[i].cfg = cfg;

			scsiDev.targets[i].liveCfg.bytesPerSector = cfg->bytesPerSector;
		}
		else
		{
			scsiDev.targets[i].targetId = 0xff;
			scsiDev.targets[i].cfg = NULL;
		}
		scsiDev.targets[i].reservedId = -1;
		scsiDev.targets[i].reserverId = -1;
		scsiDev.targets[i].unitAttention = POWER_ON_RESET;
		scsiDev.targets[i].sense.code = NO_SENSE;
		scsiDev.targets[i].sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	}
}

void scsiDisconnect()
{
	scsiEnterPhase(MESSAGE_IN);
	scsiWriteByte(0x02); // save data pointer
	scsiWriteByte(0x04); // disconnect msg.

	// For now, the caller is responsible for tracking the disconnected
	// state, and calling scsiReconnect.
	// Ideally the client would exit their loop and we'd implement this
	// as part of scsiPoll
	int phase = scsiDev.phase;
	enter_BusFree();
	scsiDev.phase = phase;
}

int scsiReconnect()
{
	int reconnected = 0;

	int sel = SCSI_ReadFilt(SCSI_Filt_SEL);
	int bsy = SCSI_ReadFilt(SCSI_Filt_BSY);
	if (!sel && !bsy)
	{
		CyDelayUs(1);
		sel = SCSI_ReadFilt(SCSI_Filt_SEL);
		bsy = SCSI_ReadFilt(SCSI_Filt_BSY);
	}

	if (!sel && !bsy)
	{
		// Arbitrate.
		ledOn();
		uint8_t scsiIdMask = 1 << scsiDev.target->targetId;
		SCSI_Out_Bits_Write(scsiIdMask);
		SCSI_Out_Ctl_Write(1); // Write bits manually.
		SCSI_SetPin(SCSI_Out_BSY);

		CyDelayUs(3); // arbitrate delay. 2.4us.

		uint8_t dbx = scsiReadDBxPins();
		sel = SCSI_ReadFilt(SCSI_Filt_SEL);
		if (sel || ((dbx ^ scsiIdMask) > scsiIdMask))
		{
			// Lost arbitration.
			SCSI_Out_Ctl_Write(0);
			SCSI_ClearPin(SCSI_Out_BSY);
			ledOff();
		}
		else
		{
			// Won arbitration
			SCSI_SetPin(SCSI_Out_SEL);
			CyDelayUs(1); // Bus clear + Bus settle.

			// Reselection phase
			SCSI_CTL_PHASE_Write(__scsiphase_io);
			SCSI_Out_Bits_Write(scsiIdMask | (1 << scsiDev.initiatorId));
			scsiDeskewDelay(); // 2 deskew delays
			scsiDeskewDelay(); // 2 deskew delays
			SCSI_ClearPin(SCSI_Out_BSY);
			CyDelayUs(1);  // Bus Settle Delay

			uint32_t waitStart_ms = getTime_ms();
			bsy = SCSI_ReadFilt(SCSI_Filt_BSY);
			// Wait for initiator.
			while (
				!bsy &&
				!scsiDev.resetFlag &&
				(elapsedTime_ms(waitStart_ms) < 250))
			{
				bsy = SCSI_ReadFilt(SCSI_Filt_BSY);
			}

			if (bsy)
			{
				SCSI_SetPin(SCSI_Out_BSY);
				scsiDeskewDelay(); // 2 deskew delays
				scsiDeskewDelay(); // 2 deskew delays
				SCSI_ClearPin(SCSI_Out_SEL);

				// Prepare for the initial IDENTIFY message.
				SCSI_Out_Ctl_Write(0);
				scsiEnterPhase(MESSAGE_IN);

				// Send identify command
				scsiWriteByte(0x80);

				scsiEnterPhase(scsiDev.phase);
				reconnected = 1;
			}
			else
			{
				// reselect timeout.
				SCSI_Out_Ctl_Write(0);
				SCSI_ClearPin(SCSI_Out_SEL);
				SCSI_CTL_PHASE_Write(0);
				ledOff();
			}
		}
	}
	return reconnected;
}

#pragma GCC pop_options
