/*
 *  ZuluSCSI
 *  Copyright (c) 2022 Rabbit Hole Computing
 *
 * Main program for initiator mode.
 */

#include "BlueSCSI_config.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_initiator.h"
#include <BlueSCSI_platform.h>
#include <minIni.h>
#include "SdFat.h"

#include <scsi2sd.h>
extern "C"
{
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

static struct
{
    // Bitmap of all drives that have been imaged
    uint32_t drives_imaged;

    uint8_t initiator_id;



    // Is imaging a drive in progress, or are we scanning?
    bool imaging;
    bool audioMode;
    // Information about currently selected drive
    int target_id;
    uint32_t sectorsize;
    uint32_t sectorcount;
    uint32_t sectorcount_all;
    uint32_t sectors_done;
    uint32_t max_sector_per_transfer;
    uint32_t badSectorCount;
    uint32_t interpolationLeft;
    uint32_t interpolationRight;
    uint32_t interpolationBoth;

    bool frameMetaDataTimeIncluded;
    uint8_t frameMetaDataTimeHour;
    uint8_t frameMetaDataTimeMinutes;
    uint8_t frameMetaDataTimeSeconds;
    uint8_t frameMetaDataTimeIndex;

    bool frameMetaDataDateIncluded;
    uint8_t frameMetaDataDateYear;
    uint8_t frameMetaDataDateMonth;
    uint8_t frameMetaDataDateDay;

    uint32_t goodFrames;
    uint32_t allFrames;
    uint8_t nullFrames;
    uint8_t nullFramesMax;
    uint8_t ansiVersion;
    uint8_t maxRetryCount;
    uint8_t deviceType;

    // Retry information for sector reads.
    // If a large read fails, retry is done sector-by-sector.
    int retrycount;
    uint32_t failposition;
    bool ejectWhenDone;

    FsFile target_file;
} g_initiator_state;

extern SdFs SD;

// Initialization of initiator mode
void scsiInitiatorInit()
{
    scsiHostPhyReset();

    g_initiator_state.initiator_id = ini_getl("SCSI", "InitiatorID", 7, CONFIGFILE);
    if (g_initiator_state.initiator_id > 7)
    {
        log("InitiatorID set to illegal value in, ", CONFIGFILE, ", defaulting to 7");
        g_initiator_state.initiator_id = 7;
    }
    else
    {
        log_f("InitiatorID set to ID %d", g_initiator_state.initiator_id);
    }
    g_initiator_state.maxRetryCount = ini_getl("SCSI", "InitiatorMaxRetry", 5, CONFIGFILE);

    g_initiator_state.audioMode = ini_getbool("SCSI", "AudioMode", 0, CONFIGFILE);
    // treat initiator id as already imaged drive so it gets skipped
    g_initiator_state.drives_imaged = 1 << g_initiator_state.initiator_id;
    g_initiator_state.imaging = false;
    g_initiator_state.target_id = -1;
    g_initiator_state.sectorsize = 0;
    g_initiator_state.sectorcount = 0;
    g_initiator_state.sectors_done = 0;
    g_initiator_state.retrycount = 0;
    g_initiator_state.failposition = 0;
    g_initiator_state.max_sector_per_transfer = 512;
    g_initiator_state.ansiVersion = 0;
    g_initiator_state.badSectorCount = 0;
    g_initiator_state.deviceType = DEVICE_TYPE_DIRECT_ACCESS;
    g_initiator_state.ejectWhenDone = false;

    // Audio mode frame data for dt
    g_initiator_state.frameMetaDataDateIncluded = 0;
    g_initiator_state.frameMetaDataDateYear = 0;
    g_initiator_state.frameMetaDataDateMonth = 0;
    g_initiator_state.frameMetaDataDateDay = 0;
    g_initiator_state.frameMetaDataTimeIncluded = 0;
    g_initiator_state.frameMetaDataTimeHour = 0;
    g_initiator_state.frameMetaDataTimeMinutes = 0;
    g_initiator_state.frameMetaDataTimeSeconds = 0;
    g_initiator_state.frameMetaDataTimeIndex = 0;
    g_initiator_state.nullFrames = 0;
    g_initiator_state.nullFramesMax = ini_getl("SCSI", "InitiatorMaxNullTrys", 10, CONFIGFILE);
}

// Update progress bar LED during transfers
static void scsiInitiatorUpdateLed()
{
    // Update status indicator, the led blinks every 5 seconds and is on the longer the more data has been transferred
    const int period = 256;
    int phase = (millis() % period);
    int duty = g_initiator_state.sectors_done * period / g_initiator_state.sectorcount;

    // Minimum and maximum time to verify that the blink is visible
    if (duty < 50)
        duty = 50;
    if (duty > period - 50)
        duty = period - 50;

    if (phase <= duty)
    {
        LED_ON();
    }
    else
    {
        LED_OFF();
    }
}

void delay_with_poll(uint32_t ms)
{
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms)
    {
        platform_poll();
        delay(1);
    }
}

// High level logic of the initiator mode
void scsiInitiatorMainLoop()
{
    SCSI_RELEASE_OUTPUTS();
    SCSI_ENABLE_INITIATOR();
    if (g_scsiHostPhyReset)
    {
        log("Executing BUS RESET after aborted command");
        scsiHostPhyReset();
    }

    if (!g_initiator_state.imaging)
    {
        // Scan for SCSI drives one at a time
        g_initiator_state.target_id = (g_initiator_state.target_id + 1) % 8;
        g_initiator_state.sectors_done = 0;
        g_initiator_state.retrycount = 0;
        g_initiator_state.max_sector_per_transfer = 512;
        g_initiator_state.badSectorCount = 0;
        g_initiator_state.ejectWhenDone = false;

        if (!(g_initiator_state.drives_imaged & (1 << g_initiator_state.target_id)))
        {
            delay_with_poll(1000);

            uint8_t inquiry_data[36] = {0};

            LED_ON();
            bool startstopok =
                scsiTestUnitReady(g_initiator_state.target_id) &&
                scsiStartStopUnit(g_initiator_state.target_id, true);

            bool readcapok = false;

            if (g_initiator_state.audioMode)
            {
                readcapok = startstopok;
                g_initiator_state.sectorsize = ini_getl("SCSI", "AudioFrameSize", 5822, "bluescsi.ini");
                g_initiator_state.sectorcount = 1000000; // I dunno how to get this yet
            }
            else
            {
                readcapok = startstopok &&
                            scsiInitiatorReadCapacity(g_initiator_state.target_id,
                                                      &g_initiator_state.sectorcount,
                                                      &g_initiator_state.sectorsize);
            }
            bool inquiryok = startstopok &&
                             scsiInquiry(g_initiator_state.target_id, inquiry_data);
            g_initiator_state.ansiVersion = inquiry_data[2] & 0x7;
            LED_OFF();

            uint64_t total_bytes = 0;
            if (readcapok)
            {
                log("SCSI ID ", g_initiator_state.target_id,
                    " capacity ", (int)g_initiator_state.sectorcount,
                    " sectors x ", (int)g_initiator_state.sectorsize, " bytes");
                log_f("SCSI-%d: Vendor: %.8s, Product: %.16s, Version: %.4s",
                      g_initiator_state.ansiVersion,
                      &inquiry_data[8],
                      &inquiry_data[16],
                      &inquiry_data[32]);

                // Check for well known ejectable media.
                if (strncmp((char *)(&inquiry_data[8]), "IOMEGA", 6) == 0 &&
                    strncmp((char *)(&inquiry_data[16]), "ZIP", 3) == 0)
                {
                    g_initiator_state.ejectWhenDone = true;
                }
                g_initiator_state.sectorcount_all = g_initiator_state.sectorcount;

                total_bytes = (uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize;
                log("Drive total size is ", (int)(total_bytes / (1024 * 1024)), " MiB");
                if (total_bytes >= 0xFFFFFFFF && SD.fatType() != FAT_TYPE_EXFAT)
                {
                    // Note: the FAT32 limit is 4 GiB - 1 byte
                    log("Image files equal or larger than 4 GiB are only possible on exFAT filesystem");
                    log("Please reformat the SD card with exFAT format to image this drive.");
                    g_initiator_state.sectorsize = 0;
                    g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 0;
                }
                if (g_initiator_state.ansiVersion < 0x02)
                {
                    // this is a SCSI-1 drive, use READ6 and 256 bytes to be safe.
                    g_initiator_state.max_sector_per_transfer = 256;
                }
            }
            else if (startstopok)
            {
                log("SCSI ID ", g_initiator_state.target_id, " responds but ReadCapacity command failed");
                log("Possibly SCSI-1 drive? Attempting to read up to 1 GB.");
                g_initiator_state.sectorsize = 512;
                g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 2097152;
                g_initiator_state.max_sector_per_transfer = 128;
            }
            else
            {
                log("* No response from SCSI ID ", g_initiator_state.target_id);
                g_initiator_state.sectorsize = 0;
                g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 0;
            }

            #define FILE_NAME_MAX_SIZE 32

            char Buffer[FILE_NAME_MAX_SIZE] = {0};

            ini_gets("SCSI", "File1", "HD00_imaged.hda", Buffer, FILE_NAME_MAX_SIZE, "bluescsi.ini");
            bool customNameSystem = strcmp(Buffer, "HD00_imaged.hda") == 0;

            char * filename_format = "HD00_imaged.hda";

            if (inquiryok)
            {
                g_initiator_state.deviceType = inquiry_data[0] & 0x1F;
                if (g_initiator_state.deviceType == DEVICE_TYPE_CD)
                {
                    filename_format = "CD00_imaged.iso";
                    g_initiator_state.ejectWhenDone = true;
                }
                else if (g_initiator_state.deviceType != DEVICE_TYPE_DIRECT_ACCESS)
                {
                    log("Unhandled device type: ", g_initiator_state.deviceType, ". Handling it as Direct Access Device.");
                }
            }
            if (g_initiator_state.sectorcount > 0)
            {
                char filename[32] = {0};
                int lun = 0;

                strncpy(filename, filename_format, sizeof(filename) - 1);
                filename[2] += g_initiator_state.target_id;


                uint64_t sd_card_free_bytes = (uint64_t)SD.vol()->freeClusterCount() * SD.vol()->bytesPerCluster();
                if (sd_card_free_bytes < total_bytes)
                {
                    log("SD Card only has ", (int)(sd_card_free_bytes / (1024 * 1024)), " MiB - not enough free space to image this drive!");
                    g_initiator_state.imaging = false;
                    return;
                }

                int Mode = -1;
                if (g_initiator_state.audioMode)
                    scsiSetMode(AUDIO_MODE, g_initiator_state.target_id);
                else
                    scsiSetMode(DATA_MODE, g_initiator_state.target_id);

                scsiGetMode(&Mode, g_initiator_state.target_id);
                if (Mode == AUDIO_MODE)
                    log("The SCSI is in Audio Mode");
                if (Mode == DATA_MODE)
                    log("The SCSI is in Data  Mode");
                if (g_initiator_state.audioMode)
                {
                    // scsiLocate(0, g_initiator_state.target_id);
                    uint32_t position;
                    scsiReadPosition(&position, g_initiator_state.target_id);
                    log("Starting at: ", position);
                    g_initiator_state.max_sector_per_transfer = 1;
                    scsiRewind(g_initiator_state.target_id);
                }


                if(customNameSystem){

                    char * CustomFileVariable = "File1";
                    char customFilename[32] = {0};
                    ini_gets("SCSI", CustomFileVariable, "[EMPTY]", customFilename, 32, "bluescsi.ini");

                    while(SD.exists(customFilename)){
                        CustomFileVariable[4]++;//Next File Variable
                        ini_gets("SCSI", CustomFileVariable, "[EMPTY]", customFilename, 32, "bluescsi.ini");
                        if(strcmp(customFilename, "[EMPTY]") == 0){
                            customNameSystem = false;
                            break;
                        }
                    }
                    if(customNameSystem){
                        strcpy(filename, customFilename);
                    }
                }
                if(!customNameSystem){//DO NOT REPLACE with else!, can be entered when running out of custom variables
                    while (SD.exists(filename))
                    {
                        filename[3] = lun++ + '0';
                    }
                }
                if (lun != 0)
                {
                    log("Using filename: ", filename, " to avoid overwriting existing file.");
                }
                g_initiator_state.target_file = SD.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
                if (!g_initiator_state.target_file.isOpen())
                {
                    log("Failed to open file for writing: ", filename);
                    return;
                }

                if (SD.fatType() == FAT_TYPE_EXFAT)
                {
                    // Only preallocate on exFAT, on FAT32 preallocating can result in false garbage data in the
                    // file if write is interrupted.
                    log("Preallocating image file");
                    g_initiator_state.target_file.preAllocate((uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize);
                }

                log("Starting to copy drive data to ", filename);
                g_initiator_state.imaging = true;
                g_initiator_state.interpolationLeft = 0;
                g_initiator_state.interpolationRight = 0;
                g_initiator_state.interpolationBoth = 0;

            }
        }
    }
    else
    {
        // Copy sectors from SCSI drive to file
        if (g_initiator_state.sectors_done >= g_initiator_state.sectorcount)
        {
            scsiStartStopUnit(g_initiator_state.target_id, false);
            log("Finished imaging drive with id ", g_initiator_state.target_id);
            LED_OFF();

            if (g_initiator_state.sectorcount != g_initiator_state.sectorcount_all)
            {
                log("NOTE: Image size was limited to first 4 GiB due to SD card filesystem limit");
                log("Please reformat the SD card with exFAT format to image this drive fully");
            }

            if (g_initiator_state.badSectorCount != 0)
            {
                log_f("NOTE: There were %d bad sectors that could not be read off this drive.", g_initiator_state.badSectorCount);
            }

            if (g_initiator_state.interpolationLeft)
                log_f("There are %d left interpolations", g_initiator_state.interpolationLeft);
            if (g_initiator_state.interpolationRight)
                log_f("There are %d right interpolations", g_initiator_state.interpolationRight);
            if (g_initiator_state.interpolationBoth)
                log_f("There are %d both interpolations", g_initiator_state.interpolationBoth);

            if (!g_initiator_state.ejectWhenDone)
            {
                log("Marking this ID as imaged, wont ask it again.");
                g_initiator_state.drives_imaged |= (1 << g_initiator_state.target_id);
            }
            g_initiator_state.imaging = false;
            if (g_initiator_state.audioMode)
                g_initiator_state.target_file.truncate();
            g_initiator_state.target_file.close();
            return;
        }

        scsiInitiatorUpdateLed();

        // How many sectors to read in one batch?
        int numtoread = g_initiator_state.sectorcount - g_initiator_state.sectors_done;
        if (numtoread > g_initiator_state.max_sector_per_transfer)
            numtoread = g_initiator_state.max_sector_per_transfer;

        // Retry sector-by-sector after failure
        if (g_initiator_state.sectors_done < g_initiator_state.failposition)
            numtoread = 1;

        uint32_t time_start = millis();
        bool status = scsiInitiatorReadDataToFile(g_initiator_state.target_id,
                                                  g_initiator_state.sectors_done, numtoread, g_initiator_state.sectorsize,
                                                  g_initiator_state.target_file);

        if (!status)
        {
            log("Failed to transfer ", numtoread, " sectors starting at ", (int)g_initiator_state.sectors_done);

            if (g_initiator_state.retrycount < g_initiator_state.maxRetryCount)
            {
                log("Retrying.. ", g_initiator_state.retrycount + 1, "/", (int)g_initiator_state.maxRetryCount);
                delay_with_poll(200);
                // This reset causes some drives to hang and seems to have no effect if left off.
                // scsiHostPhyReset();
                delay_with_poll(200);

                g_initiator_state.retrycount++;
                g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);

                if (g_initiator_state.retrycount > 1 && numtoread > 1)
                {
                    log("Multiple failures, retrying sector-by-sector");
                    g_initiator_state.failposition = g_initiator_state.sectors_done + numtoread;
                }
            }
            else
            {
                log("Retry limit exceeded, skipping one sector");
                g_initiator_state.retrycount = 0;
                g_initiator_state.sectors_done++;
                g_initiator_state.badSectorCount++;
                g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);
            }
        }
        else
        {
            g_initiator_state.retrycount = 0;
            g_initiator_state.sectors_done += numtoread;
            g_initiator_state.target_file.flush();

            int speed_kbps = numtoread * g_initiator_state.sectorsize / (millis() - time_start);
            if (g_initiator_state.audioMode)
            {
                log_f("SCSI read succeeded %02d/%02d/%04d %02d:%02d:%02d-Frame: %02d, sectors done: %d speed %d kB/s, [left: %d, right: %d, both: %d]",
                      g_initiator_state.frameMetaDataDateMonth, g_initiator_state.frameMetaDataDateDay, g_initiator_state.frameMetaDataDateYear + 1970,
                      g_initiator_state.frameMetaDataTimeHour, g_initiator_state.frameMetaDataTimeMinutes, g_initiator_state.frameMetaDataTimeSeconds, g_initiator_state.frameMetaDataTimeIndex,
                      g_initiator_state.sectors_done, speed_kbps,
                      g_initiator_state.interpolationLeft,
                      g_initiator_state.interpolationRight,
                      g_initiator_state.interpolationBoth);
            }
            else
                log_f("SCSI read succeeded, sectors done: %d / %d speed %d kB/s - %.2f%%",
                      g_initiator_state.sectors_done, g_initiator_state.sectorcount, speed_kbps,
                      (float)(((float)g_initiator_state.sectors_done / (float)g_initiator_state.sectorcount) * 100.0));
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
    if (!scsiHostPhySelect(target_id, g_initiator_state.initiator_id))
    {
        debuglog("------ Target ", target_id, " did not respond");
        scsiHostPhyRelease();
        return -1;
    }

    SCSI_PHASE phase;
    int status = -1;
    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        platform_poll();

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
            if (returnDataPhase)
                return 0;
            if (bufInLen == 0)
            {
                log("DATA_IN phase but no data to receive!");
                status = -3;
                break;
            }

            if (scsiHostRead(bufIn, bufInLen) == 0)
            {
                log("scsiHostRead failed, tried to read ", (int)bufInLen, " bytes");
                status = -2;
                break;
            }
        }
        else if (phase == DATA_OUT)
        {
            if (returnDataPhase)
                return 0;
            if (bufOutLen == 0)
            {
                log("DATA_OUT phase but no data to send!");
                status = -3;
                break;
            }

            if (scsiHostWrite(bufOut, bufOutLen) < bufOutLen)
            {
                log("scsiHostWrite failed, was writing ", bytearray(bufOut, bufOutLen));
                status = -2;
                break;
            }
        }
        else if (phase == STATUS)
        {
            uint8_t tmp = -1;
            scsiHostRead(&tmp, 1);
            status = tmp;
            debuglog("------ STATUS: ", tmp);
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
        *sectorcount = ((uint32_t)response[0] << 24) | ((uint32_t)response[1] << 16) | ((uint32_t)response[2] << 8) | ((uint32_t)response[3] << 0);

        *sectorcount += 1; // SCSI reports last sector address

        *sectorsize = ((uint32_t)response[4] << 24) | ((uint32_t)response[5] << 16) | ((uint32_t)response[6] << 8) | ((uint32_t)response[7] << 0);

        return true;
    }
    else if (status == 2)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        log("READ CAPACITY on target ", target_id, " failed, sense key ", sense_key, ", sense code ", sense_code);
        Log_Error(sense_key, sense_code);
        return false;
    }
    else
    {
        *sectorcount = *sectorsize = 0;
        return false;
    }
}

// Execute REQUEST SENSE command to get more information about error status
bool scsiRequestSense(int target_id, uint8_t *sense_key, uint16_t *sense_code)
{
    uint8_t command[6] = {0x03, 0, 0, 0, 18, 0};
    uint8_t response[18] = {0};

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);

    log("RequestSense response: ", bytearray(response, 18));

    *sense_key = response[2] & 0x0F;
    *sense_code = (((uint16_t)response[12]) << 8) | response[13];
    return status == 0;
}

// Execute UNIT START STOP command to load/unload media
bool scsiStartStopUnit(int target_id, bool start)
{
    uint8_t command[6] = {0x1B, 0x1, 0, 0, 0, 0};
    uint8_t response[4] = {0};

    if (start)
    {
        command[4] |= 1; // Start
        command[1] = 0;  // Immediate
    }
    else // stop
    {
        if (g_initiator_state.deviceType == DEVICE_TYPE_CD)
        {
            command[4] = 0b00000010; // eject(6), stop(7).
        }
    }

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);

    if (status == 2)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        log("START STOP UNIT on target ", target_id, " failed, sense key ", sense_key, ", sense code ", sense_code);
        Log_Error(sense_key, sense_code);
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
            uint16_t sense_code;
            scsiRequestSense(target_id, &sense_key, &sense_code);

            if (sense_key == 6)
            {
                uint8_t inquiry[36];
                log("Target ", target_id, " reports UNIT_ATTENTION, running INQUIRY");
                scsiInquiry(target_id, inquiry);
            }
            else if (sense_key == 2)
            {
                log("Target ", target_id, " reports NOT_READY, running STARTSTOPUNIT");
                scsiStartStopUnit(target_id, true);
            }
        }
        else
        {
            log("Target ", target_id, " TEST UNIT READY response: ", status);
        }
    }

    return false;
}

// This uses callbacks to run SD and SCSI transfers in parallel
static struct
{
    uint32_t bytes_sd;           // Number of bytes that have been transferred on SD card side
    uint32_t bytes_sd_scheduled; // Number of bytes scheduled for transfer on SD card side
    uint32_t bytes_scsi;         // Number of bytes that have been scheduled for transfer on SCSI side
    uint32_t bytes_scsi_done;    // Number of bytes that have been transferred on SCSI side

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
        if (limit < PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE)
            limit = PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE;
        if (limit > PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE)
            limit = PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE;
        if (limit > len)
            limit = PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE;
        if (limit < bytesPerSector)
            limit = bytesPerSector;

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

        // debuglog("SCSI read ", (int)start, " + ", (int)len, ", sd ready cnt ", (int)sd_ready_cnt, " ", (int)bytes_complete, ", scsi done ", (int)g_initiator_transfer.bytes_scsi_done);
        if (scsiHostRead(&scsiDev.data[start], len) != len)
        {
            log("Read failed at byte ", (int)g_initiator_transfer.bytes_scsi_done);
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
    if (start + len > bufsize)
        len = bufsize - start;

    // Try to do writes in multiple of 512 bytes
    // This allows better performance for SD card access.
    if (len >= 512)
        len &= ~511;

    // Start writing to SD card and simultaneously reading more from SCSI bus
    uint8_t *buf = &scsiDev.data[start];
    // debuglog("SD write ", (int)start, " + ", (int)len);

    if (use_callback)
    {
        platform_set_sd_callback(&initiatorReadSDCallback, buf);
    }

    g_initiator_transfer.bytes_sd_scheduled = g_initiator_transfer.bytes_sd + len;

    if (file.write(buf, len) != len)
    {
        
        log("scsiInitiatorReadDataToFile: SD card write failed");
        g_initiator_transfer.all_ok = false;
    }
    platform_set_sd_callback(NULL, NULL);
    g_initiator_transfer.bytes_sd += len;
}

uint8_t HexToDec(uint8_t hex)
{
    return 10 * ((hex & 0xF0) >> 4) + hex & 0x0F;
}

bool scsiInitiatorReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
                                 FsFile &file)
{
    int status = -1;

    // Read6 command supports 21 bit LBA - max of 0x1FFFFF
    // ref: https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 134
    if (g_initiator_state.ansiVersion < 0x02 || (start_sector < 0x1FFFFF && sectorcount <= 256))
    {
        uint8_t command[6] = {0x08, 0, 0, 0, 0, 0};

        if (g_initiator_state.audioMode)
        {
            // ReadCommand for audio mode has 2 ways of access,
            // Here I ask for data of the sector size, this works
            // Another avenue is to ask for a number of blocks, Im not certain BLUESCSI supports that
            command[2] = (uint8_t)(sectorsize >> 16);
            command[3] = (uint8_t)(sectorsize >> 8);
            command[4] = (uint8_t)sectorsize;
        }
        else
        {
            // Use READ6 command for compatibility with old SCSI1 drives
            command[1] = (uint8_t)(start_sector >> 16);
            command[2] = (uint8_t)(start_sector >> 8);
            command[3] = (uint8_t)start_sector;
            command[4] = (uint8_t)sectorcount;
        }

        // Start executing command, return in data phase
        status = scsiInitiatorRunCommand(target_id, command, sizeof(command),
                                         NULL, 0,
                                         NULL, 0,
                                         true);
    }
    else
    {
        // Use READ10 command for larger number of blocks
        uint8_t command[10] = {0x28, 0x00,
                               (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
                               (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
                               0x00,
                               (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
                               0x00};

        // Start executing command, return in data phase
        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
    }

    if (status != 0)
    {

        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);

        log("scsiInitiatorReadDataToFile: READ failed: ", status, " sense key ", sense_key, ", sense code ", sense_code);
        if (sense_key == 0x08 && sense_code == 5 && g_initiator_state.audioMode)
        {
            // Hit end of data
            g_initiator_state.sectorcount = g_initiator_state.sectors_done;
            g_initiator_state.sectorcount_all = g_initiator_state.sectors_done;
            return true;
        }

        Log_Error(sense_key, sense_code);
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
        platform_poll();

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
            scsiInitiatorUpdateLed();
            scsiInitiatorWriteDataToSd(file, true);
        }
    }

    // Write any remaining buffered data
    while (g_initiator_transfer.bytes_sd < g_initiator_transfer.bytes_scsi_done)
    {
        platform_poll();
        scsiInitiatorWriteDataToSd(file, false);
    }

    if (g_initiator_transfer.bytes_sd != g_initiator_transfer.bytes_scsi)
    {
        log("SCSI read from sector ", (int)start_sector, " was incomplete: expected ",
            (int)g_initiator_transfer.bytes_scsi, " got ", (int)g_initiator_transfer.bytes_sd, " bytes");
        g_initiator_transfer.all_ok = false;
    }
    else
    {

        // Audio mode data has interpolation flags, little checksums that determine if the data was
        // Read Properly
        // TODO: Rerecord a frame if we record an interpolation flag
        if (g_initiator_state.audioMode)
        {
            g_initiator_state.frameMetaDataTimeIncluded = scsiDev.data[0x168B] & 0xF0;
            g_initiator_state.frameMetaDataTimeHour = HexToDec(scsiDev.data[0x168B]);
            g_initiator_state.frameMetaDataTimeMinutes = HexToDec(scsiDev.data[0x168C]);
            g_initiator_state.frameMetaDataTimeSeconds = HexToDec(scsiDev.data[0x168D]);
            g_initiator_state.frameMetaDataTimeIndex = HexToDec(scsiDev.data[0x168E]);

            g_initiator_state.frameMetaDataDateIncluded = scsiDev.data[0x1698] & 0xF0;
            g_initiator_state.frameMetaDataDateYear = HexToDec(scsiDev.data[0x1699]);
            g_initiator_state.frameMetaDataDateMonth = HexToDec(scsiDev.data[0x169A]);
            g_initiator_state.frameMetaDataDateDay = HexToDec(scsiDev.data[0x169B]);
            // g_initiator_state.frameMetaDataDateHour = HexToDec(scsiDev.data[0x169C]);
            // g_initiator_state.frameMetaDataDateMinutes = HexToDec(scsiDev.data[0x169D]);
            // g_initiator_state.frameMetaDataDateSeconds = HexToDec(scsiDev.data[0x169E]);

            if (scsiDev.data[0x16BB] & 0b00000010)
            {
                g_initiator_state.interpolationLeft++;
            }
            if (scsiDev.data[0x16BB] & 0b00000001)
            {
                g_initiator_state.interpolationRight++;
            }

            if (scsiDev.data[0x16BB] & 0b00000001 && scsiDev.data[0x16BB] & 0b00000010)
            {
                g_initiator_state.interpolationBoth++;
            }

            if (!(scsiDev.data[0x16BB] & 0b00000001 || scsiDev.data[0x16BB] & 0b00000010))
            {
                g_initiator_state.goodFrames++;
            }

            g_initiator_state.allFrames++;

            bool NullFrame = true;
            for (int i = 0x1680; i < 0x17BC; i++)
            {
                if (scsiDev.data[i])
                {
                    NullFrame = false;
                    break;
                }
            }
            if (NullFrame && g_initiator_state.nullFramesMax > 0)
            {
                if (!g_initiator_state.nullFrames)
                    log("Start of Null Frames");
                g_initiator_state.nullFrames++;
                if (g_initiator_state.nullFrames >= g_initiator_state.nullFramesMax)
                {
                    log("Consequetive Null Frame Maximum has been reached");
                    g_initiator_state.sectorcount = g_initiator_state.sectors_done;
                    g_initiator_state.sectorcount_all = g_initiator_state.sectors_done;
                }
            }
            else
            {
                if (g_initiator_state.nullFrames)
                    log("End of Null Frames");
                g_initiator_state.nullFrames = 0;
            }
        }
    }
    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        platform_poll();

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
            debuglog("------ STATUS: ", tmp);
        }
    }

    scsiHostPhyRelease();

    return status == 0 && g_initiator_transfer.all_ok;
}

// Decodes Sense keys and sense codes and prints the corresponding error message to the log file
bool Log_Error(uint8_t sense_key, uint16_t sense_code)
{
    char str[200];
    strcpy(str, "Encountered Error! Sense key: ");

    switch (sense_key)
    {

    case 0x0:
        strcat(str, "No Sense");
        break;
    case 0x1:
        strcat(str, "Recovered Error");
        break;
    case 0x2:
        strcat(str, "Not Ready");
        break;
    case 0x3:
        strcat(str, "Medium Error");
        break;
    case 0x4:
        strcat(str, "Hardware Error");
        break;
    case 0x5:
        strcat(str, "Illegal Request");
        break;
    case 0x6:
        strcat(str, "Unit Attention");
        break;
    case 0x7:
        strcat(str, "Data Protect");
        break;
    case 0x8:
        strcat(str, "Blank Check");
        break;
    case 0x9:
        strcat(str, "Vendor-Specific");
        break;
    case 0xA:
        strcat(str, "Copy Aborted");
        break;
    case 0xB:
        strcat(str, "Aborted Command");
        break;
    case 0xC:
        strcat(str, "Equal (now obsolete)");
        break;
    case 0xD:
        strcat(str, "Volume Overflow");
        break;
    case 0xE:
        strcat(str, "Miscompare");
        break;
    default:
        strcat(str, "Unknown Sense Code");
        break;
    }

    strcat(str, ", Code: ");

    switch (sense_code)
    {

    case 0x0000:
        strcat(str, "No additional sense information");
        break;
    case 0x0001:
        strcat(str, "Filemark detected");
        break;
    case 0x0002:
        strcat(str, "End of partition/medium detected");
        break;
    case 0x0003:
        strcat(str, "Setmark detected");
        break;
    case 0x0004:
        strcat(str, "Beginning of partition/medium detected");
        break;
    case 0x0005:
        strcat(str, "End of data detected");
        break;
    case 0x0006:
        strcat(str, "I/O process termination");
        break;
    case 0x0011:
        strcat(str, "Play operation in progress");
        break;
    case 0x0012:
        strcat(str, "Play operation paused");
        break;
    case 0x0013:
        strcat(str, "Play operation successfully completed");
        break;
    case 0x0014:
        strcat(str, "Play operation stopped due to error");
        break;
    case 0x0015:
        strcat(str, "No current audio status to return");
        break;
    case 0x0016:
        strcat(str, "Operation in progress");
        break;
    case 0x0017:
        strcat(str, "Cleaning requested");
        break;
    case 0x0100:
        strcat(str, "Mechanical positioning or changer error");
        break;
    case 0x0200:
        strcat(str, "No seek complete");
        break;
    case 0x0300:
        strcat(str, "Peripheral device write fault");
        break;
    case 0x0301:
        strcat(str, "No write current");
        break;
    case 0x0302:
        strcat(str, "Excessive write errors");
        break;
    case 0x0400:
        strcat(str, "Logical unit not ready, cause not reportable");
        break;
    case 0x0401:
        strcat(str, "Logical unit not ready, in process of becoming ready");
        break;
    case 0x0402:
        strcat(str, "Logical unit not ready, initializing command required");
        break;
    case 0x0403:
        strcat(str, "Logical unit not ready, manual intervention required");
        break;
    case 0x0404:
        strcat(str, "Logical unit not ready, format in progress");
        break;
    case 0x0407:
        strcat(str, "Logical unit not ready, operation in progress");
        break;
    case 0x0408:
        strcat(str, "Logical unit not ready, long write in progress");
        break;
    case 0x0409:
        strcat(str, "Logical unit not ready, self-test in progress");
        break;
    case 0x0500:
        strcat(str, "Logical unit does not respond to selection");
        break;
    case 0x0501:
        strcat(str, "Media load - Eject failed");
        break;
    case 0x0600:
        strcat(str, "No reference position found");
        break;
    case 0x0700:
        strcat(str, "Multiple peripheral devices selected");
        break;
    case 0x0800:
        strcat(str, "Logical unit communication failure");
        break;
    case 0x0801:
        strcat(str, "Logical unit communication time-out");
        break;
    case 0x0802:
        strcat(str, "Logical unit communication parity error");
        break;
    case 0x0803:
        strcat(str, "Logical unit communication CRC error (Ultra-DMA/32)");
        break;
    case 0x0804:
        strcat(str, "Unreachable copy target");
        break;
    case 0x0900:
        strcat(str, "Track following error");
        break;
    case 0x0901:
        strcat(str, "Tracking servo failure");
        break;
    case 0x0902:
        strcat(str, "Focus servo failure");
        break;
    case 0x0903:
        strcat(str, "Spindle servo failure");
        break;
    case 0x0904:
        strcat(str, "Head select fault");
        break;
    case 0x0A00:
        strcat(str, "Error log overflow");
        break;
    case 0x0B00:
        strcat(str, "Warning");
        break;
    case 0x0B01:
        strcat(str, "Warning, specified temperature exceeded");
        break;
    case 0x0B02:
        strcat(str, "Warning, enclosure degraded");
        break;
    case 0x0C00:
        strcat(str, "Write error");
        break;
    case 0x0C01:
        strcat(str, "Write error, recovered with auto reallocation");
        break;
    case 0x0C02:
        strcat(str, "Write error, auto reallocation failed");
        break;
    case 0x0C03:
        strcat(str, "Write error, recommend reassignment");
        break;
    case 0x0C04:
        strcat(str, "Compression check miscompare error");
        break;
    case 0x0C05:
        strcat(str, "Data expansion occurred during compression");
        break;
    case 0x0C06:
        strcat(str, "Block not compressible");
        break;
    case 0x0C07:
        strcat(str, "Write error, recovery needed");
        break;
    case 0x0C08:
        strcat(str, "Write error, recovery failed");
        break;
    case 0x0C09:
        strcat(str, "Write error, loss of streaming");
        break;
    case 0x0C0A:
        strcat(str, "Write error, padding blocks added");
        break;
    case 0x1000:
        strcat(str, "ID, CRC or ECC error");
        break;
    case 0x1100:
        strcat(str, "Unrecovered read error");
        break;
    case 0x1101:
        strcat(str, "Read retries exhausted");
        break;
    case 0x1102:
        strcat(str, "Error too long to correct");
        break;
    case 0x1103:
        strcat(str, "Multiple read errors");
        break;
    case 0x1104:
        strcat(str, "Unrecovered read error - auto reallocate failed");
        break;
    case 0x1105:
        strcat(str, "L-EC uncorrectable error");
        break;
    case 0x1106:
        strcat(str, "CIRC unrecovered error");
        break;
    case 0x1107:
        strcat(str, "Re-synchronization error");
        break;
    case 0x1108:
        strcat(str, "Incomplete block read");
        break;
    case 0x1109:
        strcat(str, "No gap found");
        break;
    case 0x110A:
        strcat(str, "Miscorrected error");
        break;
    case 0x110B:
        strcat(str, "Unrecovered read error - recommend reassignment");
        break;
    case 0x110C:
        strcat(str, "Unrecovered read error - recommend rewrite the data");
        break;
    case 0x110D:
        strcat(str, "De-compression CRC error");
        break;
    case 0x110E:
        strcat(str, "Cannot decompress using declared algorithm");
        break;
    case 0x110F:
        strcat(str, "Error reading UPC/EAN number");
        break;
    case 0x1110:
        strcat(str, "Error reading ISRC number");
        break;
    case 0x1111:
        strcat(str, "Read error, loss of streaming");
        break;
    case 0x1200:
        strcat(str, "Address mark not found for ID field");
        break;
    case 0x1300:
        strcat(str, "Address mark not found for data field");
        break;
    case 0x1400:
        strcat(str, "Recorded entity not found");
        break;
    case 0x1401:
        strcat(str, "Record not found");
        break;
    case 0x1402:
        strcat(str, "Filemark or setmark not found");
        break;
    case 0x1403:
        strcat(str, "End of data not found");
        break;
    case 0x1404:
        strcat(str, "Block sequence error");
        break;
    case 0x1405:
        strcat(str, "Record not found - recommend reassignment");
        break;
    case 0x1406:
        strcat(str, "Record not found - data auto-reallocated");
        break;
    case 0x1500:
        strcat(str, "Random positioning error");
        break;
    case 0x1501:
        strcat(str, "Mechanical positioning or changer error");
        break;
    case 0x1502:
        strcat(str, "Positioning error detected by read of medium");
        break;
    case 0x1600:
        strcat(str, "Data synchronization mark error");
        break;
    case 0x1601:
        strcat(str, "Data sync error - data rewritten");
        break;
    case 0x1602:
        strcat(str, "Data sync error - recommend rewrite");
        break;
    case 0x1603:
        strcat(str, "Data sync error - data auto-reallocated");
        break;
    case 0x1604:
        strcat(str, "Data sync error - recommend reassignment");
        break;
    case 0x1700:
        strcat(str, "Recovered data with no error correction applied");
        break;
    case 0x1701:
        strcat(str, "Recovered data with retries");
        break;
    case 0x1702:
        strcat(str, "Recovered data with positive head offset");
        break;
    case 0x1703:
        strcat(str, "Recovered data with negative head offset");
        break;
    case 0x1704:
        strcat(str, "Recovered data with retries and/or CIRC applied");
        break;
    case 0x1705:
        strcat(str, "Recovered data using previous sector ID");
        break;
    case 0x1706:
        strcat(str, "Recovered data without ECC, data auto-reallocated");
        break;
    case 0x1707:
        strcat(str, "Recovered data without ECC, recommend reassignment");
        break;
    case 0x1708:
        strcat(str, "Recovered data without ECC, recommend rewrite");
        break;
    case 0x1709:
        strcat(str, "Recivered data without ECC, data rewritten");
        break;
    case 0x1800:
        strcat(str, "Recovered data with error correction applied");
        break;
    case 0x1801:
        strcat(str, "Recovered data with error correction & retries applied");
        break;
    case 0x1802:
        strcat(str, "Recovered data, the data was auto-reallocated");
        break;
    case 0x1803:
        strcat(str, "Recovered data with CIRC");
        break;
    case 0x1804:
        strcat(str, "Recovered data with L-EC");
        break;
    case 0x1805:
        strcat(str, "Recovered data, recommend reassignment");
        break;
    case 0x1806:
        strcat(str, "Recovered data, recommend rewrite");
        break;
    case 0x1807:
        strcat(str, "Recovered data with ECC, data rewritten");
        break;
    case 0x1808:
        strcat(str, "Recovered data with linking");
        break;
    case 0x1900:
        strcat(str, "Defect list error");
        break;
    case 0x1901:
        strcat(str, "Defect list not available");
        break;
    case 0x1902:
        strcat(str, "Defect list error in primary list");
        break;
    case 0x1903:
        strcat(str, "Defect list error in grown list");
        break;
    case 0x1A00:
        strcat(str, "Parameter list length error");
        break;
    case 0x1B00:
        strcat(str, "Synchronous data transfer error");
        break;
    case 0x1C00:
        strcat(str, "Defect list not found");
        break;
    case 0x1C01:
        strcat(str, "Primary defect list not found");
        break;
    case 0x1C02:
        strcat(str, "Grown defect list not found");
        break;
    case 0x1D00:
        strcat(str, "Miscompare during verify operation");
        break;
    case 0x1E00:
        strcat(str, "Recovered ID with ECC correction");
        break;
    case 0x1F00:
        strcat(str, "Partial defect list transfer");
        break;
    case 0x2000:
        strcat(str, "Invalid command operation code");
        break;
    case 0x2100:
        strcat(str, "Logical block address out of range");
        break;
    case 0x2101:
        strcat(str, "Invalid element address");
        break;
    case 0x2102:
        strcat(str, "Invalid address for write");
        break;
    case 0x2200:
        strcat(str, "Illegal function");
        break;
    case 0x2400:
        strcat(str, "Invalid field in CDB");
        break;
    case 0x2401:
        strcat(str, "CDB decryption error");
        break;
    case 0x2500:
        strcat(str, "Logical unit not supported");
        break;
    case 0x2600:
        strcat(str, "Invalid field in parameter list");
        break;
    case 0x2601:
        strcat(str, "Parameter not supported");
        break;
    case 0x2602:
        strcat(str, "Parameter value invalid");
        break;
    case 0x2603:
        strcat(str, "Threshold parameters not supported");
        break;
    case 0x2604:
        strcat(str, "Invalid release of active persistent reservation");
        break;
    case 0x2605:
        strcat(str, "Data decryption error");
        break;
    case 0x2606:
        strcat(str, "Too many target descriptors");
        break;
    case 0x2607:
        strcat(str, "Unsupported target descriptor type code");
        break;
    case 0x2608:
        strcat(str, "Too many segment descriptors");
        break;
    case 0x2609:
        strcat(str, "Unsupported segment descriptor type code");
        break;
    case 0x260A:
        strcat(str, "Unexpected inexact segment");
        break;
    case 0x260B:
        strcat(str, "Inline data length exceeded");
        break;
    case 0x260C:
        strcat(str, "Invalid operation for copy source or destination");
        break;
    case 0x260D:
        strcat(str, "Copy segment granularity violation");
        break;
    case 0x2700:
        strcat(str, "Write protected");
        break;
    case 0x2701:
        strcat(str, "Hardware write protected");
        break;
    case 0x2702:
        strcat(str, "Logical unit software write protected");
        break;
    case 0x2703:
        strcat(str, "Associated write protect");
        break;
    case 0x2704:
        strcat(str, "Persistent write protect");
        break;
    case 0x2705:
        strcat(str, "Permanent write protect");
        break;
    case 0x2800:
        strcat(str, "Not ready to ready transition, medium may have changed");
        break;
    case 0x2801:
        strcat(str, "Import or export element accessed");
        break;
    case 0x2900:
        strcat(str, "Power on, reset or bus device reset occurred");
        break;
    case 0x2901:
        strcat(str, "Power on occured");
        break;
    case 0x2902:
        strcat(str, "SCSI bus reset occurred");
        break;
    case 0x2903:
        strcat(str, "Bus device reset function occurred");
        break;
    case 0x2904:
        strcat(str, "Device internal reset");
        break;
    case 0x2905:
        strcat(str, "Transceiver mode changed to single-ended");
        break;
    case 0x2906:
        strcat(str, "Transceiver mode changed to LVD");
        break;
    case 0x2A00:
        strcat(str, "Parameters changed");
        break;
    case 0x2A01:
        strcat(str, "Mode parameters changed");
        break;
    case 0x2A02:
        strcat(str, "Log parameters changed");
        break;
    case 0x2A03:
        strcat(str, "Reservations preempted");
        break;
    case 0x2A04:
        strcat(str, "Reservations released");
        break;
    case 0x2A05:
        strcat(str, "Registrations preempted");
        break;
    case 0x2B00:
        strcat(str, "Copy cannot execute since host cannot disconnect");
        break;
    case 0x2C00:
        strcat(str, "Command sequence error");
        break;
    case 0x2C01:
        strcat(str, "Too many windows specified");
        break;
    case 0x2C02:
        strcat(str, "Invalid combination of windows specified");
        break;
    case 0x2C03:
        strcat(str, "Current program area is not empty");
        break;
    case 0x2C04:
        strcat(str, "Current program area is empty");
        break;
    case 0x2C05:
        strcat(str, "Persistent prevent conflict");
        break;
    case 0x2D00:
        strcat(str, "Overwrite error on update in place");
        break;
    case 0x2E00:
        strcat(str, "Insufficient time for operation");
        break;
    case 0x2F00:
        strcat(str, "Commands cleared by anther initiator");
        break;
    case 0x3000:
        strcat(str, "Incompatible medium installed");
        break;
    case 0x3001:
        strcat(str, "Cannot read medium, unknown format");
        break;
    case 0x3002:
        strcat(str, "Cannot read medium, incompatible format");
        break;
    case 0x3003:
        strcat(str, "Cleaning cartridge installed");
        break;
    case 0x3004:
        strcat(str, "Cannot write medium, unknown format");
        break;
    case 0x3005:
        strcat(str, "Cannot write medium, incompatible format");
        break;
    case 0x3006:
        strcat(str, "Cannot format medium, incompatible medium");
        break;
    case 0x3007:
        strcat(str, "Cleaning failure");
        break;
    case 0x3008:
        strcat(str, "Cannot write, application code mismatch");
        break;
    case 0x3009:
        strcat(str, "Current session not fixated for append");
        break;
    case 0x3100:
        strcat(str, "Medium format corrupted");
        break;
    case 0x3101:
        strcat(str, "Format command failed");
        break;
    case 0x3102:
        strcat(str, "Zoned formatting failed due to spare linking");
        break;
    case 0x3200:
        strcat(str, "No defect spare location available");
        break;
    case 0x3201:
        strcat(str, "Defect list update failure");
        break;
    case 0x3300:
        strcat(str, "Tape length error");
        break;
    case 0x3400:
        strcat(str, "Enclosure failure");
        break;
    case 0x3500:
        strcat(str, "Enclosure services failure");
        break;
    case 0x3501:
        strcat(str, "Unsupported enclosure function");
        break;
    case 0x3502:
        strcat(str, "Enclosure services unavailable");
        break;
    case 0x3503:
        strcat(str, "Enclosure services transfer failure");
        break;
    case 0x3504:
        strcat(str, "Enclosure services transfer refused");
        break;
    case 0x3600:
        strcat(str, "Ribbon, ink, or toner failure");
        break;
    case 0x3700:
        strcat(str, "Rounded parameter");
        break;
    case 0x3800:
        strcat(str, "Event status notification");
        break;
    case 0x3802:
        strcat(str, "ESN - Power management class event");
        break;
    case 0x3804:
        strcat(str, "ESN - Media class event");
        break;
    case 0x3806:
        strcat(str, "ESN - Device busy class event");
        break;
    case 0x3900:
        strcat(str, "Saving parameters not supported");
        break;
    case 0x3A00:
        strcat(str, "Medium not present");
        break;
    case 0x3A01:
        strcat(str, "Medium not present, tray closed");
        break;
    case 0x3A02:
        strcat(str, "Medium not present, tray open");
        break;
    case 0x3A03:
        strcat(str, "Medium not present, loadable");
        break;
    case 0x3A04:
        strcat(str, "Medium not present, medium auxiliary memory accessible");
        break;
    case 0x3B00:
        strcat(str, "Sequential positioning error");
        break;
    case 0x3B01:
        strcat(str, "Tape position error at beginning of medium");
        break;
    case 0x3B02:
        strcat(str, "Tape position error at end of medium");
        break;
    case 0x3B03:
        strcat(str, "Tape or electronic vertical forms unit not ready");
        break;
    case 0x3B04:
        strcat(str, "Slew failure");
        break;
    case 0x3B05:
        strcat(str, "Paper jam");
        break;
    case 0x3B06:
        strcat(str, "Failed to sense top-of-form");
        break;
    case 0x3B07:
        strcat(str, "Failed to sense bottom-of-form");
        break;
    case 0x3B08:
        strcat(str, "Reposition error");
        break;
    case 0x3B09:
        strcat(str, "Read past end of medium");
        break;
    case 0x3B0A:
        strcat(str, "Read past beginning of medium");
        break;
    case 0x3B0B:
        strcat(str, "Position past end of medium");
        break;
    case 0x3B0C:
        strcat(str, "Position past beginning of medium");
        break;
    case 0x3B0D:
        strcat(str, "Medium destination element full");
        break;
    case 0x3B0E:
        strcat(str, "Medium source element empty");
        break;
    case 0x3B0F:
        strcat(str, "End of medium reached");
        break;
    case 0x3B11:
        strcat(str, "Medium magazine not accessible");
        break;
    case 0x3B12:
        strcat(str, "Medium magazine removed");
        break;
    case 0x3B13:
        strcat(str, "Medium magazine inserted");
        break;
    case 0x3B14:
        strcat(str, "Medium magazine locked");
        break;
    case 0x3B15:
        strcat(str, "Medium magazine unlocked");
        break;
    case 0x3B16:
        strcat(str, "Mechanical positioning or changer error");
        break;
    case 0x3D00:
        strcat(str, "Invalid bits in identify message");
        break;
    case 0x3E00:
        strcat(str, "Logical unit has not self-configured yet");
        break;
    case 0x3E01:
        strcat(str, "Logical unit failure");
        break;
    case 0x3E02:
        strcat(str, "Timeout on logical unit");
        break;
    case 0x3E03:
        strcat(str, "Logical unit failed self-test");
        break;
    case 0x3E04:
        strcat(str, "Logical unit unable to update self-test log");
        break;
    case 0x3F00:
        strcat(str, "Target operating conditions have changed");
        break;
    case 0x3F01:
        strcat(str, "Microcode has been changed");
        break;
    case 0x3F02:
        strcat(str, "Changed operating definition");
        break;
    case 0x3F03:
        strcat(str, "Inquiry data has changed");
        break;
    case 0x3F04:
        strcat(str, "Component device attached");
        break;
    case 0x3F05:
        strcat(str, "Device identifier changed");
        break;
    case 0x3F06:
        strcat(str, "Redundancy group created or modified");
        break;
    case 0x3F07:
        strcat(str, "Redundancy group deleted");
        break;
    case 0x3F08:
        strcat(str, "Spare created or modified");
        break;
    case 0x3F09:
        strcat(str, "Spare deleted");
        break;
    case 0x3F0A:
        strcat(str, "Volume set created or modified");
        break;
    case 0x3F0B:
        strcat(str, "Volume set deleted");
        break;
    case 0x3F0C:
        strcat(str, "Volume set deassigned");
        break;
    case 0x3F0D:
        strcat(str, "Volume set reassigned");
        break;
    case 0x3F0E:
        strcat(str, "Reported LUNs data has changed");
        break;
    case 0x3F10:
        strcat(str, "Medium loadable");
        break;
    case 0x3F11:
        strcat(str, "Medium auxiliary memory accessible");
        break;
    case 0x4000:
        strcat(str, "RAM failure");
        break;
    case 0x4100:
        strcat(str, "Data path failure");
        break;
    case 0x4200:
        strcat(str, "Power-on or self-test failure");
        break;
    case 0x4300:
        strcat(str, "Message error");
        break;
    case 0x4400:
        strcat(str, "Internal target failure");
        break;
    case 0x4500:
        strcat(str, "Select or reselect failure");
        break;
    case 0x4600:
        strcat(str, "Unseccessful soft reset");
        break;
    case 0x4700:
        strcat(str, "SCSI Parity error");
        break;
    case 0x4701:
        strcat(str, "Data phase CRC error detected");
        break;
    case 0x4702:
        strcat(str, "SCSI parity error detected during ST data phase");
        break;
    case 0x4703:
        strcat(str, "Information unit CRC error detected");
        break;
    case 0x4704:
        strcat(str, "Async information protection error detected");
        break;
    case 0x4800:
        strcat(str, "Initiator detected error message received");
        break;
    case 0x4900:
        strcat(str, "Invalid message error");
        break;
    case 0x4A00:
        strcat(str, "Command phase error");
        break;
    case 0x4B00:
        strcat(str, "Data phase error");
        break;
    case 0x4C00:
        strcat(str, "Logical unit failed self-configuration");
        break;
    case 0x4E00:
        strcat(str, "Overlapped commands attempted");
        break;
    case 0x5000:
        strcat(str, "Write append error");
        break;
    case 0x5001:
        strcat(str, "Write append position error");
        break;
    case 0x5002:
        strcat(str, "Position error related to timing");
        break;
    case 0x5100:
        strcat(str, "Erase failure");
        break;
    case 0x5300:
        strcat(str, "Media load or eject failed");
        break;
    case 0x5301:
        strcat(str, "Unload tape failure");
        break;
    case 0x5302:
        strcat(str, "Medium removal prevented");
        break;
    case 0x5400:
        strcat(str, "SCSI to host system interface failure");
        break;
    case 0x5500:
        strcat(str, "System Resource failure");
        break;
    case 0x5501:
        strcat(str, "System Buffer full");
        break;
    case 0x5502:
        strcat(str, "Insufficient reservation resources");
        break;
    case 0x5503:
        strcat(str, "Insufficient resources");
        break;
    case 0x5504:
        strcat(str, "Insufficient registration resources");
        break;
    case 0x5700:
        strcat(str, "Unable to recover table of contents");
        break;
    case 0x5800:
        strcat(str, "Generation does not exist");
        break;
    case 0x5900:
        strcat(str, "Updated block read");
        break;
    case 0x5A00:
        strcat(str, "Operator request or state change input (UNSPECIFIED)");
        break;
    case 0x5A01:
        strcat(str, "Operator medium removal request");
        break;
    case 0x5A02:
        strcat(str, "Operator selected write protect");
        break;
    case 0x5A03:
        strcat(str, "Operator selected write permit");
        break;
    case 0x5B00:
        strcat(str, "Log exception");
        break;
    case 0x5B01:
        strcat(str, "Threshold condition met");
        break;
    case 0x5B02:
        strcat(str, "Log counter at maximum");
        break;
    case 0x5B03:
        strcat(str, "Log list codes exhausted");
        break;
    case 0x5C00:
        strcat(str, "RPL status change");
        break;
    case 0x5C01:
        strcat(str, "Spindles synchronized");
        break;
    case 0x5C02:
        strcat(str, "Spindle not synchronized");
        break;
    case 0x5D00:
        strcat(str, "Failure prediction threshold exceeded, predicted logical unit failure");
        break;
    case 0x5D01:
        strcat(str, "Failure prediction threshold exceeded, predicted media failure");
        break;
    case 0x5D10:
        strcat(str, "Hardware impending failure - general hard drive failure");
        break;
    case 0x5D11:
        strcat(str, "Hardware impending failure - drive error rate too high");
        break;
    case 0x5D12:
        strcat(str, "Hardware impending failure - data error rate too high");
        break;
    case 0x5D13:
        strcat(str, "Hardware impending failure - seek error rate too high");
        break;
    case 0x5D14:
        strcat(str, "Hardware impending failure - too many block reassigns");
        break;
    case 0x5D15:
        strcat(str, "Hardware impending failure - access times too high");
        break;
    case 0x5D16:
        strcat(str, "Hardware impending failure - start unit times too high");
        break;
    case 0x5D17:
        strcat(str, "Hardware impending failure - channel parametrics");
        break;
    case 0x5D18:
        strcat(str, "Hardware impending failure - controller detected");
        break;
    case 0x5D19:
        strcat(str, "Hardware impending failure - throughput performance");
        break;
    case 0x5D1A:
        strcat(str, "Hardware impending failure - seek time performance");
        break;
    case 0x5D1B:
        strcat(str, "Hardware impending failure - spin-up retry count");
        break;
    case 0x5D1C:
        strcat(str, "Hardware impending failure - drive calibration retry count");
        break;
    case 0x5D20:
        strcat(str, "Controller impending failure - general hard drive failure");
        break;
    case 0x5D21:
        strcat(str, "Controller impending failure - drive error rate too high");
        break;
    case 0x5D22:
        strcat(str, "Controller impending failure - data error rate too high");
        break;
    case 0x5D23:
        strcat(str, "Controller impending failure - seek error rate too high");
        break;
    case 0x5D24:
        strcat(str, "Controller impending failure - too many block reassigns");
        break;
    case 0x5D25:
        strcat(str, "Controller impending failure - access times too high");
        break;
    case 0x5D26:
        strcat(str, "Controller impending failure - start unit times too high");
        break;
    case 0x5D27:
        strcat(str, "Controller impending failure - channel parametrics");
        break;
    case 0x5D28:
        strcat(str, "Controller impending failure - controller detected");
        break;
    case 0x5D29:
        strcat(str, "Controller impending failure - throughput performance");
        break;
    case 0x5D2A:
        strcat(str, "Controller impending failure - seek time performance");
        break;
    case 0x5D2B:
        strcat(str, "Controller impending failure - spin-up retry count");
        break;
    case 0x5D2C:
        strcat(str, "Controller impending failure - drive calibration retry count");
        break;
    case 0x5DFF:
        strcat(str, "Failure prediction threshold exceeded (FALSE)");
        break;
    case 0x5E00:
        strcat(str, "Low power condition on");
        break;
    case 0x5E01:
        strcat(str, "Idle condition activated by timer");
        break;
    case 0x5E02:
        strcat(str, "Standby condition activated by timer");
        break;
    case 0x5E03:
        strcat(str, "Idle condition activated by command");
        break;
    case 0x5E04:
        strcat(str, "Standby condition activated by command");
        break;
    case 0x5E41:
        strcat(str, "Power state change to active");
        break;
    case 0x5E42:
        strcat(str, "Power state change to idle");
        break;
    case 0x5E43:
        strcat(str, "Power state change to standby");
        break;
    case 0x5E45:
        strcat(str, "Power state change to sleep");
        break;
    case 0x5E47:
        strcat(str, "Power state change to device control");
        break;
    case 0x6000:
        strcat(str, "Lamp failure");
        break;
    case 0x6100:
        strcat(str, "Video acquisition error");
        break;
    case 0x6101:
        strcat(str, "Unable to acquire video");
        break;
    case 0x6102:
        strcat(str, "Out of focus");
        break;
    case 0x6200:
        strcat(str, "Scan head positioning error");
        break;
    case 0x6300:
        strcat(str, "End of user area encountered on this track");
        break;
    case 0x6301:
        strcat(str, "Packet does not fit in available space");
        break;
    case 0x6400:
        strcat(str, "Illegal mode for this track or incompatible medium");
        break;
    case 0x6401:
        strcat(str, "Invalid packet size");
        break;
    case 0x6500:
        strcat(str, "Voltage fault");
        break;
    case 0x6600:
        strcat(str, "Automatic document feeder cover up");
        break;
    case 0x6601:
        strcat(str, "Automatic document feeder lift up");
        break;
    case 0x6602:
        strcat(str, "Document jam in automatic document feeder");
        break;
    case 0x6603:
        strcat(str, "Document misfeed in automatic document feeder");
        break;
    case 0x6700:
        strcat(str, "Configuration failure");
        break;
    case 0x6701:
        strcat(str, "Configuration of incapable logical unit");
        break;
    case 0x6702:
        strcat(str, "Add logical unit failed");
        break;
    case 0x6703:
        strcat(str, "Modification of logical unit failed");
        break;
    case 0x6704:
        strcat(str, "Exchange of logical unit failed");
        break;
    case 0x6705:
        strcat(str, "Remove of logical unit failed");
        break;
    case 0x6706:
        strcat(str, "Attachment of logical unit failed");
        break;
    case 0x6707:
        strcat(str, "Creation of logical unit failed");
        break;
    case 0x6800:
        strcat(str, "Logical unit not configured");
        break;
    case 0x6900:
        strcat(str, "Data loss on logical unit");
        break;
    case 0x6901:
        strcat(str, "Multiple logical unit failures");
        break;
    case 0x6902:
        strcat(str, "A parity/data mismatch");
        break;
    case 0x6A00:
        strcat(str, "Informational, refer to log");
        break;
    case 0x6B00:
        strcat(str, "State change has occurred");
        break;
    case 0x6B01:
        strcat(str, "Redundancy level got better");
        break;
    case 0x6B02:
        strcat(str, "Redundancy level got worse");
        break;
    case 0x6C00:
        strcat(str, "Rebuild failure occurred");
        break;
    case 0x6D00:
        strcat(str, "Recalculate failure occurred");
        break;
    case 0x6E00:
        strcat(str, "Command to logical unit failed");
        break;
    case 0x6F00:
        strcat(str, "Copy protection key exchange failure, authentication failure");
        break;
    case 0x6F01:
        strcat(str, "Copy protection key exchange failure, key not present");
        break;
    case 0x6F02:
        strcat(str, "Copy protection key exchange failure, key not established");
        break;
    case 0x6F03:
        strcat(str, "Read of scrambled sector without authentication");
        break;
    case 0x6F04:
        strcat(str, "Media region code is mismatched to logical unit region");
        break;
    case 0x6F05:
        strcat(str, "Drive region must be permanent/Region reset count error");
        break;
    case 0x7100:
        strcat(str, "Decompression exception long algorithm id");
        break;
    case 0x7200:
        strcat(str, "Session fixation error");
        break;
    case 0x7201:
        strcat(str, "Session fixation error writing lead-in");
        break;
    case 0x7202:
        strcat(str, "Session fixation error writing lead-out");
        break;
    case 0x7203:
        strcat(str, "Session fixation error, incomplete track in session");
        break;
    case 0x7204:
        strcat(str, "Empty or partially written reserved track");
        break;
    case 0x7205:
        strcat(str, "No more RZone reservations are allowed");
        break;
    case 0x7300:
        strcat(str, "CD control error");
        break;
    case 0x7301:
        strcat(str, "Power calibration area almost full");
        break;
    case 0x7302:
        strcat(str, "Power calibration area is full");
        break;
    case 0x7303:
        strcat(str, "Power calibration area error");
        break;
    case 0x7304:
        strcat(str, "Program memory area update failure");
        break;
    case 0x7305:
        strcat(str, "Program memory area is full");
        break;
    case 0x7306:
        strcat(str, "Program memory area is (almost) full");
        break;
    case 0xB900:
        strcat(str, "Play operation aborted");
        break;
    case 0xBF00:
        strcat(str, "Loss of streaming");
        break;
    }

    log(str);
    return true;
}

// Changes the SCSI to Audio mode or DATA mode, (data mode is the default)
int scsiSetMode(int Mode, int target_id)
{

    uint8_t mode_setting[12] =
        {0x00, 0x00, 0x10, 0x08, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

    uint8_t command[6] = {0x15, 0, 0, 0, 12, 0};

    if (!(Mode == DATA_MODE || Mode == AUDIO_MODE))
        return -1;

    if (Mode == DATA_MODE)
        mode_setting[4] = 0x13;
    else
        mode_setting[4] = 0x80;

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         NULL, 0,
                                         mode_setting, sizeof(mode_setting));

    if (status == 0)
        log("Mode Set!");
    else
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        log("Set mode on target ", target_id, " failed, sense key ", sense_key);

        Log_Error(sense_key, sense_code);
        return 1;
    }
    return 0;
}

// Unused, supposed to get the sector size for the SCSI, but it didnt work.
int scsiGetBlockLimits(uint32_t *sector_size, int target_id)
{
    uint8_t command[6] = {0x05, 0, 0, 0, 0, 0};
    uint8_t buffer[6];

    int status = scsiInitiatorRunCommand(target_id, command, sizeof(command),
                                         buffer, sizeof(buffer),
                                         NULL, 0);

    if (status != 0)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        Log_Error(sense_key, sense_code);
        return status;
    }

    *sector_size = (uint32_t)buffer[4] << 8 | (uint32_t)buffer[5];

    log("Read Block Limits: ", *sector_size);

    return 0;
}

// Not inplemented
int scsiLocate(uint32_t position, int target_id)
{

    uint8_t command[10] =
        {0x2B,
         0,
         0,
         (uint8_t)(position >> 24),
         (uint8_t)(position >> 16),
         (uint8_t)(position >> 8),
         (uint8_t)(position),
         0,
         0,
         0};

    for (int i = 0; i < 10; i++)
        log("Command[", i, "] = ", command[i]);

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         NULL, 0,
                                         NULL, 0);

    if (status != 0)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        Log_Error(sense_key, sense_code);
        return status;
    }

    return 0;
}

// This function reads the SCSIs position on the data
int scsiReadPosition(uint32_t *position, int target_id)
{
    uint8_t command[10] = {0x34, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t buffer[20];

    int status = scsiInitiatorRunCommand(target_id, command, sizeof(command),
                                         buffer, sizeof(buffer),
                                         NULL, 0);

    *position = (uint32_t)buffer[4] << 24 | (uint32_t)buffer[5] << 16 | (uint32_t)buffer[6] << 8 | (uint32_t)buffer[7];

    return 0;
}

// This function is supposed to obtain the length of the data,
// Not implemented
int scsiGetTapeLength(uint32_t *length, int target_id)
{
    return 0;
}

// DOES NOT WORK
// Used to rerecord frames, unlike data mode, audio mode does not accept an address in the read command,
// We must manually rewind when we get bad data
int scsiGoBackAFrame(int target_id, uint32_t framesize)
{
    uint32_t reversedFrameSize = ~framesize;
    reversedFrameSize++; // Twos complement

    uint8_t command[6] =
        {0x11,
         0,
         (uint8_t)(reversedFrameSize >> 16),
         (uint8_t)(reversedFrameSize >> 8),
         (uint8_t)(reversedFrameSize),
         0};

    int status = scsiInitiatorRunCommand(target_id, command, sizeof(command),
                                         NULL, 0,
                                         NULL, 0,
                                         true);

    if (status != 0)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        Log_Error(sense_key, sense_code);
        return status;
    }

    return 0;
}

// Gets the Game Mode we are in AUDIO or DATA
int scsiGetMode(int *Mode, int target_id)
{
    uint8_t senseResult[12];
    uint8_t command[6] = {0x1a, 0, 0, 0, 12, 0};
    int result = -1;
    *Mode = -1;
    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         senseResult, sizeof(senseResult),
                                         NULL, 0);

    if (status == 0)
    {
        // There is code that checks if TranserCount >= 5 before grabbing result, Not sure where that is found in this implementation
        if (senseResult[4] == 0x80)
        {
            *Mode = AUDIO_MODE;
            result = 0;
        }
        else if (senseResult[4] == 0x13)
        {
            *Mode = DATA_MODE;
            result = 0;
        }
    }
    log("GET MODE Status: ", status);

    if (status != 0)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        log("Get mode failed: ", status, " sense key ", sense_key);
        Log_Error(sense_key, sense_code);
        scsiHostPhyRelease();
        return false;
    }
    return result;
}

//
int scsiRewind(int target_id)
{
    uint8_t command[6] = {0x01, 0, 0, 0, 0, 0};

    int status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0);

    if (status != 0)
    {
        uint8_t sense_key;
        uint16_t sense_code;
        scsiRequestSense(target_id, &sense_key, &sense_code);
        Log_Error(sense_key, sense_code);
        return status;
    }
    return status;
}

#endif
