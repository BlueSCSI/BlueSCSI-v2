// SCSI trace logging

#include "AzulSCSI_log_trace.h"
#include "AzulSCSI_log.h"
#include <scsi2sd.h>

extern "C" {
#include <scsi.h>
#include <scsiPhy.h>
}

static bool g_LogData = false;

static const char *getCommandName(uint8_t cmd)
{
    if (cmd == 0x00) return "TestUnitReady";
    if (cmd == 0x1A) return "ModeSense";
    if (cmd == 0x5A) return "ModeSense10";
    if (cmd == 0x0A) return "Write6";
    if (cmd == 0x2A) return "Write10";
    if (cmd == 0x08) return "Read6";
    if (cmd == 0x28) return "Read10";
    if (cmd == 0x12) return "Inquiry";
    if (cmd == 0x25) return "ReadCapacity";
    return "";
}

static void printCommand()
{
    uint8_t cmd = scsiDev.cdb[0];
    const char *cmdname = getCommandName(cmd);

    azdbg("---- COMMAND: ", cmdname, " ", bytearray(scsiDev.cdb, scsiDev.cdbLen));
}

static void printNewPhase(int phase)
{
    g_LogData = true; //false;
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
            break;
        
        case DATA_IN:
            azdbg("---- DATA_IN");
            break;
        
        case DATA_OUT:
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
    static int old_phase = BUS_FREE;

    if (new_phase != old_phase)
    {
        if (old_phase == COMMAND && scsiDev.cdbLen > 0)
        {
            printCommand();
        }

        printNewPhase(new_phase);
        old_phase = new_phase;
    }
}

void scsiLogDataIn(const uint8_t *buf, uint32_t length)
{
    if (g_LogData)
    {
        azdbg("------ IN: ", bytearray(buf, length));
    }
}

void scsiLogDataOut(const uint8_t *buf, uint32_t length)
{
    if (g_LogData)
    {
        azdbg("------ OUT: ", bytearray(buf, length));
    }
}