/*
 *  ZuluSCSI
 *  Copyright (c) 2022 Rabbit Hole Computing
 * 
 * Main program for initiator mode.
 */

#include "ZuluSCSI_config.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_log_trace.h"
#include "ZuluSCSI_initiator.h"
#include <ZuluSCSI_platform.h>

#include <scsi2sd.h>
extern "C" {
#include <scsi.h>
}

#ifndef PLATFORM_HAS_INITIATOR_MODE

void scsiInitiatorInit()
{
}

void scsiInitiatorMainLoop()
{
}

int scsiInitiatorRunCommand(const uint8_t *command, size_t cmdlen,
                            uint8_t *bufIn, size_t bufInLen,
                            const uint8_t *bufOut, size_t bufOutLen)
{
    return -1;
}

bool scsiInitiatorReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize)
{
    return false;
}

#else

/*************************************
 * High level initiator mode logic   *
 *************************************/

static uint32_t g_initiator_drives_imaged;
static int g_initiator_next_id;

// Initialization of initiator mode
void scsiInitiatorInit()
{
    scsiHostPhyReset();

    g_initiator_drives_imaged = 0;
    g_initiator_next_id = 0;
}

// High level logic of the initiator mode
void scsiInitiatorMainLoop()
{
    // Scan for SCSI drives one at a time
    g_initiator_next_id = (g_initiator_next_id + 1) % 8;

    uint32_t sectorcount, sectorsize;
    if (scsiInitiatorReadCapacity(g_initiator_next_id, &sectorcount, &sectorsize))
    {
        azlog("SCSI id ", g_initiator_next_id, " capacity ", (int)sectorcount, " sectors x ", (int)sectorsize, " bytes");
    }

    delay(1000);
}

/*************************************
 * Low level command implementations *
 *************************************/

int scsiInitiatorRunCommand(int target_id,
                            const uint8_t *command, size_t cmdLen,
                            uint8_t *bufIn, size_t bufInLen,
                            const uint8_t *bufOut, size_t bufOutLen)
{
    if (!scsiHostPhySelect(target_id))
    {
        azdbg("------ Target ", target_id, " did not respond");
        scsiHostPhyRelease();
        return -1;
    }

    SCSI_PHASE phase;
    int status = -1;
    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        if (!scsiHostRequestWaiting())
        {
            // Wait for target to assert REQ before dealing with the new phase.
            // This way we don't react to any spurious status signal changes.
        }
        else if (phase == MESSAGE_IN)
        {
            uint8_t dummy = 0;
            scsiHostRead(&dummy, 1);
        }
        else if (phase == MESSAGE_OUT)
        {
            uint8_t identify_msg = 0x80;
            scsiHostWrite(&identify_msg, 1);
        }
        else if (phase == COMMAND)
        {
            scsiHostWrite(command, cmdLen);
        }
        else if (phase == DATA_IN)
        {
            scsiHostRead(bufIn, bufInLen);
        }
        else if (phase == DATA_OUT)
        {
            scsiHostWrite(bufOut, bufOutLen);
        }
        else if (phase == STATUS)
        {
            uint8_t tmp = 0;
            scsiHostRead(&tmp, 1);
            status = tmp;
            azdbg("------ STATUS: ", tmp);
        }
    }

    return status;
}

bool scsiInitiatorReadCapacity(int target_id, uint32_t *sectorcount, uint32_t *sectorsize)
{
    uint8_t command[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t response[8] = {0};
    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);
    
    if (status == 0)
    {
        *sectorcount = ((uint32_t)response[0] << 24)
                    | ((uint32_t)response[1] << 16)
                    | ((uint32_t)response[2] <<  8)
                    | ((uint32_t)response[3] <<  0);
        
        *sectorsize = ((uint32_t)response[4] << 24)
                    | ((uint32_t)response[5] << 16)
                    | ((uint32_t)response[6] <<  8)
                    | ((uint32_t)response[7] <<  0);

        return true;
    }
    else
    {
        *sectorcount = *sectorsize = 0;
        return false;
    }
} 


#endif
