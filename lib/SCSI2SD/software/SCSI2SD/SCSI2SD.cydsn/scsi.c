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

static void enter_SelectionPhase();
static void process_SelectionPhase();
static void enter_BusFree();
static void enter_MessageIn(uint8 message);
static void process_MessageIn();
static void enter_Status(uint8 status);
static void process_Status();
static void enter_DataIn(int len);
static void process_DataIn();
static void process_DataOut();
static void process_Command();

static void doReserveRelease();

static void enter_BusFree()
{
	scsiEnterPhase(BUS_FREE);

	ledOff();

	scsiDev.phase = BUS_FREE;
	SCSI_ClearPin(SCSI_Out_BSY);
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
	else if (scsiDev.msgIn == MSG_COMMAND_COMPLETE)
	{
		enter_BusFree();
	}
	else
	{
		// MESSAGE_REJECT. Go back to command phase
		// TODO MESSAGE_REJECT moved to messageReject method.
		scsiDev.phase = COMMAND;
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
}

static void process_Status()
{
	scsiEnterPhase(STATUS);
	scsiWriteByte(scsiDev.status);

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
	if (scsiDev.dataLen > sizeof(scsiDev.data))
	{
		scsiDev.dataLen = sizeof(scsiDev.data);
	}

	scsiEnterPhase(DATA_IN);

	uint32 len = scsiDev.dataLen - scsiDev.dataPtr;
	scsiWrite(scsiDev.data + scsiDev.dataPtr, len);
	scsiDev.dataPtr += len;


	if ((scsiDev.dataPtr >= scsiDev.dataLen) &&
		(transfer.currentBlock == transfer.blocks))
	{
		enter_Status(GOOD);
	}
}

static void process_DataOut()
{
	if (scsiDev.dataLen > sizeof(scsiDev.data))
	{
		scsiDev.dataLen = sizeof(scsiDev.data);
	}

	scsiEnterPhase(DATA_OUT);

	scsiDev.parityError = 0;
	uint32 len = scsiDev.dataLen - scsiDev.dataPtr;
	scsiRead(scsiDev.data + scsiDev.dataPtr, len);
	scsiDev.dataPtr += len;

	// TODO re-implement parity checking
	if (0 && scsiDev.parityError && config->enableParity)
	{
		scsiDev.sense.code = ABORTED_COMMAND;
		scsiDev.sense.asc = SCSI_PARITY_ERROR;
		enter_Status(CHECK_CONDITION);
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
	scsiEnterPhase(COMMAND);
	scsiDev.parityError = 0;

	memset(scsiDev.cdb, 0, sizeof(scsiDev.cdb));
	scsiDev.cdb[0] = scsiReadByte();

	int group = scsiDev.cdb[0] >> 5;
	int cmdSize = CmdGroupBytes[group];
	scsiRead(scsiDev.cdb + 1, cmdSize - 1);

	uint8 command = scsiDev.cdb[0];
	uint8 lun = scsiDev.cdb[1] >> 5;

	if (scsiDev.parityError)
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

		// TODO populate "information" field with requested LBA.
		// TODO support more detailed sense data ?

		scsiDev.data[12] = scsiDev.sense.asc >> 8;
		scsiDev.data[13] = scsiDev.sense.asc;

		// Silently truncate results. SCSI-2 spec 8.2.14.
		enter_DataIn(allocLength < 18 ? allocLength : 18);

		// This is a good time to clear out old sense information.
		scsiDev.sense.code = NO_SENSE;
		scsiDev.sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	}
	// Some old SCSI drivers do NOT properly support
	// unitAttention. OTOH, Linux seems to require it
	// confirmed LCIII with unknown scsi driver fials here.
	else if (scsiDev.unitAttention && config->enableUnitAttention)
	{
		scsiDev.sense.code = UNIT_ATTENTION;
		scsiDev.sense.asc = scsiDev.unitAttention;
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
	ledOff();
	// done in verilog SCSI_Out_DBx_Write(0);
	SCSI_CTL_IO_Write(0);
	SCSI_ClearPin(SCSI_Out_ATN);
	SCSI_ClearPin(SCSI_Out_BSY);
	SCSI_ClearPin(SCSI_Out_ACK);
	SCSI_ClearPin(SCSI_Out_RST);
	SCSI_ClearPin(SCSI_Out_SEL);
	SCSI_ClearPin(SCSI_Out_REQ);
	SCSI_ClearPin(SCSI_Out_MSG);
	SCSI_ClearPin(SCSI_Out_CD);

	scsiDev.parityError = 0;
	scsiDev.phase = BUS_FREE;

	if (scsiDev.unitAttention != POWER_ON_RESET)
	{
		scsiDev.unitAttention = SCSI_BUS_RESET;
	}
	scsiDev.reservedId = -1;
	scsiDev.reserverId = -1;
	scsiDev.sense.code = NO_SENSE;
	scsiDev.sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
	scsiDiskReset();

	// Sleep to allow the bus to settle down a bit.
	// We must be ready again within the "Reset to selection time" of
	// 250ms.
	// There is no guarantee that the RST line will be negated by then.
	int reset;
	do
	{
		CyDelay(10); // 10ms.
		reset = SCSI_ReadPin(SCSI_RST_INT);
	} while (reset);

	scsiDev.resetFlag = 0;
	scsiDev.atnFlag = 0;
}

static void enter_SelectionPhase()
{
	// Ignore stale versions of this flag, but ensure we know the
	// current value if the flag is still set.
	scsiDev.atnFlag = SCSI_ReadPin(SCSI_ATN_INT);
	scsiDev.parityError = 0;
	scsiDev.dataPtr = 0;
	scsiDev.savedDataPtr = 0;
	scsiDev.status = GOOD;
	scsiDev.phase = SELECTION;
}

static void process_SelectionPhase()
{
	uint8 mask = scsiReadDBxPins();
	int goodParity = (Lookup_OddParity[mask] == SCSI_ReadPin(SCSI_In_DBP));

	int sel = SCSI_ReadPin(SCSI_In_SEL);
	int bsy = SCSI_ReadPin(SCSI_In_BSY);
	if (!bsy && sel &&
		(mask & scsiDev.scsiIdMask) &&
		(goodParity || !config->enableParity) && (countBits(mask) == 2))
	{
		// We've been selected!
		// Assert BSY - Selection success!
		// must happen within 200us (Selection abort time) of seeing our
		// ID + SEL.
		// (Note: the initiator will be waiting the "Selection time-out delay"
		// for our BSY response, which is actually a very generous 250ms)
		SCSI_SetPin(SCSI_Out_BSY);
		ledOn();

		// Wait until the end of the selection phase.
		while (!scsiDev.resetFlag)
		{
			scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
			if (!SCSI_ReadPin(SCSI_In_SEL))
			{
				break;
			}
		}

		// Save our initiator now that we're no longer in a time-critical
		// section.
		uint8 initiatorMask = mask ^ scsiDev.scsiIdMask;
		scsiDev.initiatorId = 0;
		int i;
		for (i = 0; i < 8; ++i)
		{
			if (initiatorMask & (1 << i))
			{
				scsiDev.initiatorId = i;
				break;
			}
		}

		scsiDev.phase = COMMAND;
		
		CyDelayUs(2); // DODGY HACK
		scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
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
			enter_MessageIn(MSG_REJECT);
		}
	}
	else if (scsiDev.msgOut >= 0x20 && scsiDev.msgOut <= 0x2F)
	{
		// Two byte message. We don't support these. read and discard.
		scsiReadByte();
	}
	else if (scsiDev.msgOut == 0x01)
	{
		// Extended message.
		int msgLen = scsiReadByte();
		if (msgLen == 0) msgLen = 256;
		int i;
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
	
	// Re-check the ATN flag. We won't get another interrupt if
	// it stays asserted.
	CyDelayUs(2); // DODGY HACK
	scsiDev.atnFlag |= SCSI_ReadPin(SCSI_ATN_INT);
}


// TODO remove.
// This is a hack until I work out why the ATN ISR isn't
// running when it should.
static int atnErrCount = 0;
static int atnHitCount = 0;
static void checkATN()
{
	int atn = SCSI_ReadPin(SCSI_ATN_INT);
	if (atn && !scsiDev.atnFlag)
	{
		atnErrCount++;
		scsiDev.atnFlag = 1;
	}
	else if (atn && scsiDev.atnFlag)
	{
		atnHitCount++;
	}
}

void scsiPoll(void)
{
	if (scsiDev.resetFlag)
	{
		scsiReset();
	}

	switch (scsiDev.phase)
	{
	case BUS_FREE:
		if (SCSI_ReadPin(SCSI_In_BSY))
		{
			scsiDev.phase = BUS_BUSY;
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
		checkATN();
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
		checkATN();
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
		checkATN();
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
		checkATN();
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
		checkATN();
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
	scsiDev.scsiIdMask = 1 << (config->scsiId);

	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 1;
	scsiDev.phase = BUS_FREE;
	scsiDev.reservedId = -1;
	scsiDev.reserverId = -1;
	scsiDev.unitAttention = POWER_ON_RESET;
}

