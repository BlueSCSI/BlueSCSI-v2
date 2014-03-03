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

#include <string.h>

// Global SCSI device state.
ScsiDevice scsiDev;

static void enter_SelectionPhase(void);
static void process_SelectionPhase(void);
static void enter_BusFree(void);
static void enter_MessageIn(uint8 message);
static void process_MessageIn(void);
static void enter_Status(uint8 status);
static void process_Status(void);
static void enter_DataIn(int len);
static void process_DataIn(void);
static void process_DataOut(void);
static void process_Command(void);

static void doReserveRelease(void);

static uint8_t CmdTimerComplete = 0;
CY_ISR(CommandTimerISR)
{
	CmdTimerComplete = 1;
}

static void enter_BusFree()
{
	// Spin until the 10us timer has stopped.
	// Required for Akai MPC3000, and possibly other broken controllers.
	// 1,2us: Cannot see SCSI device.
	// 5us: Can see SCSI device, format fails
	// 10us: Format succeeds.
	// 25us: Format fails.
	while (!CmdTimerComplete) {}
	SCSI_CMD_TIMER_Stop();
	
	SCSI_ClearPin(SCSI_Out_BSY);
	// We now have a Bus Clear Delay of 800ns to release remaining signals.
	SCSI_ClearPin(SCSI_Out_MSG);
	SCSI_ClearPin(SCSI_Out_CD);
	SCSI_CTL_IO_Write(0);

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

static void process_MessageIn()
{
	scsiEnterPhase(MESSAGE_IN);
	scsiWriteByte(scsiDev.msgIn);

	if (scsiDev.atnFlag)
	{
		// If there was a parity error, we go
		// back to MESSAGE_OUT first, get out parity error message, then come
		// back here.
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


	#ifdef MM_DEBUG
	scsiDev.lastStatus = scsiDev.status;
	scsiDev.lastSense = scsiDev.sense.code;
	#endif
}

static void process_Status()
{
	scsiEnterPhase(STATUS);
	scsiWriteByte(scsiDev.status);

	#ifdef MM_DEBUG
	scsiDev.lastStatus = scsiDev.status;
	scsiDev.lastSense = scsiDev.sense.code;
	#endif

	// Command Complete occurs AFTER a valid status has been
	// sent. then we go bus-free.
	enter_MessageIn(MSG_COMMAND_COMPLETE);
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

		// TODO re-implement parity checking
		if (0 && scsiDev.parityError && config->enableParity)
		{
			scsiDev.sense.code = ABORTED_COMMAND;
			scsiDev.sense.asc = SCSI_PARITY_ERROR;
			enter_Status(CHECK_CONDITION);
		}
	}

	if ((scsiDev.dataPtr >= scsiDev.dataLen) &&
		(transfer.currentBlock == transfer.blocks))
	{
		enter_Status(GOOD);
	}
}

static const uint8 CmdGroupBytes[8] = {6, 10, 10, 6, 6, 12, 6, 6};
static void process_Command()
{
	int group;
	int cmdSize;
	uint8 command;
	uint8 lun;

	scsiEnterPhase(COMMAND);
	scsiDev.parityError = 0;

	memset(scsiDev.cdb, 0, sizeof(scsiDev.cdb));
	scsiDev.cdb[0] = scsiReadByte();

	group = scsiDev.cdb[0] >> 5;
	cmdSize = CmdGroupBytes[group];
	scsiRead(scsiDev.cdb + 1, cmdSize - 1);

	command = scsiDev.cdb[0];
	lun = scsiDev.cdb[1] >> 5;

	#ifdef MM_DEBUG
	scsiDev.cmdCount++;
	#endif

	if (scsiDev.resetFlag)
	{
#ifdef MM_DEBUG
		// Don't log bogus commands
		scsiDev.cmdCount--;
		memset(scsiDev.cdb, 0xff, sizeof(scsiDev.cdb));
#endif
		return;
	}
	else if (scsiDev.parityError)
	{
		scsiDev.sense.code = ABORTED_COMMAND;
		scsiDev.sense.asc = SCSI_PARITY_ERROR;
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
		if (allocLength == 0) allocLength = 256;
		memset(scsiDev.data, 0, 18);
		scsiDev.data[0] = 0xF0;
		scsiDev.data[2] = scsiDev.sense.code & 0x0F;

		scsiDev.data[3] = transfer.lba >> 24;
		scsiDev.data[4] = transfer.lba >> 16;
		scsiDev.data[5] = transfer.lba >> 8;
		scsiDev.data[6] = transfer.lba;

		// Additional bytes if there are errors to report
		int responseLength;
		if (scsiDev.sense.code == NO_SENSE)
		{
			responseLength = 8;
		}
		else
		{
			responseLength = 18;
			scsiDev.data[7] = 10; // additional length
			scsiDev.data[12] = scsiDev.sense.asc >> 8;
			scsiDev.data[13] = scsiDev.sense.asc;
		}

		// Silently truncate results. SCSI-2 spec 8.2.14.
		enter_DataIn(
			(allocLength < responseLength) ? allocLength : responseLength
			);

		// This is a good time to clear out old sense information.
		scsiDev.sense.code = NO_SENSE;
		scsiDev.sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	}
	// Some old SCSI drivers do NOT properly support
	// unitAttention. eg. the Mac Plus would trigger a SCSI reset
	// on receiving the unit attention response on boot, thus
	// triggering another unit attention condition.
	else if (scsiDev.unitAttention && config->enableUnitAttention)
	{
		scsiDev.sense.code = UNIT_ATTENTION;
		scsiDev.sense.asc = scsiDev.unitAttention;

		// If initiator doesn't do REQUEST SENSE for the next command, then
		// data is lost.
		scsiDev.unitAttention = 0;

		enter_Status(CHECK_CONDITION);
	}
	else if (lun)
	{
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = LOGICAL_UNIT_NOT_SUPPORTED;
		enter_Status(CHECK_CONDITION);
	}
	else if (command == 0x17 || command == 0x16)
	{
		doReserveRelease();
	}
	else if ((scsiDev.reservedId >= 0) &&
		(scsiDev.reservedId != scsiDev.initiatorId))
	{
		enter_Status(CONFLICT);
	}
	else if (command == 0x1C)
	{
		scsiReceiveDiagnostic();
	}
	else if (command == 0x1D)
	{
		scsiSendDiagnostic();
	}
	else if (
		!scsiModeCommand() &&
		!scsiDiskCommand())
	{
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = INVALID_COMMAND_OPERATION_CODE;
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
		(!thirdPty && (scsiDev.initiatorId == scsiDev.reservedId)) ||
			(thirdPty &&
				(scsiDev.reserverId == scsiDev.initiatorId) &&
				(scsiDev.reservedId == thirdPtyId)
			);

	if (extentReservation)
	{
		// Not supported.
		scsiDev.sense.code = ILLEGAL_REQUEST;
		scsiDev.sense.asc = INVALID_FIELD_IN_CDB;
		enter_Status(CHECK_CONDITION);
	}
	else if (command == 0x17) // release
	{
		if ((scsiDev.reservedId < 0) || canRelease)
		{
			scsiDev.reservedId = -1;
			scsiDev.reserverId = -1;
		}
		else
		{
			enter_Status(CONFLICT);
		}
	}
	else // assume reserve.
	{
		if ((scsiDev.reservedId < 0) || canRelease)
		{
			scsiDev.reserverId = scsiDev.initiatorId;
			if (thirdPty)
			{
				scsiDev.reservedId = thirdPtyId;
			}
			else
			{
				scsiDev.reservedId = scsiDev.initiatorId;
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
#ifdef MM_DEBUG
	scsiDev.rstCount++;
#endif
	ledOff();

	scsiPhyReset();

	scsiDev.parityError = 0;
	scsiDev.phase = BUS_FREE;
	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 0;

	if (scsiDev.unitAttention != POWER_ON_RESET)
	{
		scsiDev.unitAttention = SCSI_BUS_RESET;
	}
	scsiDev.reservedId = -1;
	scsiDev.reserverId = -1;
	scsiDev.sense.code = NO_SENSE;
	scsiDev.sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	scsiDiskReset();
	
	SCSI_CMD_TIMER_Stop();

	// Sleep to allow the bus to settle down a bit.
	// We must be ready again within the "Reset to selection time" of
	// 250ms.
	// There is no guarantee that the RST line will be negated by then.
	// NOTE: We could be connected and powered by USB for configuration,
	// in which case TERMPWR cannot be supplied, and reset will ALWAYS
	// be true.
	CyDelay(10); // 10ms.
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

	transfer.blocks = 0;
	transfer.currentBlock = 0;
}

static void process_SelectionPhase()
{
	int sel = SCSI_ReadPin(SCSI_In_SEL);
	int bsy = SCSI_ReadPin(SCSI_In_BSY);

	// Only read these pins AFTER SEL and BSY - we don't want to catch them
	// during a transition period.
	uint8 mask = scsiReadDBxPins();
	int maskBitCount = countBits(mask);
	int goodParity = (Lookup_OddParity[mask] == SCSI_ReadPin(SCSI_In_DBP));

	if (!bsy && sel &&
		(mask & scsiDev.scsiIdMask) &&
		(goodParity || !config->enableParity) && (maskBitCount <= 2))
	{
		// Do we enter MESSAGE OUT immediately ? SCSI 1 and 2 standards says
		// move to MESSAGE OUT if ATN is true before we release BSY.
		// The initiate should assert ATN with SEL.
		scsiDev.atnFlag = SCSI_ReadPin(SCSI_ATN_INT);

		// We've been selected!
		// Assert BSY - Selection success!
		// must happen within 200us (Selection abort time) of seeing our
		// ID + SEL.
		// (Note: the initiator will be waiting the "Selection time-out delay"
		// for our BSY response, which is actually a very generous 250ms)
		SCSI_SetPin(SCSI_Out_BSY);
		ledOn();
		
		// Used in enter_BusFree() to ensure each command takes at least 10us.
		// as required by some old SCSI controllers (MPC3000).
		CmdTimerComplete = 0;
		SCSI_CMD_TIMER_Enable();

		#ifdef MM_DEBUG
		scsiDev.selCount++;
		#endif

		// Wait until the end of the selection phase.
		while (!scsiDev.resetFlag)
		{
			if (!SCSI_ReadPin(SCSI_In_SEL))
			{
				break;
			}
		}

		// Save our initiator now that we're no longer in a time-critical
		// section.
		// SCSI1/SASI initiators may not set their own ID.
		if (maskBitCount == 2)
		{
			int i;
			uint8 initiatorMask = mask ^ scsiDev.scsiIdMask;
			scsiDev.initiatorId = 0;
			for (i = 0; i < 8; ++i)
			{
				if (initiatorMask & (1 << i))
				{
					scsiDev.initiatorId = i;
					break;
				}
			}
		}
		else
		{
			scsiDev.initiatorId = -1;
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
#ifdef MM_DEBUG
	scsiDev.msgCount++;
#endif

	if (scsiDev.parityError)
	{
		// Skip the remaining message bytes, and then start the MESSAGE_OUT
		// phase again from the start. The initiator will re-send the
		// same set of messages.
		while (SCSI_ReadPin(SCSI_ATN_INT) && !scsiDev.resetFlag)
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

		scsiDev.unitAttention = SCSI_BUS_RESET;

		// ANY initiator can reset the reservation state via this message.
		scsiDev.reservedId = -1;
		scsiDev.reserverId = -1;
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
		// We don't disconnect, so ignore disconnect privilege.
		if ((scsiDev.msgOut & 0x18) || // Reserved bits set.
			(scsiDev.msgOut & 0x20)  || // We don't have any target routines!
			(scsiDev.msgOut & 0x7) // We only support LUN 0!
			)
		{
			messageReject();
		}
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
		for (i = 0; i < msgLen && !scsiDev.resetFlag; ++i)
		{
			// Discard bytes.
			scsiReadByte();
		}

		// We don't support ANY extended messages.
		// Modify Data Pointer:  We don't support reselection.
		// Wide Data Transfer Request: No. 8bit only.
		// Synchronous data transfer request. No, we can't do that.
		// We don't support any 2-byte messages either.
		// And we don't support any optional 1-byte messages.
		// In each case, the correct response is MESSAGE REJECT.
		messageReject();
	}
	else
	{
		messageReject();
	}

	// Re-check the ATN flag in case it stays asserted.
	scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
}

void scsiPoll(void)
{
	if (scsiDev.resetFlag)
	{
		scsiReset();
		if ((scsiDev.resetFlag = SCSI_ReadPin(SCSI_RST_INT)))
		{
			// Still in reset phase. Do not try and process any commands.
			return;
		}
	}

	switch (scsiDev.phase)
	{
	case BUS_FREE:
		if (SCSI_ReadPin(SCSI_In_BSY))
		{
			scsiDev.phase = BUS_BUSY;
		}
		// The Arbitration phase is optional for SCSI1/SASI hosts if there is only
		// one initiator in the chain. Support this by moving
		// straight to selection if SEL is asserted.
		// ie. the initiator won't assert BSY and it's own ID before moving to selection.
		else if (SCSI_ReadPin(SCSI_In_SEL))
		{
			enter_SelectionPhase();
		}
	break;

	case BUS_BUSY:
		// Someone is using the bus. Perhaps they are trying to
		// select us.
		if (SCSI_ReadPin(SCSI_In_SEL))
		{
			enter_SelectionPhase();
		}
		else if (!SCSI_ReadPin(SCSI_In_BSY))
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
		scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
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
		scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
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
		scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
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
		scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
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
	SCSI_CMD_TIMER_Init(); // config but don't start the timeout counter
    SCSI_CMD_TIMER_ISR_StartEx(CommandTimerISR); // setup timer interrupt sub-routine
	
	scsiDev.scsiIdMask = 1 << (config->scsiId);

	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 1;
	scsiDev.phase = BUS_FREE;
	scsiDev.reservedId = -1;
	scsiDev.reserverId = -1;
	scsiDev.unitAttention = POWER_ON_RESET;
}

