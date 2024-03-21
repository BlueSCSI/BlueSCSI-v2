//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//	Copyright (c) 2023 joshua stein <jcs@jcs.org>
//	Copyright (c) 2023 Andrea Ottaviani <andrea.ottaviani.69@gmail.com>
//	Copyright (C) 2024 Rabbit Hole Computing LLC
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


#include "scsi.h"
#include "scsiPhy.h"
#include "config.h"
#include "diagnostic.h"
#include "disk.h"
#include "inquiry.h"
#include "led.h"
#include "mode.h"
#include "scsi2sd_time.h"
#include "bsp.h"
#include "cdrom.h"
#include "network.h"
#include "tape.h"
#include "mo.h"
#include "vendor.h"
#include <string.h>
#include "toolbox.h"

// Global SCSI device state.
ScsiDevice scsiDev S2S_DMA_ALIGN;

static void enter_SelectionPhase(void);
static void process_SelectionPhase(void);
static void enter_MessageIn(uint8_t message);
static void enter_Status(uint8_t status);
static void enter_DataIn(int len);
static void process_DataIn(void);
static void process_DataOut(void);
static void process_Command(void);

static void doReserveRelease(void);

void enter_BusFree()
{
	// This delay probably isn't needed for most SCSI hosts, but it won't
	// hurt either. It's possible some of the samplers needed this delay.
	if (scsiDev.compatMode < COMPAT_SCSI2)
	{
		s2s_delay_us(2);
	}

#if 0
	if (scsiDev.status != GOOD)// && isDebugEnabled())
	{
		// We want to capture debug information for failure cases.
		s2s_delay_ms(80);
	}
#endif


	scsiEnterBusFree();

	// Wait for the initiator to cease driving signals
	// Bus settle delay + bus clear delay = 1200ns
	// Just waiting the clear delay is sufficient.
	s2s_delay_ns(800);

	s2s_ledOff();
	scsiDev.phase = BUS_FREE;
	scsiDev.selFlag = 0;
}

static void enter_MessageIn(uint8_t message)
{
	scsiDev.msgIn = message;
	scsiDev.phase = MESSAGE_IN;
}

int process_MessageIn(int releaseBusFree)
{
	scsiEnterPhase(MESSAGE_IN);
	scsiWriteByte(scsiDev.msgIn);

	if (unlikely(scsiDev.atnFlag))
	{
		// If there was a parity error, we go
		// back to MESSAGE_OUT first, get out parity error message, then come
		// back here.
		return 0;
	}
	else if ((scsiDev.msgIn == MSG_LINKED_COMMAND_COMPLETE) ||
		(scsiDev.msgIn == MSG_LINKED_COMMAND_COMPLETE_WITH_FLAG))
	{
		// Go back to the command phase and start again.
		scsiDev.phase = COMMAND;
		scsiDev.dataPtr = 0;
		scsiDev.savedDataPtr = 0;
		scsiDev.dataLen = 0;
		scsiDev.status = GOOD;
		transfer.blocks = 0;
		transfer.currentBlock = 0;
		return 0;
	}
	else if (releaseBusFree) /*if (scsiDev.msgIn == MSG_COMMAND_COMPLETE)*/
	{
		enter_BusFree();
		return 1;
	}
	else
	{
		return 1;
	}
}

static void messageReject()
{
	scsiEnterPhase(MESSAGE_IN);
	scsiWriteByte(MSG_REJECT);
}

static void enter_Status(uint8_t status)
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

	if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_EWSD)
	{
		s2s_delay_ms(1);
	}

	uint8_t message;

	uint8_t control = scsiDev.cdb[scsiDev.cdbLen - 1];

	if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_OMTI)
	{
		// All commands have a control byte, except 0xC0
		if (scsiDev.cdb[0] == 0xC0)
		{
			control = 0;
		}

		// OMTI non-standard LINK control
		if (control & 0x01)
		{
			scsiDev.phase = COMMAND;
			return;
		}
	}

	if ((scsiDev.status == GOOD) && (control & 0x01) &&
		scsiDev.target->cfg->quirks != S2S_CFG_QUIRKS_XEBEC)
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

	if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		// More non-standardness. Expects 2 status bytes (really status + msg)
		// 00 d 000 err 0
		// d == disk number
		// ERR = 1 if error.
		if (scsiDev.status == GOOD)
		{
			scsiWriteByte(scsiDev.cdb[1] & 0x20);
		}
		else
		{
			scsiWriteByte((scsiDev.cdb[1] & 0x20) | 0x2);
		}
		s2s_delay_us(10); // Seems to need a delay before changing phase bits.
	}
	else if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_OMTI)
	{
		scsiDev.status |= (scsiDev.target->targetId & 0x03) << 5;
		scsiWriteByte(scsiDev.status);
	}
	else
	{
		scsiWriteByte(scsiDev.status);
	}

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
	uint32_t len;

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
	uint32_t len;

	if (scsiDev.dataLen > sizeof(scsiDev.data))
	{
		scsiDev.dataLen = sizeof(scsiDev.data);
	}

	len = scsiDev.dataLen - scsiDev.dataPtr;
	if (len > 0)
	{
		scsiEnterPhase(DATA_OUT);

		int parityError = 0;
		scsiRead(scsiDev.data + scsiDev.dataPtr, len, &parityError);
		scsiDev.dataPtr += len;

		if (parityError &&
			(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY))
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

static const uint8_t CmdGroupBytes[8] = {6, 10, 10, 6, 16, 12, 6, 6};
static void process_Command()
{
	int group;
	uint8_t command;
	uint8_t control;

	scsiEnterPhase(COMMAND);

	memset(scsiDev.cdb + 6, 0, sizeof(scsiDev.cdb) - 6);
	int parityError = 0;
	scsiRead(scsiDev.cdb, 6, &parityError);
	command = scsiDev.cdb[0];

	group = scsiDev.cdb[0] >> 5;
	scsiDev.cdbLen = CmdGroupBytes[group];
	scsiVendorCommandSetLen(scsiDev.cdb[0], &scsiDev.cdbLen);
	
	if (parityError &&
		(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY))
	{
		// Don't try and read more bytes, as we cannot be sure what group
		// the command should be.
	}
	else if (scsiDev.cdbLen - 6 > 0)
	{
		scsiRead(scsiDev.cdb + 6, scsiDev.cdbLen - 6, &parityError);
	}


	// Prefer LUN's set by IDENTIFY messages for newer hosts.
	if (scsiDev.lun < 0)
	{
		if (command == 0xE0 || command == 0xE4) // XEBEC s1410
		{
			scsiDev.lun = 0;
		}
		else
		{
			scsiDev.lun = scsiDev.cdb[1] >> 5;
		}
	}


	// For Philips P2000C with Xebec S1410 SASI/MFM adapter
	// http://bitsavers.trailing-edge.com/pdf/xebec/104524C_S1410Man_Aug83.pdf
	if ((scsiDev.lun > 0) && (scsiDev.boardCfg.flags & S2S_CFG_MAP_LUNS_TO_IDS))
	{
		int tgtIndex;
		for (tgtIndex = 0; tgtIndex < S2S_MAX_TARGETS; ++tgtIndex)
		{
			if (scsiDev.targets[tgtIndex].targetId == scsiDev.lun)
			{
				scsiDev.target = &scsiDev.targets[tgtIndex];
				scsiDev.lun = 0;
				break;
			}
		}
	}

	control = scsiDev.cdb[scsiDev.cdbLen - 1];

	scsiDev.cmdCount++;
	const S2S_TargetCfg* cfg = scsiDev.target->cfg;

	if (unlikely(scsiDev.resetFlag))
	{
		// Don't log bogus commands
		scsiDev.cmdCount--;
		memset(scsiDev.cdb, 0xff, sizeof(scsiDev.cdb));
		return;
	}
	// X68000 and strange "0x00 0xXX .. .. .. .." command
	else if ((command == 0x00) && likely(scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_X68000))
	{
		if (scsiDev.cdb[1] == 0x28)
		{
			scsiDev.target->sense.code = NO_SENSE;
			scsiDev.target->sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
			enter_Status(CHECK_CONDITION);
			return;
		} 	else if (scsiDev.cdb[1] == 0x03)
		{
			scsiDev.target->sense.code = NO_SENSE;
			scsiDev.target->sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;
			enter_Status(GOOD);
			return;
		}
	}
	else if (parityError &&
		(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY))
	{
		scsiDev.target->sense.code = ABORTED_COMMAND;
		scsiDev.target->sense.asc = SCSI_PARITY_ERROR;
		enter_Status(CHECK_CONDITION);
	}
	else if ((control & 0x02) && ((control & 0x01) == 0) &&
		// used for head step options on xebec.
		likely(scsiDev.target->cfg->quirks != S2S_CFG_QUIRKS_XEBEC))
	{
		// FLAG set without LINK flag.
		scsiDev.target->sense.code = ILLEGAL_REQUEST;
		scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
		enter_Status(CHECK_CONDITION);
	}
	else if (command == 0x12)
	{
		s2s_scsiInquiry();
	}
	else if (command == 0x03)
	{
		// REQUEST SENSE
		uint32_t allocLength = scsiDev.cdb[4];

		if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
		{
			// Completely non-standard
			allocLength = 4;

			switch (scsiDev.target->sense.code)
			{
				case NO_SENSE:
					scsiDev.data[0] = 0;
					break;
				case MEDIUM_ERROR:
					switch (scsiDev.target->sense.asc)
					{
						case NO_SEEK_COMPLETE:
							scsiDev.data[0] = 0x15; // Seek Error
							break;
						case WRITE_ERROR_AUTO_REALLOCATION_FAILED:
							scsiDev.data[0] = 0x03; // Write fault
							break;
						default:
						case UNRECOVERED_READ_ERROR:
							scsiDev.data[0] = 0x11; // Uncorrectable read error
							break;
					}
					break;
				case ILLEGAL_REQUEST:
					switch (scsiDev.target->sense.asc)
					{
						case LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE:
							scsiDev.data[0] = 0x14; // Target sector not found
							break;
						case WRITE_PROTECTED:
							scsiDev.data[0] = 0x03; // Write fault
							break;
						default:
							scsiDev.data[0] = 0x20; // Invalid command
							break;
					}
					break;
				case NOT_READY:
					switch (scsiDev.target->sense.asc)
					{
						default:
						case MEDIUM_NOT_PRESENT:
							scsiDev.data[0] = 0x04; // Drive not ready
							break;
						case LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED:
							scsiDev.data[0] = 0x1A; // Format Error
							break;
					}
					break;
				default:
					scsiDev.data[0] = 0x11;  // Uncorrectable data error
					break;
			}

			scsiDev.data[1] = (scsiDev.cdb[1] & 0x20) | ((transfer.lba >> 16) & 0x1F);
			scsiDev.data[2] = transfer.lba >> 8;
			scsiDev.data[3] = transfer.lba;
		}
		else if (cfg->quirks == S2S_CFG_QUIRKS_OMTI)
		{
			// The response is completely non-standard.
			if (likely(allocLength > 12))
				allocLength = 12;
			else if (unlikely(allocLength < 4))
				allocLength = 4;
			if (cfg->deviceType != S2S_CFG_SEQUENTIAL)
				allocLength = 4;
			memset(scsiDev.data, 0, allocLength);
			if (scsiDev.target->sense.code == NO_SENSE)
			{
				// Nothing to report.
			}
			else if (scsiDev.target->sense.code == UNIT_ATTENTION &&
				cfg->deviceType == S2S_CFG_SEQUENTIAL)
			{
				scsiDev.data[0] = 0x10; // Tape exception
			}
			else if (scsiDev.target->sense.code == ILLEGAL_REQUEST)
			{
				if (scsiDev.target->sense.asc == LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE)
				{
					if (cfg->deviceType == S2S_CFG_SEQUENTIAL)
						scsiDev.data[0] = 0x10; // Tape exception
					else
						scsiDev.data[0] = 0x21; // Illegal Parameters
				}
				else if (scsiDev.target->sense.asc == INVALID_COMMAND_OPERATION_CODE)
				{
					scsiDev.data[0] = 0x20; // Invalid Command
				}
			}
			else if (scsiDev.target->sense.code == NOT_READY)
			{
				scsiDev.data[0] = 0x04; // Drive not ready
			}
			else if (scsiDev.target->sense.code == BLANK_CHECK)
			{
				scsiDev.data[0] = 0x10; // Tape exception
			}
			else
			{
				scsiDev.data[0] = 0x11; // Uncorrectable data error
			}
			scsiDev.data[1] = (scsiDev.cdb[1] & 0x60) | ((transfer.lba >> 16) & 0x1F);
			scsiDev.data[2] = transfer.lba >> 8;
			scsiDev.data[3] = transfer.lba;
			if (cfg->deviceType == S2S_CFG_SEQUENTIAL)
			{
				// For the tape drive there are 8 extra sense bytes.
				if (scsiDev.target->sense.code == BLANK_CHECK)
					scsiDev.data[11] = 0x88; // End of data recorded on the tape
				else if (scsiDev.target->sense.code == UNIT_ATTENTION)
					scsiDev.data[5] = 0x81; // Power On Reset occurred
				else if (scsiDev.target->sense.code == ILLEGAL_REQUEST &&
					 scsiDev.target->sense.asc == LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE)
					scsiDev.data[4] = 0x81; // File Mark detected
			}
		}
		else
		{
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
			if ((scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_EWSD))
			{
				/* EWSD seems not to want something behind additional length. (8 + 0x0e = 22) */
				allocLength=22;
				scsiDev.data[7] = 0x0e;
			}
		}

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
		scsiDev.target->unitAttentionStop == 0 &&
		((scsiDev.boardCfg.flags & S2S_CFG_ENABLE_UNIT_ATTENTION) ||
		(scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_EWSD)))
	{
		/* EWSD requires unitAttention to be sent only once. */
		if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_EWSD)
		{
			scsiDev.target->unitAttentionStop = 1;
		}
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
	// Handle odd device types first that may override basic read and
	// write commands. Will fall-through to generic disk handling.
	else if (((cfg->deviceType == S2S_CFG_OPTICAL) && scsiCDRomCommand()) ||
		((cfg->deviceType == S2S_CFG_SEQUENTIAL) && scsiTapeCommand()) ||
#ifdef ZULUSCSI_NETWORK
		((cfg->deviceType == S2S_CFG_NETWORK && scsiNetworkCommand())) ||
#endif // ZULUSCSI_NETWORK
		((cfg->deviceType == S2S_CFG_MO) && scsiMOCommand()))
	{
		// Already handled.
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
	else if (scsiModeCommand())
	{
		// handled
	}
	else if (scsiVendorCommand())
	{
		// handled
	}
	else if (unlikely(command == 0x00))
    {
        // TEST UNIT READY
        doTestUnitReady();
    }
    else if (unlikely(!doTestUnitReady()))
    {
		// This should be last as it can override other commands 
        // Status and sense codes already set by doTestUnitReady
    }
	else
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
	uint8_t command = scsiDev.cdb[0];

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

static uint32_t resetUntil = 0;

static void scsiReset()
{
	scsiDev.rstCount++;
	s2s_ledOff();

	scsiPhyReset();

	scsiDev.phase = BUS_FREE;
	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 0;
	scsiDev.selFlag = 0;
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

	for (int i = 0; i < S2S_MAX_TARGETS; ++i)
	{
		scsiDev.targets[i].syncOffset = 0;
		scsiDev.targets[i].syncPeriod = 0;
	}
	scsiDev.minSyncPeriod = 0;

	scsiDiskReset();

	scsiDev.postDataOutHook = NULL;

	scsiDev.sdUnderrunCount = 0;

	// Sleep to allow the bus to settle down a bit.
	// We must be ready again within the "Reset to selection time" of
	// 250ms.
	// There is no guarantee that the RST line will be negated by then.
	// NOTE: We could be connected and powered by USB for configuration,
	// in which case TERMPWR cannot be supplied, and reset will ALWAYS
	// be true. Therefore, the sleep here must be slow to avoid slowing
	// USB comms
	resetUntil = s2s_getTime_ms() + 2; // At least 1ms.
}

static void enter_SelectionPhase()
{
	// Ignore stale versions of this flag, but ensure we know the
	// current value if the flag is still set.
	scsiDev.atnFlag = 0;
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

	scsiDev.needSyncNegotiationAck = 0;
}

static void process_SelectionPhase()
{
	// Selection delays.
	// Many SCSI1 samplers that use a 5380 chip need a delay of at least 1ms.
	// The Mac Plus boot-time (ie. rom code) selection abort time
	// is < 1ms and must have no delay (standard suggests 250ms abort time)
	// Most newer SCSI2 hosts don't care either way.
	if (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
	{
		s2s_delay_ms(1); // Simply won't work if set to 0.
	}
	else if (scsiDev.boardCfg.selectionDelay == 255) // auto
	{
		if (scsiDev.compatMode < COMPAT_SCSI2)
		{
			s2s_delay_ms(1);
		}
	}
	else if (scsiDev.boardCfg.selectionDelay != 0)
	{
		s2s_delay_ms(scsiDev.boardCfg.selectionDelay);
	}

	uint8_t selStatus = *SCSI_STS_SELECTED;
	if ((selStatus == 0) && (scsiDev.boardCfg.flags & S2S_CFG_ENABLE_SEL_LATCH))
	{
		selStatus = scsiDev.selFlag;
	}

	int tgtIndex;
	TargetState* target = NULL;
	for (tgtIndex = 0; tgtIndex < S2S_MAX_TARGETS; ++tgtIndex)
	{
		if (scsiDev.targets[tgtIndex].targetId == (selStatus & 7))
		{
			target = &scsiDev.targets[tgtIndex];
			break;
		}
	}
	if ((target != NULL) && (selStatus & 0x40))
	{
		// We've been selected!
		// Assert BSY - Selection success!
		// must happen within 200us (Selection abort time) of seeing our
		// ID + SEL.
		// (Note: the initiator will be waiting the "Selection time-out delay"
		// for our BSY response, which is actually a very generous 250ms)
		*SCSI_CTRL_BSY = 1;
		s2s_ledOn();

		scsiDev.target = target;

		// Do we enter MESSAGE OUT immediately ? SCSI 1 and 2 standards says
		// move to MESSAGE OUT if ATN is true before we assert BSY.
		// The initiator should assert ATN with SEL.
		scsiDev.atnFlag = selStatus & 0x80;


		// Unit attention breaks many older SCSI hosts. Disable it completely
		// for SCSI-1 (and older) hosts, regardless of our configured setting.
		// Enable the compatability mode also as many SASI and SCSI1
		// controllers don't generate parity bits.
		if (!scsiDev.atnFlag)
		{
			target->unitAttention = 0;
			scsiDev.compatMode = COMPAT_SCSI1;
		}
		else if (!(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_SCSI2))
		{
			scsiDev.compatMode = COMPAT_SCSI2_DISABLED;
		}
		else
		{
			scsiDev.compatMode = COMPAT_SCSI2;
		}

		scsiDev.selCount++;


		// Save our initiator now that we're no longer in a time-critical
		// section.
		// SCSI1/SASI initiators may not set their own ID.
		scsiDev.initiatorId = (selStatus >> 3) & 0x7;

		// Wait until the end of the selection phase.
		uint32_t selTimerBegin = s2s_getTime_ms();
		while (likely(!scsiDev.resetFlag))
		{
			if (!scsiStatusSEL())
			{
				break;
			}
			else if (s2s_elapsedTime_ms(selTimerBegin) >= 10 &&
				scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_XEBEC)
			{
				// XEBEC hosts may not bother releasing SEL at all until
				// just before the command ends.
				break;
			}
			else if (s2s_elapsedTime_ms(selTimerBegin) >= 250)
			{
				*SCSI_CTRL_BSY = 0;
				scsiDev.resetFlag = 1;
				break;
			}
		}

		scsiDev.phase = COMMAND;
	}
	else if (!selStatus)
	{
		scsiDev.phase = BUS_BUSY;
	}
	scsiDev.selFlag = 0;
}

static void process_MessageOut()
{
	int wasNeedSyncNegotiationAck = scsiDev.needSyncNegotiationAck;
	scsiDev.needSyncNegotiationAck = 0; // Successful on -most- messages.

	scsiEnterPhase(MESSAGE_OUT);

	scsiDev.atnFlag = 0;
	scsiDev.msgOut = scsiReadByte();
	scsiDev.msgCount++;

	if (scsiParityError() &&
		(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY))
	{
		// Skip the remaining message bytes, and then start the MESSAGE_OUT
		// phase again from the start. The initiator will re-send the
		// same set of messages.
		while (scsiStatusATN() && !scsiDev.resetFlag)
		{
			scsiReadByte();
		}

		// Go-back and try the message again.
		scsiDev.atnFlag = 1;
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

		// Cancel any sync negotiation
		scsiDev.target->syncOffset = 0;
		scsiDev.target->syncPeriod = 0;

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

		if (wasNeedSyncNegotiationAck)
		{
			scsiDev.target->syncOffset = 0;
			scsiDev.target->syncPeriod = 0;
		}
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

		if (wasNeedSyncNegotiationAck)
		{
			scsiDev.target->syncOffset = 0;
			scsiDev.target->syncPeriod = 0;
		}
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

		if (scsiDev.msgOut == 0x23) {
			// Ignore Wide Residue. We're only 8 bit anyway.
		} else {
			messageReject();
		}
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

			// SDTR becomes invalidated.
			scsiDev.target->syncOffset = 0;
			scsiDev.target->syncPeriod = 0;
		}
		else if (extmsg[0] == 1 && msgLen == 3) // Synchronous data request
		{
			int oldPeriod = scsiDev.target->syncPeriod;
			int oldOffset = scsiDev.target->syncOffset;

			int transferPeriod = extmsg[1];
			int offset = extmsg[2];

			if ((
					(transferPeriod > 0) &&
					(transferPeriod < scsiDev.minSyncPeriod)) ||
				(scsiDev.minSyncPeriod == 0))
			{
				scsiDev.minSyncPeriod = transferPeriod;
			}

			if ((transferPeriod > 80) || // 320ns, 3.125MB/s
				// Amiga A590 (WD33C93 chip) only does 3.5MB/s sync
				// After 80 we start to run out of bits in the fpga timing
				// register.
				(transferPeriod == 0) ||
				(offset == 0) ||
				((scsiDev.boardCfg.scsiSpeed != S2S_CFG_SPEED_NoLimit) &&
					(scsiDev.boardCfg.scsiSpeed <= S2S_CFG_SPEED_ASYNC_50)))
			{
				scsiDev.target->syncOffset = 0;
				scsiDev.target->syncPeriod = 0;
			} else {
				scsiDev.target->syncOffset = offset <= 15 ? offset : 15;
				// FAST20 / 50ns / 20MHz is disabled for now due to
				// data corruption while reading data. We can count the
				// ACK's correctly, but can't save the data to a register
				// before it changes. (ie. transferPeriod == 12)
				if ((scsiDev.boardCfg.scsiSpeed == S2S_CFG_SPEED_TURBO) &&
					(transferPeriod <= 16))
				{
					scsiDev.target->syncPeriod = 16; // 15.6MB/s
				}
				else if (scsiDev.boardCfg.scsiSpeed == S2S_CFG_SPEED_TURBO)
				{
					scsiDev.target->syncPeriod = transferPeriod;
				}
				else if (transferPeriod <= 25 &&
					((scsiDev.boardCfg.scsiSpeed == S2S_CFG_SPEED_NoLimit) ||
						(scsiDev.boardCfg.scsiSpeed >= S2S_CFG_SPEED_SYNC_10)))
				{
					scsiDev.target->syncPeriod = 25; // 100ns, 10MB/s

				} else if (transferPeriod < 50 &&
					((scsiDev.boardCfg.scsiSpeed == S2S_CFG_SPEED_NoLimit) ||
						(scsiDev.boardCfg.scsiSpeed >= S2S_CFG_SPEED_SYNC_10)))
				{
					scsiDev.target->syncPeriod = transferPeriod;
				} else if (transferPeriod >= 50)
				{
					scsiDev.target->syncPeriod = transferPeriod;
				} else {
					scsiDev.target->syncPeriod = 50;
				}
			}

			if (transferPeriod != oldPeriod ||
				scsiDev.target->syncPeriod != oldPeriod ||
				offset != oldOffset ||
				scsiDev.target->syncOffset != oldOffset ||
				!wasNeedSyncNegotiationAck) // Don't get into infinite loops negotiating.
			{
				scsiEnterPhase(MESSAGE_IN);
				uint8_t SDTR[] = {0x01, 0x03, 0x01, scsiDev.target->syncPeriod, scsiDev.target->syncOffset};
				scsiWrite(SDTR, sizeof(SDTR));
				scsiDev.needSyncNegotiationAck = 1; // Check if this message is rejected.
				scsiDev.sdUnderrunCount = 0;  // reset counter, may work now.

				// Set to the theoretical speed, then adjust if we measure lower
				// actual speeds.
				scsiDev.hostSpeedKBs = s2s_getScsiRateKBs();
				scsiDev.hostSpeedMeasured = 0;
			}
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
	scsiDev.atnFlag |= scsiStatusATN();

	if (!scsiDev.atnFlag)
	{
		// Message wasn't rejected!
		scsiDev.needSyncNegotiationAck = 0;
	}
}

void scsiPoll(void)
{
	if (resetUntil != 0 && resetUntil > s2s_getTime_ms())
	{
		return;
	}
	resetUntil = 0;

	if (unlikely(scsiDev.resetFlag))
	{
		scsiReset();
		// Still in reset phase for a few ms.
		// Do not try and process any commands.
		return;
	}

	switch (scsiDev.phase)
	{
	case BUS_FREE:
		if (scsiStatusBSY())
		{
			scsiDev.phase = BUS_BUSY;
		}
		// The Arbitration phase is optional for SCSI1/SASI hosts if there is only
		// one initiator in the chain. Support this by moving
		// straight to selection if SEL is asserted.
		// ie. the initiator won't assert BSY and it's own ID before moving to selection.
		else if (scsiDev.selFlag || *SCSI_STS_SELECTED)
		{
			enter_SelectionPhase();
		}
	break;

	case BUS_BUSY:
		// Someone is using the bus. Perhaps they are trying to
		// select us.
		if (scsiDev.selFlag || *SCSI_STS_SELECTED)
		{
			enter_SelectionPhase();
		}
		else if (!scsiStatusBSY())
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
		scsiDev.atnFlag |= scsiStatusATN();
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
		scsiDev.atnFlag |= scsiStatusATN();
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
		scsiDev.atnFlag |= scsiStatusATN();
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
		scsiDev.atnFlag |= scsiStatusATN();
		if (scsiDev.atnFlag)
		{
			process_MessageOut();
		}
		else
		{
			process_MessageIn(1);
		}

	break;

	case MESSAGE_OUT:
		process_MessageOut();
	break;
	}
}

void scsiInit()
{
	static int firstInit = 1;

	scsiDev.atnFlag = 0;
	scsiDev.resetFlag = 1;
	scsiDev.selFlag = 0;
	scsiDev.phase = BUS_FREE;
	scsiDev.target = NULL;
	scsiDev.compatMode = COMPAT_UNKNOWN;
	scsiDev.hostSpeedKBs = 0;
	scsiDev.hostSpeedMeasured = 0;

	int i;
	for (i = 0; i < S2S_MAX_TARGETS; ++i)
	{
		const S2S_TargetCfg* cfg = s2s_getConfigByIndex(i);
		if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
		{
			scsiDev.targets[i].targetId = cfg->scsiId & S2S_CFG_TARGET_ID_BITS;
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
		if (firstInit)
		{
			if ((cfg->deviceType == S2S_CFG_MO) && (scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_EWSD))
			{
				scsiDev.targets[i].unitAttention = POWER_ON_RESET_OR_BUS_DEVICE_RESET_OCCURRED;
			} else
			{
				scsiDev.targets[i].unitAttention = POWER_ON_RESET;
			}
		}
		else
		{
			scsiDev.targets[i].unitAttention = PARAMETERS_CHANGED;
		}
		scsiDev.targets[i].sense.code = NO_SENSE;
		scsiDev.targets[i].sense.asc = NO_ADDITIONAL_SENSE_INFORMATION;

		scsiDev.targets[i].syncOffset = 0;
		scsiDev.targets[i].syncPeriod = 0;

		// Always "start" the device. Many systems (eg. Apple System 7)
		// won't respond properly to
		// LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED sense
		// code
		scsiDev.targets[i].started = 1;
	}
	firstInit = 0;
}

/* TODO REENABLE
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
*/

/* TODO REENABLE
int scsiReconnect()
{
	int reconnected = 0;

	int sel = SCSI_ReadFilt(SCSI_Filt_SEL);
	int bsy = SCSI_ReadFilt(SCSI_Filt_BSY);
	if (!sel && !bsy)
	{
		s2s_delay_us(1);
		sel = SCSI_ReadFilt(SCSI_Filt_SEL);
		bsy = SCSI_ReadFilt(SCSI_Filt_BSY);
	}

	if (!sel && !bsy)
	{
		// Arbitrate.
		s2s_ledOn();
		uint8_t scsiIdMask = 1 << scsiDev.target->targetId;
		SCSI_Out_Bits_Write(scsiIdMask);
		SCSI_Out_Ctl_Write(1); // Write bits manually.
		SCSI_SetPin(SCSI_Out_BSY);

		s2s_delay_us(3); // arbitrate delay. 2.4us.

		uint8_t dbx = scsiReadDBxPins();
		sel = SCSI_ReadFilt(SCSI_Filt_SEL);
		if (sel || ((dbx ^ scsiIdMask) > scsiIdMask))
		{
			// Lost arbitration.
			SCSI_Out_Ctl_Write(0);
			SCSI_ClearPin(SCSI_Out_BSY);
			s2s_ledOff();
		}
		else
		{
			// Won arbitration
			SCSI_SetPin(SCSI_Out_SEL);
			s2s_delay_us(1); // Bus clear + Bus settle.

			// Reselection phase
			SCSI_CTL_PHASE_Write(__scsiphase_io);
			SCSI_Out_Bits_Write(scsiIdMask | (1 << scsiDev.initiatorId));
			scsiDeskewDelay(); // 2 deskew delays
			scsiDeskewDelay(); // 2 deskew delays
			SCSI_ClearPin(SCSI_Out_BSY);
			s2s_delay_us(1);  // Bus Settle Delay

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
				s2s_ledOff();
			}
		}
	}
	return reconnected;
}
*/
