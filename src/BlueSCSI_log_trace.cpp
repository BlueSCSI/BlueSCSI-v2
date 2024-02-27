// SCSI trace logging
//
// Copyright (c) 2022 Rabbit Hole Computing™
// Copyright (c) 2023 Eric Helgeson

#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_log.h"
#include <scsi2sd.h>
#include "BlueSCSI_Toolbox.h"

extern "C" {
#include <scsi.h>
#include <scsiPhy.h>
}

static bool g_LogData = false;
static bool g_LogInitiatorCommand = false;
static int g_InByteCount = 0;
static int g_OutByteCount = 0;
static uint16_t g_DataChecksum = 0;

static const char *getCommandName(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x00: return "TestUnitReady";
        case 0x01: return "RezeroUnit/Rewind";
        case 0x03: return "RequestSense";
        case 0x04: return "FormatUnit";
        case 0x05: return "ReadBlockLimits";
        case 0x08: return "Read6";
        case 0x0A: return "Write6";
        case 0x0B: return "Seek6";
        case 0x0C: return "Xebec InitializeDriveCharacteristics";
        case 0x0F: return "Xebec WriteSectorBuffer";
        case 0x10: return "WriteFilemarks";
        case 0x11: return "Space";
        case 0x12: return "Inquiry";
        case 0x13: return "Verify";
        case 0x15: return "ModeSelect6";
        case 0x16: return "Reserve";
        case 0x17: return "Release";
        case 0x19: return "Erase";
        case 0x1A: return "ModeSense";
        case 0x1B: return "StartStopUnit";
        case 0x1C: return "ReceiveDiagnostic";
        case 0x1D: return "SendDiagnostic";
        case 0x1E: return "PreventAllowMediumRemoval";
        case 0x25: return "ReadCapacity";
        case 0x28: return "Read10";
        case 0x2A: return "Write10";
        case 0x2B: return "Seek10";
        case 0x2C: return "Erase10";
        case 0x2E: return "WriteVerify";
        case 0x2F: return "Verify";
        case 0x34: return "PreFetch/ReadPosition";
        case 0x35: return "SynchronizeCache";
        case 0x36: return "LockUnlockCache";
        case 0x37: return "ReadDefectData";
        case 0x3B: return "WriteBuffer";
        case 0x3C: return "ReadBuffer";
        case 0x42: return "CDROM Read SubChannel";
        case 0x43: return "CDROM Read TOC";
        case 0x44: return "CDROM Read Header";
        case 0x46: return "CDROM GetConfiguration";
        case 0x4A: return "GetEventStatusNotification";
        case 0x4B: return "CDROM PauseResume";
        case 0x4E: return "CDROM StopPlayScan";
        case 0x51: return "CDROM ReadDiscInformation";
        case 0x45: return "CDROM PlayAudio10";
        case 0xA5: return "CDROM PlayAudio12";
        case 0x47: return "CDROM PlayAudioMSF";
        case 0x48: return "CDROM PauseResume";
        case 0x52: return "CDROM ReadTrackInformation";
        case 0xBB: return "CDROM SetCDSpeed";
        case 0xBD: return "CDROM MechanismStatus";
        case 0xBE: return "ReadCD";
        case 0xB9: return "ReadCDMSF";
        case 0x55: return "ModeSelect10";
        case 0x5A: return "ModeSense10";
        case 0xAC: return "Erase12";
        case 0xA8: return "Read12";
        case 0xC0: return "OMTI-5204 DefineFlexibleDiskFormat";
        case 0xC2: return "OMTI-5204 AssignDiskParameters";
        case 0xE0: return "Xebec RAM Diagnostic";
        case 0xE4: return "Xebec Drive Diagnostic";
        case BLUESCSI_TOOLBOX_COUNT_FILES: return "BLUESCSI_TOOLBOX_COUNT_FILES";
        case BLUESCSI_TOOLBOX_LIST_FILES: return "BLUESCSI_TOOLBOX_LIST_FILES";
        case BLUESCSI_TOOLBOX_GET_FILE: return "BLUESCSI_TOOLBOX_GET_FILE";
        case BLUESCSI_TOOLBOX_SEND_FILE_PREP: return "BLUESCSI_TOOLBOX_SEND_FILE_PREP";
        case BLUESCSI_TOOLBOX_SEND_FILE_10: return "BLUESCSI_TOOLBOX_SEND_FILE_10";
        case BLUESCSI_TOOLBOX_SEND_FILE_END: return "BLUESCSI_TOOLBOX_SEND_FILE_END";
        case BLUESCSI_TOOLBOX_TOGGLE_DEBUG: return "BLUESCSI_TOOLBOX_TOGGLE_DEBUG";
        case BLUESCSI_TOOLBOX_LIST_CDS: return "BLUESCSI_TOOLBOX_LIST_CDS";
        case BLUESCSI_TOOLBOX_SET_NEXT_CD: return "BLUESCSI_TOOLBOX_SET_NEXT_CD";
        case BLUESCSI_TOOLBOX_LIST_DEVICES: return "BLUESCSI_TOOLBOX_LIST_DEVICES";
        case BLUESCSI_TOOLBOX_COUNT_CDS: return "BLUESCSI_TOOLBOX_COUNT_CDS";
        default:   return "Unknown";
    }
}

static void printNewPhase(int phase, bool initiator = false)
{
    g_LogData = false;
    g_LogInitiatorCommand = false;
    if (!g_log_debug)
    {
        return;
    }

    switch(phase)
    {
        case BUS_FREE:
            debuglog("-- BUS_FREE");
            break;

        case BUS_BUSY:
            debuglog("-- BUS_BUSY");
            break;

        case ARBITRATION:
            debuglog("---- ARBITRATION");
            break;

        case SELECTION:
            if (initiator)
                debuglog("---- SELECTION");
            else
                debuglog("---- SELECTION: ", (int)(*SCSI_STS_SELECTED & 7));
            break;

        case RESELECTION:
            debuglog("---- RESELECTION");
            break;

        case STATUS:
            if (initiator)
            {
                debuglog("---- STATUS");
                g_LogData = true;
            }
            else if (scsiDev.status == GOOD)
            {
                debuglog("---- STATUS: 0 GOOD");
            }
            else if (scsiDev.status == CHECK_CONDITION && scsiDev.target)
            {
                debuglog("---- STATUS: 2 CHECK_CONDITION, sense ", (uint32_t)scsiDev.target->sense.asc);
            }
            else
            {
                debuglog("---- STATUS: ", (int)scsiDev.status);
            }
            break;

        case COMMAND:
            g_LogInitiatorCommand = initiator;
            g_LogData = true;
            break;

        case DATA_IN:
            if (!initiator && scsiDev.target->syncOffset > 0)
                debuglog("---- DATA_IN, syncOffset ", (int)scsiDev.target->syncOffset,
                                   " syncPeriod ", (int)scsiDev.target->syncPeriod);
            else
                debuglog("---- DATA_IN");
            break;

        case DATA_OUT:
            if (!initiator && scsiDev.target->syncOffset > 0)
                debuglog("---- DATA_OUT, syncOffset ", (int)scsiDev.target->syncOffset,
                                    " syncPeriod ", (int)scsiDev.target->syncPeriod);
	    // log Xebec vendor commands data
	    else if (scsiDev.cdb[0] == 0x0C || scsiDev.cdb[0] == 0x0F)
		    g_LogData = true;
            else
                debuglog("---- DATA_OUT");
            break;

        case MESSAGE_IN:
            debuglog("---- MESSAGE_IN");
            g_LogData = true;
            break;

        case MESSAGE_OUT:
            debuglog("---- MESSAGE_OUT");
            g_LogData = true;
            break;

        default:
            debuglog("---- PHASE: ", phase);
            break;
    }
}

void scsiLogPhaseChange(int new_phase)
{
    static int old_scsi_id = 0;
    static int old_phase = BUS_FREE;
    static int old_sync_period = 0;

    if (new_phase != old_phase)
    {
        if (old_phase == DATA_IN || old_phase == DATA_OUT)
        {
            debuglog("---- Total IN: ", g_InByteCount, " OUT: ", g_OutByteCount, " CHECKSUM: ", (int)g_DataChecksum);
        }
	// log Xebec vendor command
        if (old_phase == DATA_OUT && scsiDev.cdb[0] == 0x0C && g_OutByteCount == 8)
	{
		int cylinders = ((uint16_t)scsiDev.data[0] << 8) + scsiDev.data[1];
		int heads = scsiDev.data[2];
		int reducedWrite = ((uint16_t)scsiDev.data[3] << 8) + scsiDev.data[4];
		int writePrecomp = ((uint16_t)scsiDev.data[5] << 8) + scsiDev.data[6];
		int eccBurst = scsiDev.data[7];
		debuglog("---- Xebec Initialize Drive Characteristics: cylinders=", cylinders, " heads=", heads,
				 " reducedWrite=", reducedWrite, " writePrecomp=", writePrecomp, " eccBurst=", eccBurst);
	}
        g_InByteCount = g_OutByteCount = 0;
        g_DataChecksum = 0;

        if (old_phase >= 0 &&
            scsiDev.target != NULL &&
            old_scsi_id == scsiDev.target->targetId &&
            old_sync_period != scsiDev.target->syncPeriod)
        {
            // Add a log message when negotiated synchronous speed changes.
            int syncper = scsiDev.target->syncPeriod;
            int syncoff = scsiDev.target->syncOffset;

            if (syncper > 0)
            {
                int mbyte_per_s = (1000 + syncper * 2) / (syncper * 4);
                log("SCSI ID ", (int)scsiDev.target->targetId,
                    " negotiated synchronous mode ", mbyte_per_s, " MB/s ",
                    "(period 4x", syncper, " ns, offset ", syncoff, " bytes)");
            }
            else
            {
                log("SCSI ID ", (int)scsiDev.target->targetId,
                    " negotiated asynchronous mode ",
                    "(period 4x", syncper, " ns, offset ", syncoff, " bytes)");
            }
        }

        printNewPhase(new_phase);
        old_phase = new_phase;
        old_sync_period = scsiDev.target->syncPeriod;
        old_scsi_id = scsiDev.target->targetId;
    }
}

void scsiLogInitiatorPhaseChange(int new_phase)
{
    static int old_phase = BUS_FREE;

    if (new_phase != old_phase)
    {
        if (old_phase == DATA_IN || old_phase == DATA_OUT)
        {
            debuglog("---- Total IN: ", g_InByteCount, " OUT: ", g_OutByteCount, " CHECKSUM: ", (int)g_DataChecksum);
        }
        g_InByteCount = g_OutByteCount = 0;
        g_DataChecksum = 0;

        printNewPhase(new_phase, true);
        old_phase = new_phase;
    }
}

void scsiLogDataIn(const uint8_t *buf, uint32_t length)
{
    if (g_LogData)
    {
        debuglog("------ IN: ", bytearray(buf, length));
    }

    if (g_log_debug)
    {
        // BSD checksum algorithm
        for (uint32_t i = 0; i < length; i++)
        {
            g_DataChecksum = (g_DataChecksum >> 1) + ((g_DataChecksum & 1) << 15);
            g_DataChecksum += buf[i];
        }
    }

    g_InByteCount += length;
}

void scsiLogDataOut(const uint8_t *buf, uint32_t length)
{
    if (buf == scsiDev.cdb || g_LogInitiatorCommand)
    {
        debuglog("---- COMMAND: ", getCommandName(buf[0]));
    }

    if (g_LogData)
    {
        debuglog("------ OUT: ", bytearray(buf, length));
    }

    if (g_log_debug)
    {
        // BSD checksum algorithm
        for (uint32_t i = 0; i < length; i++)
        {
            g_DataChecksum = (g_DataChecksum >> 1) + ((g_DataChecksum & 1) << 15);
            g_DataChecksum += buf[i];
        }
    }

    g_OutByteCount += length;
}
