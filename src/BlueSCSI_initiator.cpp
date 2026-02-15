/**
 * This file is originally part of ZuluSCSI adopted for BlueSCSI
 *
 * BlueSCSI - Copyright (c) 2024 Eric Helgeson, Androda
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 *
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 *
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/


/*
 * Main program for initiator mode.
 */

#include "BlueSCSI_config.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_initiator.h"
#include "BlueSCSI_msc_initiator.h"
#include "BlueSCSI_msc.h"
#include <BlueSCSI_platform.h>
#include <minIni.h>
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

// From BlueSCSI.cpp
extern bool g_sdcard_present;

// Forward declarations
static void parseAudioFrameMetadata(uint8_t *data, uint32_t frame_size);

/*************************************
 * High level initiator mode logic   *
 *************************************/

static struct {
    // Bitmap of all drives that have been imaged
    uint32_t drives_imaged;

    // Configuration from .ini
    uint8_t initiator_id;
    uint8_t max_retry_count;
    bool use_read10; // Always use read10 commands

    // Is imaging a drive in progress, or are we scanning?
    bool imaging;

    // Audio mode for DAT tape drives
    bool audio_mode;
    uint16_t null_frames_max;  // Max consecutive null frames before stopping (can be up to 65535)

    // Information about currently selected drive
    int target_id;
    uint32_t sectorsize;
    uint32_t sectorcount;
    uint32_t sectorcount_all;
    uint32_t sectors_done;
    uint32_t max_sector_per_transfer;
    uint32_t bad_sector_count;
    uint8_t ansi_version;
    uint8_t device_type;

    // Audio mode frame metadata (for DAT tapes)
    uint32_t interpolation_left;
    uint32_t interpolation_right;
    uint32_t interpolation_both;
    uint32_t good_frames;
    uint32_t all_frames;
    uint8_t null_frames;

    // DAT frame metadata - absolute time
    uint8_t frame_abs_time_hour;
    uint8_t frame_abs_time_min;
    uint8_t frame_abs_time_sec;
    uint8_t frame_abs_time_frame;

    // DAT frame metadata - program time
    uint8_t frame_prg_time_hour;
    uint8_t frame_prg_time_min;
    uint8_t frame_prg_time_sec;
    uint8_t frame_prg_time_frame;

    // DAT frame metadata - date/time
    uint8_t frame_datetime_year;
    uint8_t frame_datetime_month;
    uint8_t frame_datetime_day;
    uint8_t frame_datetime_hour;
    uint8_t frame_datetime_min;
    uint8_t frame_datetime_sec;

    // Retry information for sector reads.
    // If a large read fails, retry is done sector-by-sector.
    int retrycount;
    uint32_t failposition;
    bool eject_when_done;
    bool removable;

    uint32_t removable_count[8];

    // Negotiated bus width for targets
    int targetBusWidth[NUM_SCSIID];
    uint32_t start_sector[NUM_SCSIID];

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
        logmsg("InitiatorID set to illegal value in, ", CONFIGFILE, ", defaulting to 7");
        g_initiator_state.initiator_id = 7;
    }
    else
    {
        logmsg("InitiatorID set to ID ", static_cast<int>(g_initiator_state.initiator_id));
    }
    g_initiator_state.max_retry_count = ini_getl("SCSI", "InitiatorMaxRetry", 5, CONFIGFILE);
    g_initiator_state.use_read10 = ini_getbool("SCSI", "InitiatorUseRead10", false, CONFIGFILE);

    // Audio mode configuration for DAT tape drives
    g_initiator_state.audio_mode = ini_getbool("SCSI", "AudioMode", false, CONFIGFILE);
    g_initiator_state.null_frames_max = ini_getl("SCSI", "InitiatorMaxNullFrames", 300, CONFIGFILE);

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
    g_initiator_state.ansi_version = 0;
    g_initiator_state.bad_sector_count = 0;
    g_initiator_state.device_type = SCSI_DEVICE_TYPE_DIRECT_ACCESS;
    g_initiator_state.removable = false;
    g_initiator_state.eject_when_done = false;
    memset(g_initiator_state.removable_count, 0, sizeof(g_initiator_state.removable_count));

    // Initiator start sector override
    char section[6] = "SCSI0";
    char* end = NULL;
    char size_buffer[64];
    uint8_t string_len;
    uint32_t sector_start;
    for (int i = 0; i < NUM_SCSIID; i++) {
        section[4] = '0' + i;
        string_len = ini_gets(section, "InitiatorStartSector", "", size_buffer, sizeof(size_buffer), CONFIGFILE);
        if (string_len > 0) {
            sector_start = strtoul(size_buffer, &end, 10);
            g_initiator_state.start_sector[i] = sector_start;
        } else {
            g_initiator_state.start_sector[i] = 0;
        }
    }

    // Audio mode frame metadata initialization
    g_initiator_state.interpolation_left = 0;
    g_initiator_state.interpolation_right = 0;
    g_initiator_state.interpolation_both = 0;
    g_initiator_state.good_frames = 0;
    g_initiator_state.all_frames = 0;
    g_initiator_state.null_frames = 0;
    g_initiator_state.frame_abs_time_hour = 0;
    g_initiator_state.frame_abs_time_min = 0;
    g_initiator_state.frame_abs_time_sec = 0;
    g_initiator_state.frame_abs_time_frame = 0;
    g_initiator_state.frame_prg_time_hour = 0;
    g_initiator_state.frame_prg_time_min = 0;
    g_initiator_state.frame_prg_time_sec = 0;
    g_initiator_state.frame_prg_time_frame = 0;
    g_initiator_state.frame_datetime_year = 0;
    g_initiator_state.frame_datetime_month = 0;
    g_initiator_state.frame_datetime_day = 0;
    g_initiator_state.frame_datetime_hour = 0;
    g_initiator_state.frame_datetime_min = 0;
    g_initiator_state.frame_datetime_sec = 0;
}

int scsiInitiatorGetOwnID()
{
    return g_initiator_state.initiator_id;
}

// Update progress bar LED during transfers
static void scsiInitiatorUpdateLed()
{
    // Update status indicator, the led blinks every 5 seconds and is on the longer the more data has been transferred
    const int period = 256;
    int phase = (platform_millis() % period);
    int duty = (int64_t)g_initiator_state.sectors_done * period / g_initiator_state.sectorcount;

    // Minimum and maximum time to verify that the blink is visible
    if (duty < 50) duty = 50;
    if (duty > period - 50) duty = period - 50;

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
    uint32_t start = platform_millis();
    while ((uint32_t)(platform_millis() - start) < ms)
    {
        platform_poll();
        platform_delay_ms(1);
    }
}

static int scsiTypeToIniType(int scsi_type, bool removable)
{
    int ini_type = -1;
    switch (scsi_type)
    {
        case SCSI_DEVICE_TYPE_DIRECT_ACCESS:
            ini_type = removable ? S2S_CFG_REMOVABLE : S2S_CFG_FIXED;
            break;
        case 1:
            ini_type = -1; // S2S_CFG_SEQUENTIAL
            break;
        case SCSI_DEVICE_TYPE_CD:
            ini_type = S2S_CFG_OPTICAL;
            break;
        case SCSI_DEVICE_TYPE_MO:
            ini_type = S2S_CFG_MO;
            break;
        default:
            ini_type = -1;
            break;
    }
    return ini_type;
}

// High level logic of the initiator mode
void scsiInitiatorMainLoop()
{
    SCSI_RELEASE_OUTPUTS();
    SCSI_ENABLE_INITIATOR();
    if (g_scsiHostPhyReset)
    {
        logmsg("Executing BUS RESET after aborted command");
        scsiHostPhyReset();
    }

#ifdef PLATFORM_MASS_STORAGE
    if (g_msc_initiator)
    {
        poll_msc_initiator();
        platform_run_msc();
        return;
    }
    else
    {
        bool initiator_msc_mode = ini_getbool("SCSI", "InitiatorMSC", false, CONFIGFILE);
#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
        // If MSC mode is turned on in INI, do it
        // If not, check hardware switch and go with that setting
        if (!initiator_msc_mode) {
            initiator_msc_mode = is_initiator_USB_mode_enabled();
        }
#endif
        if (!g_sdcard_present || initiator_msc_mode)
        {
            // This delay allows the USB serial console to connect immediately to the host
            // It also decreases the delay in callback processing of MSC commands
            int32_t msc_init_delay = ini_getl("SCSI", "InitiatorMSCInitDelay", MSC_INIT_DELAY, CONFIGFILE);
            if (msc_init_delay != MSC_INIT_DELAY)
                logmsg("Initiator init delay set in ", CONFIGFILE ," to ", (int)msc_init_delay, " milliseconds");
            platform_delay_ms(msc_init_delay);

            logmsg("Entering USB MSC initiator mode");
            platform_enter_msc();
            setup_msc_initiator();
            return;
        }
    }
#endif
    if (!g_sdcard_present)
    {
        // Wait for SD card
        return;
    }

    if (!g_initiator_state.imaging)
    {
        // Scan for SCSI drives one at a time
        g_initiator_state.target_id = (g_initiator_state.target_id + 1) % 8;
        g_initiator_state.sectorsize = 0;
        g_initiator_state.sectorcount = 0;
        g_initiator_state.sectors_done = 0;
        g_initiator_state.retrycount = 0;
        g_initiator_state.failposition = 0;
        g_initiator_state.max_sector_per_transfer = 512;
        g_initiator_state.ansi_version = 0;
        g_initiator_state.bad_sector_count = 0;
        g_initiator_state.device_type = SCSI_DEVICE_TYPE_DIRECT_ACCESS;
        g_initiator_state.removable = false;
        g_initiator_state.eject_when_done = false;
        g_initiator_state.use_read10 = false;

        if (!(g_initiator_state.drives_imaged & (1 << g_initiator_state.target_id)))
        {
            logmsg("Scanning SCSI ID ", g_initiator_state.target_id);
            delay_with_poll(1000);

            uint8_t inquiry_data[36] = {0};

            LED_ON();

            bool startstopok =
                scsiTestUnitReady(g_initiator_state.target_id) &&
                scsiStartStopUnit(g_initiator_state.target_id, true);

#if defined(PLATFORM_MAX_BUS_WIDTH) && PLATFORM_MAX_BUS_WIDTH > 0
            if (startstopok)
            {
                // Negotiate bus width
                // This is done before other commands just in case the target
                // happens to be in 16-bit mode. Only commands that have no
                // data phase can be used before this.
                int configBusWidth = ini_getl("SCSI", "InitiatorBusWidth", PLATFORM_MAX_BUS_WIDTH, CONFIGFILE);
                bool busWidthSet = scsiInitiatorSetBusWidth(g_initiator_state.target_id, configBusWidth);
                if (!busWidthSet && ini_haskey("SCSI", "InitiatorBusWidth", CONFIGFILE))
                {
                    logmsg("-- Failed to negotiate ", 8 << configBusWidth, " bit bus width that is forced in .ini file");
                    logmsg("-- Refusing to connect at lower bus width");
                    return;
                }
            }
#endif

            bool readcapok = false;

            // For audio mode (DAT tapes), skip read capacity and use configured frame size
            if (g_initiator_state.audio_mode)
            {
                readcapok = startstopok;
                g_initiator_state.sectorsize = ini_getl("SCSI", "AudioFrameSize", 5822, CONFIGFILE);
                g_initiator_state.sectorcount = 1000000;  // Unknown length, will detect end of data
                logmsg("Audio mode enabled, frame size: ", (int)g_initiator_state.sectorsize);
            }
            else
            {
                readcapok = startstopok &&
                    scsiInitiatorReadCapacity(g_initiator_state.target_id,
                                              &g_initiator_state.sectorcount,
                                              &g_initiator_state.sectorsize);
            }
            dbgmsg("read capacity: ", readcapok ? "OK" : "FAILED");

            bool inquiryok = startstopok &&
                scsiInquiry(g_initiator_state.target_id, inquiry_data);
            dbgmsg("inquiry: ", inquiryok ? "OK" : "FAILED");

            LED_OFF();

            uint64_t total_bytes = 0;
            if (readcapok)
            {
                logmsg("SCSI ID ", g_initiator_state.target_id,
                    " capacity ", (int)g_initiator_state.sectorcount,
                    " sectors x ", (int)g_initiator_state.sectorsize, " bytes");

                g_initiator_state.sectorcount_all = g_initiator_state.sectorcount;

                total_bytes = (uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize;
                logmsg("Drive total size is ", (int)(total_bytes / (1024 * 1024)), " MiB");
                if (total_bytes >= 0xFFFFFFFF && SD.fatType() != FAT_TYPE_EXFAT)
                {
                    // Note: the FAT32 limit is 4 GiB - 1 byte
                    logmsg("Target SCSI ID ", g_initiator_state.target_id, " image size is equal or larger than 4 GiB.");
                    logmsg("This is larger than the max filesize supported by SD card's filesystem");
                    logmsg("Please reformat the SD card with exFAT format to image this target");
                    g_initiator_state.drives_imaged |= 1 << g_initiator_state.target_id;
                    return;
                }
            }
            else if (startstopok)
            {
                logmsg("SCSI ID ", g_initiator_state.target_id, " responds but ReadCapacity command failed");
                logmsg("Possibly SCSI-1 drive? Attempting to read up to 1 GB.");
                g_initiator_state.sectorsize = 512;
                g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 2097152;
                g_initiator_state.max_sector_per_transfer = 128;
            }
            else
            {
#ifndef BLUESCSI_NETWORK
                dbgmsg("Failed to connect to SCSI ID ", g_initiator_state.target_id);
#endif
                g_initiator_state.sectorsize = 0;
                g_initiator_state.sectorcount = g_initiator_state.sectorcount_all = 0;
            }

            char filename_base[12];
            strncpy(filename_base, "HD00_imaged", sizeof(filename_base));
            const char *filename_extension = ".hda";

            if (inquiryok)
            {
                char vendor[9], product[17], revision[5];
                g_initiator_state.device_type=inquiry_data[0] & 0x1f;
                g_initiator_state.ansi_version = inquiry_data[2] & 0x7;
                g_initiator_state.removable = !!(inquiry_data[1] & 0x80);
                g_initiator_state.eject_when_done = g_initiator_state.removable;
                memcpy(vendor, &inquiry_data[8], 8);
                vendor[8]=0;
                memcpy(product, &inquiry_data[16], 16);
                product[16]=0;
                memcpy(revision, &inquiry_data[32], 4);
                revision[4]=0;

                g_initiator_state.use_read10 = scsiInitiatorTestSupportsRead10(g_initiator_state.target_id, g_initiator_state.sectorsize);
                if(!g_initiator_state.use_read10)
                {
                    // READ6 command can transfer up to 256 sectors
                    g_initiator_state.max_sector_per_transfer = 256;
                }

                // Limit sectors per transfer based on buffer size
                uint32_t max_by_buffer = sizeof(scsiDev.data) / g_initiator_state.sectorsize;
                if (max_by_buffer < g_initiator_state.max_sector_per_transfer)
                {
                    g_initiator_state.max_sector_per_transfer = max_by_buffer;
                }

                int ini_type = scsiTypeToIniType(g_initiator_state.device_type, g_initiator_state.removable);
                logmsg("SCSI Version ", (int) g_initiator_state.ansi_version);
                logmsg("[SCSI", g_initiator_state.target_id,"]");
                logmsg("  Vendor = \"", vendor,"\"");
                logmsg("  Product = \"", product,"\"");
                logmsg("  Version = \"", revision,"\"");
                if (ini_type == -1)
                    logmsg("Type = Not Supported, trying direct access");
                else
                    logmsg("  Type = ", ini_type);

                if (g_initiator_state.device_type == SCSI_DEVICE_TYPE_CD)
                {
                    strncpy(filename_base, "CD00_imaged", sizeof(filename_base));
                    filename_extension = ".iso";
                }
                else if (g_initiator_state.device_type == SCSI_DEVICE_TYPE_MO)
                {
                    strncpy(filename_base, "MO00_imaged", sizeof(filename_base));
                    filename_extension = ".img";
                }
                else if (g_initiator_state.device_type != SCSI_DEVICE_TYPE_DIRECT_ACCESS)
                {
                    logmsg("Unhandled scsi device type: ", g_initiator_state.device_type, ". Handling it as Direct Access Device.");
                    g_initiator_state.device_type = SCSI_DEVICE_TYPE_DIRECT_ACCESS;
                }

                if (g_initiator_state.device_type == SCSI_DEVICE_TYPE_DIRECT_ACCESS && g_initiator_state.removable)
                {
                    strncpy(filename_base, "RM00_imaged", sizeof(filename_base));
                    filename_extension = ".img";
                }
            }

            if (g_initiator_state.eject_when_done && g_initiator_state.removable_count[g_initiator_state.target_id] == 0)
            {
                g_initiator_state.removable_count[g_initiator_state.target_id] = 1;
            }

            if (g_initiator_state.sectorcount > 0)
            {
                char filename[32] = {0};
                filename_base[2] += g_initiator_state.target_id;
                if (g_initiator_state.eject_when_done)
                {
                    auto removable_count = g_initiator_state.removable_count[g_initiator_state.target_id];
                    snprintf(filename, sizeof(filename), "%s(%lu)%s",filename_base, removable_count, filename_extension);
                }
                else
                {
                    snprintf(filename, sizeof(filename), "%s%s", filename_base, filename_extension);
                }
                static int handling = -1;
                if (handling == -1)
                {
                    handling = ini_getl("SCSI", "InitiatorImageHandling", INITIATOR_IMAGE_INCREMENT_IF_EXISTS, CONFIGFILE);
                }
                // Stop if a file already exists
                if (handling == INITIATOR_IMAGE_SKIP_IF_EXISTS)
                {
                    if (SD.exists(filename))
                    {
                        logmsg("File, ", filename, ", already exists, InitiatorImageHandling set to stop if file exists.");
                        g_initiator_state.drives_imaged |= (1 << g_initiator_state.target_id);
                        return;
                    }
                }
                // Create a new copy to the file 002-999
                else if (handling == INITIATOR_IMAGE_INCREMENT_IF_EXISTS)
                {
                    for (uint32_t i = 1; i <= 1000; i++)
                    {
                        if (i == 1)
                        {
                            if (SD.exists(filename))
                                continue;
                            break;
                        }
                        else if(i >= 1000)
                        {
                            logmsg("Max images created from SCSI ID ", g_initiator_state.target_id, ", skipping image creation");
                            g_initiator_state.drives_imaged |= (1 << g_initiator_state.target_id);
                            return;
                        }
                        char filename_copy[6] = {0};
                        if (g_initiator_state.eject_when_done)
                        {
                            auto removable_count = g_initiator_state.removable_count[g_initiator_state.target_id];
                            snprintf(filename, sizeof(filename), "%s(%lu)-%03lu%s", filename_base, removable_count, i, filename_extension);
                        }
                        else
                        {
                            snprintf(filename, sizeof(filename), "%s-%03lu%s", filename_base, i, filename_extension);
                        }
                        snprintf(filename_copy, sizeof(filename_copy), "-%03lu", i);
                        if (SD.exists(filename))
                            continue;
                        break;
                    }

                }
                // overwrite file if it exists
                else if (handling == INITIATOR_IMAGE_OVERWRITE_IF_EXISTS)
                {
                    if (SD.exists(filename))
                    {
                        logmsg("File, ",filename, " already exists, InitiatorImageHandling set to overwrite file");
                        SD.remove(filename);
                    }
                }
                // InitiatorImageHandling invalid setting
                else
                {
                    static bool invalid_logged_once = false;
                    if (!invalid_logged_once)
                    {
                        logmsg("InitiatorImageHandling is set to, ", handling, ", which is invalid");
                        invalid_logged_once = true;
                    }
                    return;
                }

                uint64_t sd_card_free_bytes = (uint64_t)SD.vol()->freeClusterCount() * SD.vol()->bytesPerCluster();
                if (sd_card_free_bytes < total_bytes)
                {
                    logmsg("SD Card only has ", (int)(sd_card_free_bytes / (1024 * 1024)),
                           " MiB - not enough free space to image SCSI ID ", g_initiator_state.target_id);
                    g_initiator_state.drives_imaged |= 1 << g_initiator_state.target_id;
                    return;
                }

                g_initiator_state.target_file = SD.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
                if (!g_initiator_state.target_file.isOpen())
                {
                    logmsg("Failed to open file for writing: ", filename);
                    return;
                }

                if (SD.fatType() == FAT_TYPE_EXFAT)
                {
                    // Only preallocate on exFAT, on FAT32 preallocating can result in false garbage data in the
                    // file if write is interrupted.
                    logmsg("Preallocating image file");
                    g_initiator_state.target_file.preAllocate((uint64_t)g_initiator_state.sectorcount * g_initiator_state.sectorsize);
                }

                logmsg("Starting to copy drive data to ", filename);

                // Initialize audio mode if enabled
                if (g_initiator_state.audio_mode)
                {
                    // Reset audio mode statistics
                    g_initiator_state.interpolation_left = 0;
                    g_initiator_state.interpolation_right = 0;
                    g_initiator_state.interpolation_both = 0;
                    g_initiator_state.good_frames = 0;
                    g_initiator_state.all_frames = 0;
                    g_initiator_state.null_frames = 0;

                    // Set tape to audio mode and rewind
                    if (scsiSetMode(SCSI_MODE_AUDIO, g_initiator_state.target_id) != 0)
                    {
                        logmsg("Failed to set audio mode");
                    }

                    int current_mode = -1;
                    scsiGetMode(&current_mode, g_initiator_state.target_id);
                    if (current_mode == SCSI_MODE_AUDIO)
                        logmsg("Drive is in AUDIO mode");
                    else if (current_mode == SCSI_MODE_DATA)
                        logmsg("Drive is in DATA mode");

                    uint32_t position;
                    scsiReadPosition(&position, g_initiator_state.target_id);
                    logmsg("Starting position: ", (int)position);

                    scsiRewind(g_initiator_state.target_id);
                    g_initiator_state.max_sector_per_transfer = 1;  // Read one frame at a time
                }

                g_initiator_state.imaging = true;

                // Initiator start sector override
                if (g_initiator_state.start_sector[g_initiator_state.target_id] != 0) {
                    g_initiator_state.sectors_done = g_initiator_state.start_sector[g_initiator_state.target_id];
                    logmsg("Using Alternate Start Sector ", g_initiator_state.start_sector[g_initiator_state.target_id],
                        " For SCSI ID ", g_initiator_state.target_id);
                }
            }
        }
    }
    else
    {
        // Copy sectors from SCSI drive to file
        if (g_initiator_state.sectors_done >= g_initiator_state.sectorcount)
        {
            scsiStartStopUnit(g_initiator_state.target_id, false);
            logmsg("Finished imaging drive with id ", g_initiator_state.target_id);
            LED_OFF();

            if (g_initiator_state.sectorcount != g_initiator_state.sectorcount_all)
            {
                logmsg("NOTE: Image size was limited to first 4 GiB due to SD card filesystem limit");
                logmsg("Please reformat the SD card with exFAT format to image this drive fully");
            }

            if(g_initiator_state.bad_sector_count != 0)
            {
                logmsg("NOTE: There were ",  (int) g_initiator_state.bad_sector_count, " bad sectors that could not be read off this drive.");
            }

            // Report audio mode statistics
            if (g_initiator_state.audio_mode)
            {
                if (g_initiator_state.interpolation_left > 0)
                    logmsg("Audio: ", (int)g_initiator_state.interpolation_left, " left channel interpolations");
                if (g_initiator_state.interpolation_right > 0)
                    logmsg("Audio: ", (int)g_initiator_state.interpolation_right, " right channel interpolations");
                if (g_initiator_state.interpolation_both > 0)
                    logmsg("Audio: ", (int)g_initiator_state.interpolation_both, " both channel interpolations");
                logmsg("Audio: ", (int)g_initiator_state.good_frames, " good frames / ",
                       (int)g_initiator_state.all_frames, " total frames");

                // Truncate file to actual data written
                g_initiator_state.target_file.truncate();
            }

            if (!g_initiator_state.eject_when_done)
            {
                logmsg("Marking SCSI ID, ", g_initiator_state.target_id, ", as imaged, wont ask it again.");
                g_initiator_state.drives_imaged |= (1 << g_initiator_state.target_id);
            }

            g_initiator_state.imaging = false;
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

        uint32_t time_start = platform_millis();
        bool status = scsiInitiatorReadDataToFile(g_initiator_state.target_id,
            g_initiator_state.sectors_done, numtoread, g_initiator_state.sectorsize,
            g_initiator_state.target_file);

        if (!status)
        {
            logmsg("Failed to transfer ", numtoread, " sectors starting at ", (int)g_initiator_state.sectors_done);

            if (g_initiator_state.retrycount < g_initiator_state.max_retry_count)
            {
                logmsg("Retrying.. ", g_initiator_state.retrycount + 1, "/", (int) g_initiator_state.max_retry_count);
                delay_with_poll(200);
                // This reset causes some drives to hang and seems to have no effect if left off.
                // scsiHostPhyReset();
                delay_with_poll(200);

                g_initiator_state.retrycount++;
                g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);

                if (g_initiator_state.retrycount > 1 && numtoread > 1)
                {
                    logmsg("Multiple failures, retrying sector-by-sector");
                    g_initiator_state.failposition = g_initiator_state.sectors_done + numtoread;
                }
            }
            else
            {
                logmsg("Retry limit exceeded, skipping one sector");
                g_initiator_state.retrycount = 0;
                g_initiator_state.sectors_done++;
                g_initiator_state.bad_sector_count++;
                g_initiator_state.target_file.seek((uint64_t)g_initiator_state.sectors_done * g_initiator_state.sectorsize);
            }
        }
        else
        {
            g_initiator_state.retrycount = 0;
            g_initiator_state.sectors_done += numtoread;
            g_initiator_state.target_file.flush();

            int speed_kbps = numtoread * g_initiator_state.sectorsize / (platform_millis() - time_start);
            if (g_initiator_state.audio_mode)
            {
                // Audio mode: show timestamp and interpolation info
                logmsg("Audio frame ", (int)g_initiator_state.sectors_done,
                       " ABS: ", (int)g_initiator_state.frame_abs_time_hour, ":",
                       (int)g_initiator_state.frame_abs_time_min, ":",
                       (int)g_initiator_state.frame_abs_time_sec, ":",
                       (int)g_initiator_state.frame_abs_time_frame,
                       " PRG: ", (int)g_initiator_state.frame_prg_time_hour, ":",
                       (int)g_initiator_state.frame_prg_time_min, ":",
                       (int)g_initiator_state.frame_prg_time_sec,
                       " interp[L:", (int)g_initiator_state.interpolation_left,
                       " R:", (int)g_initiator_state.interpolation_right,
                       " B:", (int)g_initiator_state.interpolation_both, "]");
            }
            else
            {
                logmsg("SCSI read succeeded, sectors done: ",
                      (int)g_initiator_state.sectors_done, " / ", (int)g_initiator_state.sectorcount,
                      " speed ", speed_kbps, " kB/s - ",
                      (int)(100 * (int64_t)g_initiator_state.sectors_done / g_initiator_state.sectorcount), "%");
            }
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
                            bool returnDataPhase, uint32_t timeout)
{

    if (!scsiHostPhySelect(target_id, g_initiator_state.initiator_id))
    {
#ifndef BLUESCSI_NETWORK
        dbgmsg("------ Target ", target_id, " did not respond");
#endif
        scsiHostPhyRelease();
        return -1;
    }

    SCSI_PHASE phase;
    int status = -1;
    uint32_t start = platform_millis();
    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        // If explicit timeout is specified, prevent watchdog from triggering too early.
        if ((uint32_t)(platform_millis() - start) < timeout)
        {
            platform_reset_watchdog();
        }

        platform_poll();

        if (phase == MESSAGE_IN)
        {
            uint8_t msg = 0;
            scsiHostRead(&msg, 1);

            if (msg == MSG_COMMAND_COMPLETE)
            {
                break;
            }
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
                logmsg("DATA_IN phase but no data to receive!");
                status = -3;
                break;
            }

            scsiHostSetBusWidth(g_initiator_state.targetBusWidth[target_id]);
            uint32_t readCount = scsiHostRead(bufIn, bufInLen);
            scsiHostSetBusWidth(0);
            if (readCount != bufInLen)
            {
                logmsg("scsiHostRead failed, tried to read ", (int)bufInLen, " bytes, got ", (int)readCount);
                status = -2;
                break;
            }
        }
        else if (phase == DATA_OUT)
        {
            if (returnDataPhase) return 0;
            if (bufOutLen == 0)
            {
                logmsg("DATA_OUT phase but no data to send!");
                status = -3;
                break;
            }

            scsiHostSetBusWidth(g_initiator_state.targetBusWidth[target_id]);
            uint32_t writeCount = scsiHostWrite(bufOut, bufOutLen);
            scsiHostSetBusWidth(0);
            if (writeCount != bufOutLen)
            {
                logmsg("scsiHostWrite failed, was writing ", bytearray(bufOut, bufOutLen), " return value ", (int)writeCount);
                status = -2;
                break;
            }
        }
        else if (phase == STATUS)
        {
            uint8_t tmp = -1;
            scsiHostRead(&tmp, 1);
            status = tmp;
#ifndef BLUESCSI_NETWORK
            dbgmsg("------ STATUS: ", tmp);
#endif
        }
    }

    scsiHostWaitBusFree();

    return status;
}

bool scsiInitiatorTestSupportsRead10(int target_id, uint32_t sectorsize)
{
    if (ini_haskey("SCSI", "InitiatorUseRead10", CONFIGFILE))
    {
        return ini_getbool("SCSI", "InitiatorUseRead10", false, CONFIGFILE);
    }

    uint8_t command[10] = {0x28, 0x00, 0, 0, 0, 0, 0, 0, 1, 0}; // READ10, LBA 0, 1 sector
    int status = scsiInitiatorRunCommand(target_id, command, sizeof(command),
        scsiDev.data, sectorsize, NULL, 0);

    if (status == 0)
    {
        dbgmsg("Target supports READ10 command");
        return true;
    }
    else
    {
        dbgmsg("Target does not support READ10 command");
        return false;
    }
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
        scsiLogInitiatorCommandFailure("READ CAPACITY", target_id, status, sense_key);
        return false;
    }
    else
    {
        *sectorcount = *sectorsize = 0;
        return false;
    }
}

// Execute REQUEST SENSE command to get more information about error status
bool scsiRequestSense(int target_id, uint8_t *sense_key, uint8_t *sense_asc, uint8_t *sense_ascq)
{
    uint8_t command[6] = {0x03, 0, 0, 0, 18, 0};
    uint8_t response[18] = {0};

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0);

    logmsg("RequestSense response: ", bytearray(response, 18),
        " sense_key ", (int)(response[2] & 0xF),
        " asc ", response[12], " ascq ", response[13]);

    if (sense_key) *sense_key = response[2] & 0xF;
    if (sense_asc) *sense_asc = response[12];
    if (sense_ascq) *sense_ascq = response[13];

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
        if(g_initiator_state.eject_when_done)
        {
            logmsg("Ejecting media on SCSI ID: ", target_id);
            g_initiator_state.removable_count[g_initiator_state.target_id]++;
            command[4] = 0b00000010; // eject(6), stop(7).
        }
    }

    uint32_t timeout = 60000; // Some drives can take long to initialize
    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         response, sizeof(response),
                                         NULL, 0, false, timeout);

    if (status == 2)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);
        scsiLogInitiatorCommandFailure("START STOP UNIT", target_id, status, sense_key);

        if (sense_key == NOT_READY)
        {
            dbgmsg("--- Device reports NOT_READY, running STOP to attempt restart");
            // Some devices will only leave NOT_READY state after they have been
            // commanded to stop state first.
            platform_delay_ms(1000);
            uint8_t cmd_stop[6] = {0x1B, 0x1, 0, 0, 0, 0};
            scsiInitiatorRunCommand(target_id,
                                    cmd_stop, sizeof(cmd_stop),
                                    response, sizeof(response),
                                    NULL, 0);
        }
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

            if (sense_key == UNIT_ATTENTION)
            {
                uint8_t inquiry[36];
                dbgmsg("Target ", target_id, " reports UNIT_ATTENTION, running INQUIRY");
                scsiInquiry(target_id, inquiry);
            }
            else if (sense_key == NOT_READY)
            {
                dbgmsg("Target ", target_id, " reports NOT_READY, running STARTSTOPUNIT");
                scsiStartStopUnit(target_id, true);
            }
        }
        else
        {
            dbgmsg("Target ", target_id, " TEST UNIT READY response: ", status);
        }
    }

    return false;
}

int scsiInitiatorMessage(int target_id,
    const uint8_t *msgOut, size_t msgOutLen,
    uint8_t *msgIn, size_t msgInBufSize, size_t *msgInLen,
    uint32_t timeout)
{
    uint8_t command[6] = {0x00, 0, 0, 0, 0, 0};

    scsiHostPhySetATN(true);

    if (!scsiHostPhySelect(target_id, g_initiator_state.initiator_id))
    {
        dbgmsg("------ Target ", target_id, " did not respond");
        scsiHostPhyRelease();
        return -1;
    }

    size_t dummy;
    if (!msgInLen) msgInLen = &dummy;
    *msgInLen = 0;

    size_t msgOutSent = 0;

    SCSI_PHASE phase;
    int status = -1;
    uint32_t start = platform_millis();
    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        // If explicit timeout is specified, prevent watchdog from triggering too early.
        if ((uint32_t)(platform_millis() - start) < timeout)
        {
            platform_reset_watchdog();
        }

        platform_poll();

        if (phase == MESSAGE_IN)
        {
            uint8_t msg = 0;
            scsiHostRead(&msg, 1);

            if (*msgInLen < msgInBufSize)
            {
                msgIn[*msgInLen] = msg;
                *msgInLen += 1;
            }

            if (status != -1 && msg == MSG_COMMAND_COMPLETE)
            {
                break;
            }
        }
        else if (phase == MESSAGE_OUT)
        {
            if (msgOutSent < msgOutLen)
            {
                scsiHostWrite(&msgOut[msgOutSent++], 1);
                if (msgOutSent >= msgOutLen)
                {
                    // End of MESSAGE_OUT phase
                    // Note that target may switch to MESSAGE_IN earlier than this
                    scsiHostPhySetATN(false);
                }
            }
        }
        else if (phase == COMMAND)
        {
            scsiHostWrite(command, sizeof(command));
        }
        else if (phase == STATUS)
        {
            uint8_t tmp = -1;
            scsiHostRead(&tmp, 1);
            status = tmp;
            dbgmsg("------ STATUS: ", tmp);
        }
    }

    scsiHostWaitBusFree();

    return status;
}

bool scsiInitiatorResetBusConfig(int target_id)
{
    uint8_t msgOut[] = {0x80, // Identify
        0x01, 0x03, 0x01, 0x00, 0x00, // Disable synchronous mode
        0x01, 0x02, 0x03, 0x00  // 8-bit mode
    };

    g_initiator_state.targetBusWidth[target_id] = 0;

    int status = scsiInitiatorMessage(target_id, msgOut, sizeof(msgOut), nullptr, 0, nullptr);
    return status == 0;
}

#if !defined(PLATFORM_MAX_BUS_WIDTH) || PLATFORM_MAX_BUS_WIDTH == 0
bool scsiInitiatorSetBusWidth(int target_id, int busWidth)
{
    return false;
}
#else
bool scsiInitiatorSetBusWidth(int target_id, int busWidth)
{
    uint8_t msgOut[] = {0x80, // Identify
        0x01, 0x02, 0x03, (uint8_t)busWidth  // Bus width
    };

    uint8_t msgIn[16] = {0};
    size_t msgInLen = 0;

    dbgmsg("---- Negotiating bus width = ", (uint8_t)busWidth);
    int status = scsiInitiatorMessage(target_id, msgOut, sizeof(msgOut), msgIn, sizeof(msgIn), &msgInLen);
    if (status != 0)
    {
        scsiInitiatorResetBusConfig(target_id);
        return false;
    }

    // Parse response message
    int agreedMode = -1;
    size_t parsed = 0;
    while (parsed < msgInLen)
    {
        uint8_t msgByte = msgIn[parsed++];
        if (msgByte == 0x01)
        {
            // Extended message
            uint8_t extLen = msgIn[parsed++];
            uint8_t *extMsg = &msgIn[parsed];
            parsed += extLen;

            if (extMsg[0] == 0x03)
            {
                dbgmsg("-- Target bus width response: ", extMsg[1]);
                agreedMode = extMsg[1];
            }
        }
        else if ((msgByte & 0xF0) == 0x20)
        {
            // Two-byte message, ignore
            parsed++;
        }
    }

    if (agreedMode < 0)
    {
        logmsg("-- Target did not respond to bus width negotiation, reverting to 8-bit");
        scsiInitiatorResetBusConfig(target_id);
        return false;
    }
    else if (agreedMode == busWidth)
    {
        dbgmsg("-- Negotiated bus width ", 8 << busWidth, " bits, testing with Inquiry command");
        g_initiator_state.targetBusWidth[target_id] = busWidth;
        uint8_t inquiryData[36];
        if (!scsiInquiry(target_id, inquiryData))
        {
            logmsg("-- Bus width test failed, reverting to 8-bit");
            scsiInitiatorResetBusConfig(target_id);
            return false;
        }
        else
        {
            logmsg("-- Successfully negotiated ", 8 << busWidth, " bit bus mode");
            return true;
        }
    }
    else
    {
        logmsg("-- Target refused wide bus request, reverting to 8-bit");
        scsiInitiatorResetBusConfig(target_id);
        return false;
    }
}
#endif

// This uses callbacks to run SD and SCSI transfers in parallel
static struct {
    uint32_t bytes_sd; // Number of bytes that have been transferred on SD card side
    uint32_t bytes_sd_scheduled; // Number of bytes scheduled for transfer on SD card side
    uint32_t bytes_scsi; // Number of bytes that have been scheduled for transfer on SCSI side
    uint32_t bytes_scsi_done; // Number of bytes that have been transferred on SCSI side

    uint32_t bytes_per_sector;
    uint8_t target_id;
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

        if (len == 0) {
            return;
        }

        scsiHostSetBusWidth(g_initiator_state.targetBusWidth[g_initiator_transfer.target_id]);
        // dbgmsg("SCSI read ", (int)start, " + ", (int)len, ", sd ready cnt ", (int)sd_ready_cnt, " ", (int)bytes_complete, ", scsi done ", (int)g_initiator_transfer.bytes_scsi_done);
        if (scsiHostRead(&scsiDev.data[start], len) != len)
        {
            logmsg("Read failed at byte ", (int)g_initiator_transfer.bytes_scsi_done);
            g_initiator_transfer.all_ok = false;
        }
        scsiHostSetBusWidth(0);
        g_initiator_transfer.bytes_scsi_done += len;
    }
}

static void scsiInitiatorWriteDataToSd(FsFile &file, bool use_callback)
{
    // Figure out longest continuous block in buffer
    uint32_t bufsize = sizeof(scsiDev.data);
    uint32_t start = g_initiator_transfer.bytes_sd % bufsize;
    uint32_t len = g_initiator_transfer.bytes_scsi_done - g_initiator_transfer.bytes_sd;
    if (start + len > bufsize) {
        len = bufsize - start;
    }

    // Try to do writes in multiples that align to both SCSI sectors and SD card sectors.
    // SD cards use 512-byte sectors, so writes should be 512-byte aligned for performance.
    // LCM(512, 520) = 33280 bytes = 64 SCSI sectors = 65 SD sectors.
    uint32_t bytesPerSector = g_initiator_transfer.bytes_per_sector;
    // Calculate GCD of bytesPerSector and 512 using binary GCD
    uint32_t a = bytesPerSector, b = 512;
    while (b != 0) { uint32_t t = b; b = a % b; a = t; }
    uint32_t gcd = a;
    uint32_t lcm_alignment = (bytesPerSector / gcd) * 512;
    if (len >= lcm_alignment)
    {
        len -= len % lcm_alignment;
    }
    else if (len >= bytesPerSector)
    {
        // Not enough for LCM alignment, but write complete sectors
        len -= len % bytesPerSector;
    }

    // Start writing to SD card and simultaneously reading more from SCSI bus
    uint8_t *buf = &scsiDev.data[start];
    // dbgmsg("SD write ", (int)start, " + ", (int)len);

    if (use_callback)
    {
        platform_set_sd_callback(&initiatorReadSDCallback, buf);
    }

    g_initiator_transfer.bytes_sd_scheduled = g_initiator_transfer.bytes_sd + len;
    if (file.write(buf, len) != len)
    {
        logmsg("scsiInitiatorReadDataToFile: SD card write failed");
        g_initiator_transfer.all_ok = false;
    }
    platform_set_sd_callback(NULL, NULL);
    g_initiator_transfer.bytes_sd += len;
}

bool scsiInitiatorReadDataToFile(int target_id, uint32_t start_sector, uint32_t sectorcount, uint32_t sectorsize,
                                 FsFile &file)
{
    int status = -1;

    if (g_initiator_state.audio_mode)
    {
        // Audio mode for tape drives: READ command specifies byte count, not LBA/sector
        // The transfer length is in bytes for sequential access devices
        uint32_t byte_count = sectorsize;
        uint8_t command[6] = {0x08,
            0x00,
            (uint8_t)(byte_count >> 16),
            (uint8_t)(byte_count >> 8),
            (uint8_t)(byte_count),
            0x00
        };

        // Start executing command, return in data phase
        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
    }
    // Read6 command supports 21 bit LBA - max of 0x1FFFFF
    // ref: https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 134
    else if (!g_initiator_state.use_read10 && (start_sector < 0x1FFFFF && sectorcount <= 256))
    {
        // Use READ6 command for compatibility with old SCSI1 drives
        // Note that even with SCSI1 drives we have no choice but to use READ10 if the drive
        // size is larger than 1 GB, as the sector number wouldn't fit in the command.
        uint8_t command[6] = {0x08,
            (uint8_t)(start_sector >> 16),
            (uint8_t)(start_sector >> 8),
            (uint8_t)start_sector,
            (uint8_t)sectorcount,
            0x00
        };

        // Start executing command, return in data phase
        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
    }
    else
    {
        // Use READ10 command for larger number of blocks
        uint8_t command[10] = {0x28, 0x00,
            (uint8_t)(start_sector >> 24), (uint8_t)(start_sector >> 16),
            (uint8_t)(start_sector >> 8), (uint8_t)start_sector,
            0x00,
            (uint8_t)(sectorcount >> 8), (uint8_t)(sectorcount),
            0x00
        };

        // Start executing command, return in data phase
        status = scsiInitiatorRunCommand(target_id, command, sizeof(command), NULL, 0, NULL, 0, true);
    }


    if (status != 0)
    {
        uint8_t sense_key, asc, ascq;
        scsiRequestSense(target_id, &sense_key, &asc, &ascq);

        // In audio mode, check for end of data condition
        if (g_initiator_state.audio_mode && sense_key == 0x08 && asc == 0x00 && ascq == 0x05)
        {
            // Blank check / End of data detected - this is normal for tape end
            logmsg("End of data detected on tape");
            g_initiator_state.sectorcount = g_initiator_state.sectors_done;
            g_initiator_state.sectorcount_all = g_initiator_state.sectors_done;
            scsiHostPhyRelease();
            return true;
        }

        scsiLogInitiatorCommandFailure("scsiInitiatorReadDataToFile command phase", target_id, status, sense_key);
        if (g_initiator_state.audio_mode)
        {
            scsiLogSenseError(sense_key, asc, ascq);
        }
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
    g_initiator_transfer.target_id = target_id;

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
        logmsg("SCSI read from sector ", (int)start_sector, " was incomplete: expected ",
             (int)g_initiator_transfer.bytes_scsi, " got ", (int)g_initiator_transfer.bytes_sd, " bytes");
        g_initiator_transfer.all_ok = false;
    }
    else if (g_initiator_state.audio_mode)
    {
        // Parse DAT audio frame metadata for timestamp and interpolation info
        parseAudioFrameMetadata(scsiDev.data, sectorsize);
    }

    while ((phase = (SCSI_PHASE)scsiHostPhyGetPhase()) != BUS_FREE)
    {
        platform_poll();

        if (phase == MESSAGE_IN)
        {
            uint8_t msg = 0;
            scsiHostRead(&msg, 1);

            if (msg == MSG_COMMAND_COMPLETE)
            {
                break;
            }
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
            dbgmsg("------ STATUS: ", tmp);
        }
    }

    scsiHostWaitBusFree();

    if (!g_initiator_transfer.all_ok)
    {
        dbgmsg("scsiInitiatorReadDataToFile: Incomplete transfer");
        return false;
    }
    else if (status == 2)
    {
        uint8_t sense_key;
        scsiRequestSense(target_id, &sense_key);

        if (sense_key == RECOVERED_ERROR)
        {
            dbgmsg("scsiInitiatorReadDataToFile: RECOVERED_ERROR at ", (int)start_sector);
            return true;
        }
        else if (sense_key == UNIT_ATTENTION)
        {
            dbgmsg("scsiInitiatorReadDataToFile: UNIT_ATTENTION");
            return true;
        }
        else
        {
            scsiLogInitiatorCommandFailure("scsiInitiatorReadDataToFile data phase", target_id, status, sense_key);
            return false;
        }
    }
    else
    {
        return status == 0;
    }
}

// ============================================================
// Tape drive / DAT audio mode functions
// ============================================================

// Helper to convert BCD to decimal
static uint8_t bcdToDec(uint8_t bcd)
{
    return (10 * ((bcd & 0xF0) >> 4)) + (bcd & 0x0F);
}

#ifdef UNIT_TEST
// Test accessor for bcdToDec
uint8_t test_bcdToDec(uint8_t bcd)
{
    return bcdToDec(bcd);
}
#endif

// Log detailed SCSI sense error information
void scsiLogSenseError(uint8_t sense_key, uint8_t asc, uint8_t ascq)
{
    const char *sense_key_str = "Unknown";
    switch (sense_key)
    {
        case 0x0: sense_key_str = "No Sense"; break;
        case 0x1: sense_key_str = "Recovered Error"; break;
        case 0x2: sense_key_str = "Not Ready"; break;
        case 0x3: sense_key_str = "Medium Error"; break;
        case 0x4: sense_key_str = "Hardware Error"; break;
        case 0x5: sense_key_str = "Illegal Request"; break;
        case 0x6: sense_key_str = "Unit Attention"; break;
        case 0x7: sense_key_str = "Data Protect"; break;
        case 0x8: sense_key_str = "Blank Check"; break;
        case 0x9: sense_key_str = "Vendor-Specific"; break;
        case 0xA: sense_key_str = "Copy Aborted"; break;
        case 0xB: sense_key_str = "Aborted Command"; break;
        case 0xD: sense_key_str = "Volume Overflow"; break;
        case 0xE: sense_key_str = "Miscompare"; break;
    }

    uint16_t sense_code = ((uint16_t)asc << 8) | ascq;
    const char *code_str = "Unknown code";

    // Common sense codes
    switch (sense_code)
    {
        case 0x0000: code_str = "No additional sense information"; break;
        case 0x0001: code_str = "Filemark detected"; break;
        case 0x0002: code_str = "End of partition/medium detected"; break;
        case 0x0003: code_str = "Setmark detected"; break;
        case 0x0004: code_str = "Beginning of partition/medium detected"; break;
        case 0x0005: code_str = "End of data detected"; break;
        case 0x0006: code_str = "I/O process termination"; break;
        case 0x0400: code_str = "LU not ready, cause not reportable"; break;
        case 0x0401: code_str = "LU not ready, becoming ready"; break;
        case 0x0402: code_str = "LU not ready, init command required"; break;
        case 0x0403: code_str = "LU not ready, manual intervention required"; break;
        case 0x1100: code_str = "Unrecovered read error"; break;
        case 0x1400: code_str = "Recorded entity not found"; break;
        case 0x2000: code_str = "Invalid command operation code"; break;
        case 0x2100: code_str = "LBA out of range"; break;
        case 0x2400: code_str = "Invalid field in CDB"; break;
        case 0x2500: code_str = "LU not supported"; break;
        case 0x2600: code_str = "Invalid field in parameter list"; break;
        case 0x2800: code_str = "Not ready to ready transition"; break;
        case 0x2900: code_str = "Power on, reset or bus device reset"; break;
        case 0x3000: code_str = "Incompatible medium installed"; break;
        case 0x3001: code_str = "Cannot read medium, unknown format"; break;
        case 0x3A00: code_str = "Medium not present"; break;
        case 0x3B00: code_str = "Sequential positioning error"; break;
        case 0x3B01: code_str = "Tape position error at beginning of medium"; break;
        case 0x3B02: code_str = "Tape position error at end of medium"; break;
        case 0x3B08: code_str = "Reposition error"; break;
        case 0x3B09: code_str = "Read past end of medium"; break;
        case 0x3B0A: code_str = "Read past beginning of medium"; break;
        case 0x3B0F: code_str = "End of medium reached"; break;
    }

    logmsg("SCSI Error - Sense: ", sense_key_str, " (", (int)sense_key,
           "), Code: ", code_str, " (", (int)asc, "/", (int)ascq, ")");
}

// Set the tape drive mode (SCSI_MODE_DATA or SCSI_MODE_AUDIO)
int scsiSetMode(int mode, int target_id)
{
    // Mode Select command for DAT drives
    // This sets the density code in the block descriptor
    uint8_t mode_data[12] = {
        0x00, 0x00, 0x10, 0x08,  // Header
        0x00, 0x00, 0x00, 0x00,  // Block descriptor
        0x00, 0x00, 0x00, 0x02
    };

    uint8_t command[6] = {0x15, 0x00, 0x00, 0x00, 12, 0x00};

    if (mode != SCSI_MODE_DATA && mode != SCSI_MODE_AUDIO)
        return -1;

    // Set density code based on mode
    if (mode == SCSI_MODE_DATA)
        mode_data[4] = 0x13;  // DDS data mode
    else
        mode_data[4] = 0x80;  // Audio mode

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         NULL, 0,
                                         mode_data, sizeof(mode_data));

    if (status == 0)
    {
        logmsg("Tape mode set to ", (mode == SCSI_MODE_AUDIO) ? "AUDIO" : "DATA");
    }
    else
    {
        uint8_t sense_key, asc, ascq;
        scsiRequestSense(target_id, &sense_key, &asc, &ascq);
        logmsg("Set mode failed on target ", target_id);
        scsiLogSenseError(sense_key, asc, ascq);
        return 1;
    }
    return 0;
}

// Get the current tape drive mode
int scsiGetMode(int *mode, int target_id)
{
    uint8_t sense_data[12] = {0};
    uint8_t command[6] = {0x1A, 0x00, 0x00, 0x00, 12, 0x00};  // Mode Sense

    *mode = -1;
    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         sense_data, sizeof(sense_data),
                                         NULL, 0);

    if (status == 0)
    {
        // Check density code in block descriptor
        if (sense_data[4] == 0x80)
        {
            *mode = SCSI_MODE_AUDIO;
            return 0;
        }
        else if (sense_data[4] == 0x13)
        {
            *mode = SCSI_MODE_DATA;
            return 0;
        }
    }
    else
    {
        uint8_t sense_key, asc, ascq;
        scsiRequestSense(target_id, &sense_key, &asc, &ascq);
        logmsg("Get mode failed on target ", target_id);
        scsiLogSenseError(sense_key, asc, ascq);
    }
    return -1;
}

// Rewind the tape to beginning
int scsiRewind(int target_id)
{
    uint8_t command[6] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         NULL, 0,
                                         NULL, 0,
                                         false, 120000);  // Long timeout for rewind

    if (status != 0)
    {
        uint8_t sense_key, asc, ascq;
        scsiRequestSense(target_id, &sense_key, &asc, &ascq);
        logmsg("Rewind failed on target ", target_id);
        scsiLogSenseError(sense_key, asc, ascq);
        return status;
    }
    logmsg("Tape rewound successfully");
    return 0;
}

// Read the current tape position
int scsiReadPosition(uint32_t *position, int target_id)
{
    uint8_t command[10] = {0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t buffer[20] = {0};

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         buffer, sizeof(buffer),
                                         NULL, 0);

    if (status == 0)
    {
        *position = ((uint32_t)buffer[4] << 24) |
                    ((uint32_t)buffer[5] << 16) |
                    ((uint32_t)buffer[6] << 8) |
                    (uint32_t)buffer[7];
    }
    else
    {
        *position = 0;
    }
    return status;
}

// Seek to a specific position on the tape
int scsiLocate(uint32_t position, int target_id)
{
    uint8_t command[10] = {
        0x2B, 0x00, 0x00,
        (uint8_t)(position >> 24),
        (uint8_t)(position >> 16),
        (uint8_t)(position >> 8),
        (uint8_t)(position),
        0x00, 0x00, 0x00
    };

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         NULL, 0,
                                         NULL, 0,
                                         false, 120000);  // Long timeout for seek

    if (status != 0)
    {
        uint8_t sense_key, asc, ascq;
        scsiRequestSense(target_id, &sense_key, &asc, &ascq);
        logmsg("Locate failed on target ", target_id);
        scsiLogSenseError(sense_key, asc, ascq);
    }
    return status;
}

// Get block size limits for the tape drive
int scsiGetBlockLimits(uint32_t *max_block_size, int target_id)
{
    uint8_t command[6] = {0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t buffer[6] = {0};

    int status = scsiInitiatorRunCommand(target_id,
                                         command, sizeof(command),
                                         buffer, sizeof(buffer),
                                         NULL, 0);

    if (status == 0)
    {
        *max_block_size = ((uint32_t)buffer[1] << 16) |
                          ((uint32_t)buffer[2] << 8) |
                          (uint32_t)buffer[3];
        logmsg("Block limits: max ", (int)*max_block_size);
    }
    else
    {
        uint8_t sense_key, asc, ascq;
        scsiRequestSense(target_id, &sense_key, &asc, &ascq);
        scsiLogSenseError(sense_key, asc, ascq);
        *max_block_size = 0;
    }
    return status;
}

#ifdef UNIT_TEST
// Test accessors for audio mode state
uint32_t test_get_interpolation_left() { return g_initiator_state.interpolation_left; }
uint32_t test_get_interpolation_right() { return g_initiator_state.interpolation_right; }
uint32_t test_get_interpolation_both() { return g_initiator_state.interpolation_both; }
uint32_t test_get_good_frames() { return g_initiator_state.good_frames; }
uint32_t test_get_all_frames() { return g_initiator_state.all_frames; }
uint8_t test_get_null_frames() { return g_initiator_state.null_frames; }
uint16_t test_get_null_frames_max() { return g_initiator_state.null_frames_max; }
uint32_t test_get_sectorcount() { return g_initiator_state.sectorcount; }
uint32_t test_get_sectors_done() { return g_initiator_state.sectors_done; }
bool test_get_audio_mode() { return g_initiator_state.audio_mode; }
uint32_t test_get_sectorsize() { return g_initiator_state.sectorsize; }

// Test accessors for frame metadata
uint8_t test_get_abs_time_hour() { return g_initiator_state.frame_abs_time_hour; }
uint8_t test_get_abs_time_min() { return g_initiator_state.frame_abs_time_min; }
uint8_t test_get_abs_time_sec() { return g_initiator_state.frame_abs_time_sec; }
uint8_t test_get_abs_time_frame() { return g_initiator_state.frame_abs_time_frame; }
uint8_t test_get_prg_time_hour() { return g_initiator_state.frame_prg_time_hour; }
uint8_t test_get_prg_time_min() { return g_initiator_state.frame_prg_time_min; }
uint8_t test_get_prg_time_sec() { return g_initiator_state.frame_prg_time_sec; }
uint8_t test_get_prg_time_frame() { return g_initiator_state.frame_prg_time_frame; }
uint8_t test_get_datetime_year() { return g_initiator_state.frame_datetime_year; }
uint8_t test_get_datetime_month() { return g_initiator_state.frame_datetime_month; }
uint8_t test_get_datetime_day() { return g_initiator_state.frame_datetime_day; }
uint8_t test_get_datetime_hour() { return g_initiator_state.frame_datetime_hour; }
uint8_t test_get_datetime_min() { return g_initiator_state.frame_datetime_min; }
uint8_t test_get_datetime_sec() { return g_initiator_state.frame_datetime_sec; }

// Test setters for audio mode state (for test setup)
void test_set_null_frames_max(uint16_t val) { g_initiator_state.null_frames_max = val; }
void test_set_sectors_done(uint32_t val) { g_initiator_state.sectors_done = val; }
void test_set_audio_mode(bool val) { g_initiator_state.audio_mode = val; }
void test_set_sectorsize(uint32_t val) { g_initiator_state.sectorsize = val; }
void test_set_sectorcount(uint32_t val) { g_initiator_state.sectorcount = val; }
void test_set_use_read10(bool val) { g_initiator_state.use_read10 = val; }
void test_reset_audio_state() {
    g_initiator_state.interpolation_left = 0;
    g_initiator_state.interpolation_right = 0;
    g_initiator_state.interpolation_both = 0;
    g_initiator_state.good_frames = 0;
    g_initiator_state.all_frames = 0;
    g_initiator_state.null_frames = 0;
    g_initiator_state.sectorcount = 1000000;
    g_initiator_state.sectors_done = 0;
}
#endif

// Parse DAT audio frame metadata from the read buffer
// Frame format follows DAT (Digital Audio Tape) subcode specifications
static void parseAudioFrameMetadata(uint8_t *data, uint32_t frame_size)
{
    if (frame_size < 0x16BC)
        return;

    // Extract timestamps from DAT subcode area
    // These offsets are specific to DAT audio frame format
    g_initiator_state.frame_abs_time_hour = bcdToDec(data[0x168B]);
    g_initiator_state.frame_abs_time_min = bcdToDec(data[0x168C]);
    g_initiator_state.frame_abs_time_sec = bcdToDec(data[0x168D]);
    g_initiator_state.frame_abs_time_frame = bcdToDec(data[0x168E]);

    g_initiator_state.frame_prg_time_hour = bcdToDec(data[0x1683]);
    g_initiator_state.frame_prg_time_min = bcdToDec(data[0x1684]);
    g_initiator_state.frame_prg_time_sec = bcdToDec(data[0x1685]);
    g_initiator_state.frame_prg_time_frame = bcdToDec(data[0x1686]);

    g_initiator_state.frame_datetime_year = bcdToDec(data[0x16A1]);
    g_initiator_state.frame_datetime_month = bcdToDec(data[0x16A2]);
    g_initiator_state.frame_datetime_day = bcdToDec(data[0x16A3]);
    g_initiator_state.frame_datetime_hour = bcdToDec(data[0x16A4]);
    g_initiator_state.frame_datetime_min = bcdToDec(data[0x16A5]);
    g_initiator_state.frame_datetime_sec = bcdToDec(data[0x16A6]);

    // Check interpolation flags (indicate data recovery was needed)
    uint8_t interp_flags = data[0x16BB];
    if (interp_flags & 0x40)
        g_initiator_state.interpolation_left++;
    if (interp_flags & 0x20)
        g_initiator_state.interpolation_right++;
    if ((interp_flags & 0x60) == 0x60)
        g_initiator_state.interpolation_both++;
    if ((interp_flags & 0x60) == 0x00)
        g_initiator_state.good_frames++;

    g_initiator_state.all_frames++;

    // Check for null frame (all time codes are zero)
    bool is_null_frame =
        (g_initiator_state.frame_abs_time_hour == 0) &&
        (g_initiator_state.frame_abs_time_min == 0) &&
        (g_initiator_state.frame_abs_time_sec == 0) &&
        (g_initiator_state.frame_prg_time_hour == 0) &&
        (g_initiator_state.frame_prg_time_min == 0) &&
        (g_initiator_state.frame_prg_time_sec == 0) &&
        (g_initiator_state.frame_datetime_hour == 0);

    if (is_null_frame && g_initiator_state.null_frames_max > 0)
    {
        if (g_initiator_state.null_frames == 0)
            logmsg("Start of null frames detected");
        g_initiator_state.null_frames++;

        if (g_initiator_state.null_frames >= g_initiator_state.null_frames_max)
        {
            logmsg("Consecutive null frame maximum reached, ending capture");
            g_initiator_state.sectorcount = g_initiator_state.sectors_done;
            g_initiator_state.sectorcount_all = g_initiator_state.sectors_done;
        }
    }
    else
    {
        if (g_initiator_state.null_frames > 0)
            logmsg("End of null frames");
        g_initiator_state.null_frames = 0;
    }
}

#ifdef UNIT_TEST
// Test accessor for parseAudioFrameMetadata
void test_parseAudioFrameMetadata(uint8_t *data, uint32_t frame_size)
{
    parseAudioFrameMetadata(data, frame_size);
}
#endif

#endif
