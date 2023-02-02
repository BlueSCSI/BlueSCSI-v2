/*
BlueSCSI
Copyright (c) 2022-2023 the BlueSCSI contributors (CONTRIBUTORS.txt)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// SCSI trace logging

#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_log.h"
#include <scsi2sd.h>

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
            int mbyte_per_s = (1000 + syncper * 2) / (syncper * 4);
            log("SCSI ID ", (int)scsiDev.target->targetId,
                  " negotiated synchronous mode ", mbyte_per_s, " MB/s ",
                  "(period 4x", syncper, " ns, offset ", syncoff, " bytes)");
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
