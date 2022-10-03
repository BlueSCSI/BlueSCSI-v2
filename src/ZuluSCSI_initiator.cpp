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
#include "SdFat.h"

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

static struct {
    // Bitmap of all drives that have been imaged
    uint32_t drives_imaged;

    // Is imaging a drive in progress, or are we scanning?
    bool imaging;

    // Information about currently selected drive
    int target_id;
    uint32_t sectorsize;
    uint32_t sectorcount;
    uint32_t sectors_done;

    // Retry information for sector reads.
    // If a large read fails, retry is done sector-by-sector.
    int retrycount;
    uint32_t failposition;

    FsFile target_file;
} g_initiator_state;

extern SdFs SD;

// Initialization of initiator mode
void scsiInitiatorInit()
{
    scsiHostPhyReset();

    g_initiator_state.drives_imaged = 0;
    g_initiator_state.imaging = false;
    g_initiator_state.target_id = -1;
    g_initiator_state.sectorsize = 0;
    g_initiator_state.sectorcount = 0;
    g_initiator_state.sectors_done = 0;
    g_initiator_state.retrycount = 0;
    g_initiator_state.failposition = 0;
}

// High level logic of the initiator mode
void scsiInitiatorMainLoop()
{
    if (!g_initiator_state.imaging)
    {
        // Scan for SCSI drives one at a time
        g_initiator_state.target_id = (g_initiator_state.target_id + 1) % 8;
        g_initiator_state.sectors_done = 0;
        g_initiator_state.retrycount = 0;

        if (!(g_initiator_state.drives_imaged & (1 << g_initiator_state.target_id)))
        {
            delay(1000);
            LED_ON();
            bool readcapok =
                scsiTestUnitReady(g_initiator_state.target_id) &&
                scsiStartStopUnit(g_initiator_state.target_id, true) &&
                scsiInitiatorReadCapacity(g_initiator_state.target_id, &g_initiator_state.sectorcount, &g_initiator_state.sectorsize);
            LED_OFF();

            if (readcapok)
            {
                azlog("SCSI id ", g_initiator_state.target_id,
                    " capacity ", (int)g_initiator_state.sectorcount,
                    " sectors x ", (int)g_initiator_state.sectorsize, " bytes");

                char filename[] = "HD00_imaged.hda";
                filename[2] += g_initiator_state.target_id;

                SD.remove(filename);
                g_initiator_state.target_file = SD.open(filename, O_RDWR | O_CREAT | O_TRUNC);
                if (!g_initiator_state.target_file.isOpen())
                {
                    azlog("Failed to open file for writing: ", filename);
                    return;
                }

                azlog("Starting to copy drive data to ", filename);
                g_initiator_state.target_file.preAllocate((uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize);
                g_initiator_state.imaging = true;
            }
        }
    }
    else
    {
        // Copy sectors from SCSI drive to file
        if (g_initiator_state.sectors_done >= g_initiator_state.sectorcount)
        {
            scsiStartStopUnit(g_initiator_state.target_id, false);
            azlog("Finished imaging drive with id ", g_initiator_state.target_id);
            LED_OFF();
            g_initiator_state.drives_imaged |= (1 << g_initiator_state.target_id);
            g_initiator_state.imaging = false;
            g_initiator_state.target_file.close();
            return;
        }

        // Update status indicator, the led blinks every 5 seconds and is on the longer the more data has been transferred
        uint32_t time_start = millis();
        int phase = (time_start % 5000);
        int duty = g_initiator_state.sectors_done * 5000 / g_initiator_state.sectorcount;
        if (duty < 100) duty = 100;
        if (phase <= duty)
        {
            LED_ON();
        }
        else
        {
            LED_OFF();
        }

        // How many sectors to read in one batch?
        int numtoread = g_initiator_state.sectorcount - g_initiator_state.sectors_done;
        if (numtoread > 512) numtoread = 512;

        // Retry sector-by-sector after failure
        if (g_initiator_state.sectors_done < g_initiator_state.failposition)
            numtoread = 1;

        bool status = scsiInitiatorReadDataToFile(g_initiator_state.target_id,
            g_initiator_state.sectors_done, numtoread, g_initiator_state.sectorsize,
            g_initiator_state.target_file);

        if (!status)
        {
            azlog("Failed to transfer ", numtoread, " sectors starting at ", (int)g_initiator_state.sectors_done);

            if (g_initiator_state.retrycount < 5)
            {
                azlog("Retrying.. ", g_initiator_state.retrycount, "/5");
                delay(200);
                scsiHostPhyReset();
                delay(200);

                g_initiator_state.retrycount++;
                g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);

                if (g_initiator_state.retrycount > 1 && numtoread > 1)
                {
                    azlog("Multiple failures, retrying sector-by-sector");
                    g_initiator_state.failposition = g_initiator_state.sectors_done + numtoread;
                }
            }
            else
            {
                azlog("Retry limit exceeded, skipping one sector");
                g_initiator_state.retrycount = 0;
                g_initiator_state.sectors_done++;
                g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);
            }
        }
        else
        {
            g_initiator_state.retrycount = 0;
            g_initiator_state.sectors_done += numtoread;
            g_initiator_state.target_file.flush();

            int speed_kbps = numtoread * g_initiator_state.sectorsize / (millis() - time_start);
            azlog("SCSI read succeeded, sectors done: ",
                  (int)g_initiator_state.sectors_done, " / ", (int)g_initiator_state.sectorcount,
                  " speed ", speed_kbps, " kB/s");
        }
    }
}

/*************************************
 * Low level command implementations *
 *************************************/

int scsiInitiatorRunCommand(int target_id,
                            const uint8_t *command, size_t cmdLen,
                            uint8_t *bufIn, size_t bufInLen,
                            const uint8_t *bufOut, size_t bufOutLen,
                            bool returnDataPhase)
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
        if (phase == MESSAGE_IN)
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
            if (returnDataPhase) return 0;
            if (bufInLen == 0)
            {
                azlog("DATA_IN phase but no data to receive!");
                status = -3;
                break;
            }

            if (!scsiHostRead(bufIn, bufInLen))
            {
                azlog("scsiHostRead failed, tried to read ", (int)bufInLen, " bytes");
                status = -2;
                break;
            }
        }
        else if (phase == DATA_OUT)
        {
            if (returnDataPhase) return 0;
            if (bufOutLen == 0)
            {
                azlog("DATA_OUT phase but no data to send!");
                status = -3;
                break;
            }

            if (!scsiHostWrite(bufOut, bufOutLen))
            {
                azlog("scsiHostWrite failed, was writing ", bytearray(bufOut, bufOutLen));
                status = -2;
                break;
            }
        }
        else if (phase == STATUS)
        {
            uint8_t tmp = 0;
            scsiHostRead(&tmp, 1);
            status = tmp;
            azdbg("------ STATUS: ", tmp);
        }
    }

    scsiHostPhyRelease();

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
        
        *sectorcount += 1; // SCSI reports last sector address

        *sectorsize = ((uint32_t)response[4] << 24)
                    | ((uint32_t)response[5] << 16)
                    | ((uint32_t)response[6] <<  8)
                    | ((uint32_t)response[7] <<  0);

        return true;
    }
    else if (status == 2)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);
        azlog("READ CAPACITY on target ", target_id, " failed, sense key ", sense_key);
        return false;
    }
    else
    {
        *sectorcount = *sectorsize = 0;
        return false;
    }
} 

// Execute REQUEST SENSE command to get more information about error status
bool scsiRequestSense(int target_id, uint8_t *sense_key)
{
    uint8_t command[6] = {0x03, 0, 0, 0, 4, 0};
    uint8_t response[18] = {0};

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);

    azdbg("RequestSense response: ", bytearray(response, 18));

    *sense_key = response[2];
    return status == 0;
}

// Execute UNIT START STOP command to load/unload media
bool scsiStartStopUnit(int target_id, bool start)
{
    uint8_t command[6] = {0x1B, 0, 0, 0, 0, 0};
    uint8_t response[4] = {0};

    if (start) command[4] |= 1;

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);

    if (status == 2)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);
        azlog("START STOP UNIT on target ", target_id, " failed, sense key ", sense_key);
    }

    return status == 0;
}

// Execute INQUIRY command
bool scsiInquiry(int target_id, uint8_t inquiry_data[36])
{
    uint8_t command[6] = {0x12, 0, 0, 0, 36, 0};
    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         inquiry_data, 36,
                                         NULL, 0);
    return status == 0;
}

// Execute TEST UNIT READY command and handle unit attention state
bool scsiTestUnitReady(int target_id)
{
    for (int retries = 0; retries < 2; retries++)
    {
        uint8_t command[6] = {0x00, 0, 0, 0, 0, 0};
        int status = scsiInitiatorRunCommand(target_id,
                                            command, sizeof(command),
                                            NULL, 0,
                                            NULL, 0);

        if (status == 0)
        {
            return true;
        }
        else if (status == -1)
        {
            // No response to select
            return false;
        }
        else if (status == 2)
        {
            uint8_t sense_key;
            scsiRequestSense(target_id, &sense_key);

            if (sense_key == 6)
            {
                uint8_t inquiry[36];
                azlog("Target ", target_id, " reports UNIT_ATTENTION, running INQUIRY");
                scsiInquiry(target_id, inquiry);
            }
            else if (sense_key == 2)
            {
                azlog("Target ", target_id, " reports NOT_READY, running STARTSTOPUNIT");
                scsiStartStopUnit(target_id, true);
            }
        }
        else
        {
            azlog("Target ", target_id, " TEST UNIT READY response: ", status);
        }
    }

    return false;
}

// This uses callbacks to run SD and SCSI transfers in parallel
static struct {
    uint32_t bytes_sd; // Number of bytes that have been transferred on SD card side
    uint32_t bytes_sd_scheduled; // Number of bytes scheduled for transfer on SD card side
    uint32_t bytes_scsi; // Number of bytes that have been scheduled for transfer on SCSI side
    uint32_t bytes_scsi_done; // Number of bytes that have been transferred on SCSI side
    
    uint32_t bytes_per_sector;
    bool all_ok;
} g_initiator_transfer;

static void initiatorReadSDCallback(uint32_t bytes_complete)
{
    if (g_initiator_transfer.bytes_scsi_done < g_initiator_transfer.bytes_scsi)
    {
        // How many bytes remaining in the transfer?
        uint32_t remain = g_initiator_transfer.bytes_scsi - g_initiator_transfer.bytes_scsi_done;
        uint32_t len = remain;

        // Limit maximum amount of data transferred at one go, to give enough callbacks to SD driver.
        // Select the limit based on total bytes in the transfer.
        // Transfer size is reduced towards the end of transfer to reduce the dead time between
        // end of SCSI transfer and the SD write completing.
        uint32_t limit = g_initiator_transfer.bytes_scsi / 8;
        uint32_t bytesPerSector = g_initiator_transfer.bytes_per_sector;
        if (limit < PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE) limit = PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE;
        if (limit > PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE) limit = PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE;
        if (limit > len) limit = PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE;
        if (limit < bytesPerSector) limit = bytesPerSector;

        if (len > limit)
        {
            len = limit;
        }

        // Split read so that it doesn't wrap around buffer edge
        uint32_t bufsize = sizeof(scsiDev.data);
        uint32_t start = (g_initiator_transfer.bytes_scsi_done % bufsize);
        if (start + len > bufsize)
            len = bufsize - start;

        // Don't overwrite data that has not yet been written to SD card
        uint32_t sd_ready_cnt = g_initiator_transfer.bytes_sd + bytes_complete;
        if (g_initiator_transfer.bytes_scsi_done + len > sd_ready_cnt + bufsize)
            len = sd_ready_cnt + bufsize - g_initiator_transfer.bytes_scsi_done;
        
        if (sd_ready_cnt == g_initiator_transfer.bytes_sd_scheduled &&
            g_initiator_transfer.bytes_sd_scheduled + bytesPerSector <= g_initiator_transfer.bytes_scsi_done)
        {
            // Current SD transfer is complete, it is better we return now and offer a chance for the next
            // transfer to begin.
            return;
        }

        // Keep transfers a multiple of sector size.
        if (remain >= bytesPerSector && len % bytesPerSector != 0)
        {
            len -= len % bytesPerSector;
        }

        if (len == 0)
            return;

        // azdbg("SCSI read ", (int)start, " + ", (int)len, ", sd ready cnt ", (int)sd_ready_cnt, " ", (int)bytes_complete, ", scsi done ", (int)g_initiator_transfer.bytes_scsi_done);
        if (!scsiHostRead(&scsiDev.data[start], len))
        {
            azlog("Read failed at byte ", (int)g_initiator_transfer.bytes_scsi_done);
            g_initiator_transfer.all_ok = false;
        }
        g_initiator_transfer.bytes_scsi_done += len;
    }
}

static void scsiInitiatorWriteDataToSd(FsFile &file, bool use_callback)
{
    // Figure out longest continuous block in buffer
    uint32_t bufsize = sizeof(scsiDev.data);
    uint32_t start = g_initiator_transfer.bytes_sd % bufsize;
    uint32_t len = g_initiator_transfer.bytes_scsi_done - g_initiator_transfer.bytes_sd;
    if (start + len > bufsize) len = bufsize - start;

    // Try to do writes in multiple of 512 bytes
    // This allows better performance for SD card access.
    if (len >= 512) len &= ~511;

    // Start writing to SD card and simultaneously reading more from SCSI bus
    uint8_t *buf = &scsiDev.data[start];
    // azdbg("SD write ", (int)start, " + ", (int)len);

    if (use_callback)
    {
        azplatform_set_sd_callback(&initiatorReadSDCallback, buf);
    }

    g_initiator_transfer.bytes_sd_scheduled = g_initiator_transfer.bytes_sd + len;
    if (file.write(buf, len) != len)
    {
        azlog("scsiInitiatorReadDataToFile: SD card write failed");
        g_initiator_transfer.all_ok = false;
    }
    azplatform_set_sd_callback(NULL, NULL);
    g_initiator_transfer.bytes_sd += len;
}

bool scsiInitiatorReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
                                 FsFile &file)
{
    uint8_t command[10] = {0x28, 0x00,
        (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
        (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
        0x00,
        (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
        0x00
    };

    // Start executing command, return in data phase
    int status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);

    if (status != 0)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);

        azlog("scsiInitiatorReadDataToFile: READ10 failed: ", status, " sense key ", sense_key);
        scsiHostPhyRelease();
        return false;
    }

    SCSI_PHASE phase;

    g_initiator_transfer.bytes_scsi = sectorcount * sectorsize;
    g_initiator_transfer.bytes_per_sector = sectorsize;
    g_initiator_transfer.bytes_sd = 0;
    g_initiator_transfer.bytes_sd_scheduled = 0;
    g_initiator_transfer.bytes_scsi_done = 0;
    g_initiator_transfer.all_ok = true;

    while (true)
    {
        phase = (SCSI_PHASE)scsiHostPhyGetPhase();
        if (phase != DATA_IN && phase != BUS_BUSY)
        {
            break;
        }

        // Read next block from SCSI bus if buffer empty
        if (g_initiator_transfer.bytes_sd == g_initiator_transfer.bytes_scsi_done)
        {
            initiatorReadSDCallback(0);
        }
        else
        {
            // Write data to SD card and simultaneously read more from SCSI
            scsiInitiatorWriteDataToSd(file, true);
        }
    }

    // Write any remaining buffered data
    while (g_initiator_transfer.bytes_sd < g_initiator_transfer.bytes_scsi_done)
    {
        scsiInitiatorWriteDataToSd(file, false);
    }

    if (g_initiator_transfer.bytes_sd != g_initiator_transfer.bytes_scsi)
    {
        azlog("SCSI read from sector ", (int)start_sector, " was incomplete: expected ",
             (int)g_initiator_transfer.bytes_scsi, " got ", (int)g_initiator_transfer.bytes_sd, " bytes");
        g_initiator_transfer.all_ok = false;
    }

    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        if (phase == MESSAGE_IN)
        {
            uint8_t dummy = 0;
            scsiHostRead(&dummy, 1);
        }
        else if (phase == MESSAGE_OUT)
        {
            uint8_t identify_msg = 0x80;
            scsiHostWrite(&identify_msg, 1);
        }
        else if (phase == STATUS)
        {
            uint8_t tmp = 0;
            scsiHostRead(&tmp, 1);
            status = tmp;
            azdbg("------ STATUS: ", tmp);
        }
    }

    scsiHostPhyRelease();

    return status == 0 && g_initiator_transfer.all_ok;
}


#endif
