// SCSI trace logging

#include "ZuluSCSI_log_trace.h"
#include "ZuluSCSI_log.h"
#include <scsi2sd.h>

extern "C" {
#include <scsi.h>
#include <scsiPhy.h>
}

static bool g_LogData = false;
static int g_InByteCount = 0;
static int g_OutByteCount = 0;

static const char *getCommandName(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x00: return "TestUnitReady";
        case 0x01: return "RezeroUnit";
        case 0x03: return "RequestSense";
        case 0x04: return "FormatUnit";
        case 0x08: return "Read6";
        case 0x0A: return "Write6";
        case 0x0B: return "Seek6";
        case 0x0F: return "WriteSectorBuffer";
        case 0x12: return "Inquiry";
        case 0x15: return "ModeSelect6";
        case 0x16: return "Reserve";
        case 0x17: return "Release";
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
        case 0x34: return "PreFetch";
        case 0x35: return "SynchronizeCache";
        case 0x36: return "LockUnlockCache";
        case 0x37: return "ReadDefectData";
        case 0x3B: return "WriteBuffer";
        case 0x3C: return "ReadBuffer";
        case 0x43: return "CDROM Read TOC";
        case 0x44: return "CDROM Read Header";
        case 0x4A: return "GetEventStatusNotification";
        case 0x55: return "ModeSelect10";
        case 0x5A: return "ModeSense10";
        case 0xAC: return "Erase12";
        case 0xC0: return "OMTI-5204 DefineFlexibleDiskFormat";
        case 0xC2: return "OMTI-5204 AssignDiskParameters";
        default:   return "Unknown";
    }
}

static void printNewPhase(int phase)
{
    g_LogData = false;
    if (!g_azlog_debug)
    {
        return;
    }

    switch(phase)
    {
        case BUS_FREE:
            azdbg("-- BUS_FREE");
            break;
        
        case BUS_BUSY:
            azdbg("-- BUS_BUSY");
            break;
        
        case ARBITRATION:
            azdbg("---- ARBITRATION");
            break;
        
        case SELECTION:
            azdbg("---- SELECTION: ", (int)(*SCSI_STS_SELECTED & 7));
            break;
        
        case RESELECTION:
            azdbg("---- RESELECTION");
            break;
        
        case STATUS:
            if (scsiDev.status == GOOD)
            {
                azdbg("---- STATUS: 0 GOOD");
            }
            else if (scsiDev.status == CHECK_CONDITION && scsiDev.target)
            {
                azdbg("---- STATUS: 2 CHECK_CONDITION, sense ", (uint32_t)scsiDev.target->sense.asc);
            }
            else
            {
                azdbg("---- STATUS: ", (int)scsiDev.status);
            }
            break;
        
        case COMMAND:
            g_LogData = true;
            break;
        
        case DATA_IN:
            if (scsiDev.target->syncOffset > 0)
                azdbg("---- DATA_IN, syncOffset ", (int)scsiDev.target->syncOffset,
                                   " syncPeriod ", (int)scsiDev.target->syncPeriod);
            else
                azdbg("---- DATA_IN");
            break;
        
        case DATA_OUT:
            if (scsiDev.target->syncOffset > 0)
                azdbg("---- DATA_OUT, syncOffset ", (int)scsiDev.target->syncOffset,
                                    " syncPeriod ", (int)scsiDev.target->syncPeriod);
            else
                azdbg("---- DATA_OUT");
            break;
        
        case MESSAGE_IN:
            azdbg("---- MESSAGE_IN");
            g_LogData = true;
            break;
        
        case MESSAGE_OUT:
            azdbg("---- MESSAGE_OUT");
            g_LogData = true;
            break;
        
        default:
            azdbg("---- PHASE: ", phase);
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
            azdbg("---- Total IN: ", g_InByteCount, " OUT: ", g_OutByteCount);
        }
        g_InByteCount = g_OutByteCount = 0;


        if (old_phase >= 0 &&
            old_scsi_id == scsiDev.target->targetId &&
            old_sync_period != scsiDev.target->syncPeriod)
        {
            // Add a log message when negotiated synchronous speed changes.
            int syncper = scsiDev.target->syncPeriod;
            int syncoff = scsiDev.target->syncOffset;
            int mbyte_per_s = (1000 + syncper * 2) / (syncper * 4);
            azlog("SCSI ID ", (int)scsiDev.target->targetId,
                  " negotiated synchronous mode ", mbyte_per_s, " MB/s ",
                  "(period 4x", syncper, " ns, offset ", syncoff, " bytes)");
        }

        printNewPhase(new_phase);
        old_phase = new_phase;
        old_sync_period = scsiDev.target->syncPeriod;
        old_scsi_id = scsiDev.target->targetId;
    }
}

void scsiLogDataIn(const uint8_t *buf, uint32_t length)
{
    if (g_LogData)
    {
        azdbg("------ IN: ", bytearray(buf, length));
    }

    g_InByteCount += length;
}

void scsiLogDataOut(const uint8_t *buf, uint32_t length)
{
    if (buf == scsiDev.cdb)
    {
        azdbg("---- COMMAND: ", getCommandName(buf[0]));
    }
    
    if (g_LogData)
    {
        azdbg("------ OUT: ", bytearray(buf, length));
    }

    g_OutByteCount += length;
}
