// This file implements the main SCSI disk emulation and data streaming.
// It is derived from disk.c in SCSI2SD V6.
//
//    Licensed under GPL v3.
//    Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//    Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
//    Copyright (C) 2022 Rabbit Hole Computing
//    Copyright (C) 2024 Eric Helgeson <erichelgeson@gmail.com>

#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_presets.h"
#ifdef ENABLE_AUDIO_OUTPUT
#include "BlueSCSI_audio.h"
#endif
#include "BlueSCSI_cdrom.h"
#include "BlueSCSI_platform_config_hook.h"
#include "ImageBackingStore.h"
#include "ROMDrive.h"
#include <minIni.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <SdFat.h>

extern uint8_t sdSpeedClass;
#define SD_SPEED_CLASS_WARN_BELOW 10

extern "C" {
#include <scsi2sd_time.h>
#include <sd.h>
#include <mode.h>
}

#ifndef PLATFORM_MAX_SCSI_SPEED
#define PLATFORM_MAX_SCSI_SPEED S2S_CFG_SPEED_ASYNC_50
#endif

// This can be overridden in platform file to set the size of the transfers
// used when reading from SCSI bus and writing to SD card.
// When SD card access is fast, these are usually better increased.
// If SD card access is roughly same speed as SCSI bus, these can be left at 512
#ifndef PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE
#define PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE 512
#endif

#ifndef PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE
#define PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE 1024
#endif

// Optimal size for the last write in a write request.
// This is often better a bit smaller than PLATFORM_OPTIMAL_SD_WRITE_SIZE
// to reduce the dead time between end of SCSI transfer and finishing of SD write.
#ifndef PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE
#define PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE 512
#endif

// Optimal size for read block from SCSI bus
// For platforms with nonblocking transfer, this can be large.
// For Akai MPC60 compatibility this has to be at least 5120
#ifndef PLATFORM_OPTIMAL_SCSI_READ_BLOCK_SIZE
#ifdef PLATFORM_SCSIPHY_HAS_NONBLOCKING_READ
#define PLATFORM_OPTIMAL_SCSI_READ_BLOCK_SIZE 65536
#else
#define PLATFORM_OPTIMAL_SCSI_READ_BLOCK_SIZE 8192
#endif
#endif

#ifndef PLATFORM_SCSIPHY_HAS_NONBLOCKING_READ
// For platforms that do not have non-blocking read from SCSI bus
void scsiStartRead(uint8_t* data, uint32_t count, int *parityError)
{
    scsiRead(data, count, parityError);
}
void scsiFinishRead(uint8_t* data, uint32_t count, int *parityError)
{
    
}
bool scsiIsReadFinished(const uint8_t *data)
{
    return true;
}
#endif

/************************************************/
/* ROM drive support (in microcontroller flash) */
/************************************************/

// Check if rom drive exists and activate it
bool scsiDiskActivateRomDrive()
{
#ifndef PLATFORM_HAS_ROM_DRIVE
    return false;
#else
    log("");
    log("=== ROM Drive ===");

    uint32_t maxsize = platform_get_romdrive_maxsize() - PLATFORM_ROMDRIVE_PAGE_SIZE;
    log("Platform supports ROM drive up to ", (int)(maxsize / 1024), " kB");

    romdrive_hdr_t hdr = {};
    if (!romDriveCheckPresent(&hdr))
    {
        log("---- ROM drive image not detected");
        return false;
    }

    if (ini_getbool("SCSI", "DisableROMDrive", 0, CONFIGFILE))
    {
        log("---- ROM drive disabled in ini file, not enabling");
        return false;
    }
    else
    {
        debuglog("---- ROM drive enabled");
    }

    long rom_scsi_id = ini_getl("SCSI", "ROMDriveSCSIID", -1, CONFIGFILE);
    if (rom_scsi_id >= 0 && rom_scsi_id <= 7)
    {
        hdr.scsi_id = rom_scsi_id;
        log("---- ROM drive SCSI id overriden in ini file, changed to ", (int)hdr.scsi_id);
    }

    if (s2s_getConfigById(hdr.scsi_id))
    {
        log("---- ROM drive SCSI id ", (int)hdr.scsi_id, " is already in use, not enabling");
        return false;
    }

    log("---- Activating ROM drive, SCSI id ", (int)hdr.scsi_id, " size ", (int)(hdr.imagesize / 1024), " kB");
    bool status = scsiDiskOpenHDDImage("ROM:", hdr.scsi_id, 0, hdr.blocksize, hdr.drivetype);

    if (!status)
    {
        log("---- ROM drive activation failed");
        return false;
    }
    else
    {
        log("---- Activated ROM drive, SCSI id ", (int)hdr.scsi_id, " size ", (int)(hdr.imagesize / 1024), " kB");
        return true;
    }

#endif
}


/***********************/
/* Image configuration */
/***********************/

extern SdFs SD;
SdDevice sdDev = {2, 256 * 1024 * 1024 * 2}; /* For SCSI2SD */

static image_config_t g_DiskImages[S2S_MAX_TARGETS];

void scsiDiskResetImages()
{
    for (int i = 0; i < S2S_MAX_TARGETS; i++)
    {
        g_DiskImages[i].clear();
    }
}

void image_config_t::clear()
{
    static const image_config_t empty; // Statically zero-initialized
    *this = empty;
}

void scsiDiskCloseSDCardImages()
{
    for (int i = 0; i < S2S_MAX_TARGETS; i++)
    {
        if (!g_DiskImages[i].file.isRom())
        {
            g_DiskImages[i].file.close();
        }

        g_DiskImages[i].cuesheetfile.close();
    }
}

// Verify format conformance to SCSI spec:
// - Empty bytes filled with 0x20 (space)
// - Only values 0x20 to 0x7E
// - Left alignment for vendor/product/revision, right alignment for serial.
static void formatDriveInfoField(char *field, int fieldsize, bool align_right)
{
    if (align_right)
    {
        // Right align and trim spaces on either side
        int dst = fieldsize - 1;
        for (int src = fieldsize - 1; src >= 0; src--)
        {
            char c = field[src];
            if (c < 0x20 || c > 0x7E) c = 0x20;
            if (c != 0x20 || dst != fieldsize - 1)
            {
                field[dst--] = c;
            }
        }
        while (dst >= 0)
        {
            field[dst--] = 0x20;
        }
    }
    else
    {
        // Left align, preserve spaces in case config tries to manually right-align
        int dst = 0;
        for (int src = 0; src < fieldsize; src++)
        {
            char c = field[src];
            if (c < 0x20 || c > 0x7E) c = 0x20;
            field[dst++] = c;
        }
        while (dst < fieldsize)
        {
            field[dst++] = 0x20;
        }
    }
}

// remove path and extension from filename
void extractFileName(const char* path, char* output) {

    const char *lastSlash, *lastDot;
    int fileNameLength;

    lastSlash = strrchr(path, '/');
    if (!lastSlash) lastSlash = path;
        else lastSlash++;

    lastDot = strrchr(lastSlash, '.');
    if (lastDot && (lastDot > lastSlash)) {
        fileNameLength = lastDot - lastSlash;
        strncpy(output, lastSlash, fileNameLength);
        output[fileNameLength] = '\0';
    } else {
        strcpy(output, lastSlash);
    }
}

void setNameFromImage(image_config_t &img, const char *filename) {

    char image_name[MAX_FILE_PATH];

    extractFileName(filename, image_name);
    memset(img.vendor, 0, 8);
    strncpy(img.vendor, image_name, 8);
    memset(img.prodId, 0, 8);
    strncpy(img.prodId, image_name+8, 8);
}

// Set default drive vendor / product info after the image file
// is loaded and the device type is known.
static void setDefaultDriveInfo(int target_idx)
{
    image_config_t &img = g_DiskImages[target_idx];

    static const char *driveinfo_fixed[4]     = DRIVEINFO_FIXED;
    static const char *driveinfo_removable[4] = DRIVEINFO_REMOVABLE;
    static const char *driveinfo_optical[4]   = DRIVEINFO_OPTICAL;
    static const char *driveinfo_floppy[4]    = DRIVEINFO_FLOPPY;
    static const char *driveinfo_magopt[4]    = DRIVEINFO_MAGOPT;
    static const char *driveinfo_network[4]   = DRIVEINFO_NETWORK;
    static const char *driveinfo_tape[4]      = DRIVEINFO_TAPE;
    static const char *driveinfo_amigawifi[4]   = DRIVEINFO_AMIGAWIFI;

    static const char *apl_driveinfo_fixed[4]     = APPLE_DRIVEINFO_FIXED;
    static const char *apl_driveinfo_removable[4] = APPLE_DRIVEINFO_REMOVABLE;
    static const char *apl_driveinfo_optical[4]   = APPLE_DRIVEINFO_OPTICAL;
    static const char *apl_driveinfo_floppy[4]    = APPLE_DRIVEINFO_FLOPPY;
    static const char *apl_driveinfo_magopt[4]    = APPLE_DRIVEINFO_MAGOPT;
    static const char *apl_driveinfo_network[4]   = APPLE_DRIVEINFO_NETWORK;    
    static const char *apl_driveinfo_tape[4]      = APPLE_DRIVEINFO_TAPE;

    static const char *iomega_driveinfo_removeable[4] = IOMEGA_DRIVEINFO_ZIP100;

    const char **driveinfo = NULL;

    if (img.quirks == S2S_CFG_QUIRKS_APPLE)
    {
        // Use default drive IDs that are recognized by Apple machines
        switch (img.deviceType)
        {
            case S2S_CFG_FIXED:         driveinfo = apl_driveinfo_fixed; break;
            case S2S_CFG_REMOVEABLE:    driveinfo = apl_driveinfo_removable; break;
            case S2S_CFG_OPTICAL:       driveinfo = apl_driveinfo_optical; break;
            case S2S_CFG_FLOPPY_14MB:   driveinfo = apl_driveinfo_floppy; break;
            case S2S_CFG_MO:            driveinfo = apl_driveinfo_magopt; break;
            case S2S_CFG_NETWORK:       driveinfo = apl_driveinfo_network; break;            
            case S2S_CFG_SEQUENTIAL:    driveinfo = apl_driveinfo_tape; break;
            case S2S_CFG_ZIP100:        driveinfo = iomega_driveinfo_removeable; break;
            case S2S_CFG_AMIGAWIFI:     driveinfo = driveinfo_amigawifi; break; // just incase
            default:                    driveinfo = apl_driveinfo_fixed; break;
        }
    }
    else
    {
        // Generic IDs
        switch (img.deviceType)
        {
            case S2S_CFG_FIXED:         driveinfo = driveinfo_fixed; break;
            case S2S_CFG_REMOVEABLE:    driveinfo = driveinfo_removable; break;
            case S2S_CFG_OPTICAL:       driveinfo = driveinfo_optical; break;
            case S2S_CFG_FLOPPY_14MB:   driveinfo = driveinfo_floppy; break;
            case S2S_CFG_MO:            driveinfo = driveinfo_magopt; break;
            case S2S_CFG_NETWORK:       driveinfo = driveinfo_network; break;
            case S2S_CFG_AMIGAWIFI:     driveinfo = driveinfo_amigawifi; break;
            case S2S_CFG_SEQUENTIAL:    driveinfo = driveinfo_tape; break;
            case S2S_CFG_ZIP100:        driveinfo = iomega_driveinfo_removeable; break;
            default:                    driveinfo = driveinfo_fixed; break;
        }
    }

    if (img.vendor[0] == '\0')
    {
        memset(img.vendor, 0, sizeof(img.vendor));
        strncpy(img.vendor, driveinfo[0], sizeof(img.vendor));
    }

    if (img.prodId[0] == '\0')
    {
        memset(img.prodId, 0, sizeof(img.prodId));
        strncpy(img.prodId, driveinfo[1], sizeof(img.prodId));
    }

    if (img.revision[0] == '\0')
    {
        memset(img.revision, 0, sizeof(img.revision));
        strncpy(img.revision, driveinfo[2], sizeof(img.revision));
    }

    if (img.serial[0] == '\0')
    {
        memset(img.serial, 0, sizeof(img.serial));
        strncpy(img.serial, driveinfo[3], sizeof(img.serial));
    }

    if (img.serial[0] == '\0')
    {
        // Use SD card serial number
        cid_t sd_cid;
        uint32_t sd_sn = 0;
        if (SD.card()->readCID(&sd_cid))
        {
            sd_sn = sd_cid.psn();
        }

        memset(img.serial, 0, sizeof(img.serial));
        const char *nibble = "0123456789ABCDEF";
        img.serial[0] = nibble[(sd_sn >> 28) & 0xF];
        img.serial[1] = nibble[(sd_sn >> 24) & 0xF];
        img.serial[2] = nibble[(sd_sn >> 20) & 0xF];
        img.serial[3] = nibble[(sd_sn >> 16) & 0xF];
        img.serial[4] = nibble[(sd_sn >> 12) & 0xF];
        img.serial[5] = nibble[(sd_sn >>  8) & 0xF];
        img.serial[6] = nibble[(sd_sn >>  4) & 0xF];
        img.serial[7] = nibble[(sd_sn >>  0) & 0xF];
    }

    int rightAlign = img.rightAlignStrings;

    formatDriveInfoField(img.vendor, sizeof(img.vendor), rightAlign);
    formatDriveInfoField(img.prodId, sizeof(img.prodId), rightAlign);
    formatDriveInfoField(img.revision, sizeof(img.revision), rightAlign);
    formatDriveInfoField(img.serial, sizeof(img.serial), true);
}

bool scsiDiskOpenHDDImage(const char *filename, int scsi_id, int scsi_lun, int block_size, S2S_CFG_TYPE type)
{
    image_config_t &img = g_DiskImages[scsi_id];
    img.cuesheetfile.close();
    img.file = ImageBackingStore(filename, block_size);

    if (img.file.isOpen())
    {
        img.bytesPerSector = block_size;
        img.scsiSectors = img.file.size() / block_size;
        img.scsiId = scsi_id | S2S_CFG_TARGET_ENABLED;
        img.sdSectorStart = 0;

        if ((type != S2S_CFG_NETWORK) && (type != S2S_CFG_AMIGAWIFI))
        {
            if (img.scsiSectors == 0)
            {
                log("---- Error: image file ", filename, " is empty");
                img.file.close();
                return false;
            }

            uint32_t sector_begin = 0, sector_end = 0;
            if (img.file.isRom())
            {
                // ROM is always contiguous, no need to log
            }
            else if (!img.file.contiguousRange(&sector_begin, &sector_end))
            {
                log("---- WARNING: file ", filename, " is fragmented, see https://github.com/BlueSCSI/BlueSCSI-v2/wiki/Image-File-Fragmentation");
            }
        }

        if (type == S2S_CFG_OPTICAL)
        {
            log("---- Configuring as CD-ROM drive based on image name");
            img.deviceType = S2S_CFG_OPTICAL;
        }
        else if (type == S2S_CFG_FLOPPY_14MB)
        {
            log("---- Configuring as floppy drive based on image name");
            img.deviceType = S2S_CFG_FLOPPY_14MB;
        }
        else if (type == S2S_CFG_MO)
        {
            log("---- Configuring as magneto-optical based on image name");
            img.deviceType = S2S_CFG_MO;
        }
        else if (type == S2S_CFG_NETWORK)
        {
            if (!platform_network_supported())
            {
                log("---- Error: network not supported on this device, ignoring ", filename);
                img.file.close();
                return false;
            }
            log("---- Configuring as network based on image name");
            img.deviceType = S2S_CFG_NETWORK;
        }
        else if (type == S2S_CFG_AMIGAWIFI)
        {
            if (!platform_network_supported())
            {
                log("---- Error: network not supported on this device, ignoring ", filename);
                img.file.close();
                return false;
            }
            log("---- Configuring as network based on image name");
            img.deviceType = S2S_CFG_AMIGAWIFI;
        }
        else if (type == S2S_CFG_REMOVEABLE)
        {
            log("---- Configuring as removable drive based on image name");
            img.deviceType = S2S_CFG_REMOVEABLE;
        }
        else if (type == S2S_CFG_SEQUENTIAL)
        {
            log("---- Configuring as tape drive based on image name");
            img.deviceType = S2S_CFG_SEQUENTIAL;
        }
        else if (type == S2S_CFG_ZIP100)
        {
            log("---- Configuration as Iomega ZIP100 drive based on image name");
            img.deviceType = S2S_CFG_ZIP100;
            if(img.file.size() != ZIP100_DISC_SIZE)
            {
                log("---- ZIP100 disc (", (int)img.file.size(), " bytes) is not exactly ", ZIP100_DISC_SIZE, " bytes, drive is ignored");
                img.file.close();
                img.clear();
                return false;
            }
        }

        if (img.prefetchbytes != PREFETCH_BUFFER_SIZE)
        {
            log("---- Read prefetch enabled: ", (int)img.prefetchbytes, " bytes");
        }
        else if(img.prefetchbytes == 0)
        {
            log("---- Read prefetch disabled");
        }
        else 
        {
            debuglog("---- Read prefetch enabled: ", (int)img.prefetchbytes, " bytes");
        }
        if (img.name_from_image)
        {
            setNameFromImage(img, filename);
            log("Vendor / product id set from image file name");
        }

        setDefaultDriveInfo(scsi_id);

#ifdef PLATFORM_CONFIG_HOOK
        PLATFORM_CONFIG_HOOK(&img);
#endif

        if (img.deviceType == S2S_CFG_OPTICAL &&
            strncasecmp(filename + strlen(filename) - 4, ".bin", 4) == 0)
        {
            char cuesheetname[MAX_FILE_PATH + 1] = {0};
            strncpy(cuesheetname, filename, strlen(filename) - 4);
            strlcat(cuesheetname, ".cue", sizeof(cuesheetname));
            img.cuesheetfile = SD.open(cuesheetname, O_RDONLY);

            if (img.cuesheetfile.isOpen())
            {
                log("---- Found CD-ROM CUE sheet at ", cuesheetname);
                if (!cdromValidateCueSheet(img))
                {
                    log("---- Failed to parse cue sheet, using as plain binary image");
                    img.cuesheetfile.close();
                }
            }
            else
            {
                log("---- No CUE sheet found at ", cuesheetname, ", using as plain binary image");
            }
        }

        return true;
    }
    else
    {
        log("---- Failed to load image '", filename, "', ignoring");
        return false;
    }
}

static void checkDiskGeometryDivisible(image_config_t &img)
{
    if (!img.geometrywarningprinted)
    {
        uint32_t sectorsPerHeadTrack = img.sectorsPerTrack * img.headsPerCylinder;
        if (img.scsiSectors % sectorsPerHeadTrack != 0)
        {
            debuglog("WARNING: Host used command ", scsiDev.cdb[0],
                     " which is affected by drive geometry. Current settings are ",
                     (int)img.sectorsPerTrack, " sectors x ", (int)img.headsPerCylinder, " heads = ",
                     (int)sectorsPerHeadTrack, " but image size of ", (int)img.scsiSectors,
                     " sectors is not divisible. This can cause error messages in diagnostics tools.");
            img.geometrywarningprinted = true;
        }
    }
}

bool scsiDiskFilenameValid(const char* name)
{
    // Check file extension
    const char *extension = strrchr(name, '.');
    if (extension)
    {
        const char *ignore_exts[] = {
            ".rom_loaded", ".cue",
            NULL
        };
        const char *archive_exts[] = {
            ".tar", ".tgz", ".gz", ".bz2", ".tbz2", ".xz", ".zst", ".z",
            ".zip", ".zipx", ".rar", ".lzh", ".lha", ".lzo", ".lz4", ".arj",
            ".dmg", ".hqx", ".cpt", ".7z", ".s7z",
            NULL
        };

        for (int i = 0; ignore_exts[i]; i++)
        {
            if (strcasecmp(extension, ignore_exts[i]) == 0)
            {
                debuglog("-- Ignoring file ", name);
                return false;
            }
        }
        for (int i = 0; archive_exts[i]; i++)
        {
            if (strcasecmp(extension, archive_exts[i]) == 0)
            {
                log("-- Ignoring compressed file ", name);
                return false;
            }
        }
    }
    if(name[0] == '.')
    {
        debuglog("-- Ignoring hidden file ", name);
        return false;
    }
    return true;
}

// Set target configuration to default values
static void scsiDiskConfigDefaults(int target_idx)
{
    // Get default values from system preset, if any
    char presetName[32];
    ini_gets("SCSI", "System", "", presetName, sizeof(presetName), CONFIGFILE);
    preset_config_t defaults = getSystemPreset(presetName);

    image_config_t &img = g_DiskImages[target_idx];
    img.scsiId = target_idx;
    img.deviceType = S2S_CFG_FIXED;
    img.deviceTypeModifier = defaults.deviceTypeModifier;
    img.sectorsPerTrack = defaults.sectorsPerTrack;
    img.headsPerCylinder = defaults.headsPerCylinder;
    img.bytesPerSector = defaults.bytesPerSector;
    img.quirks = defaults.quirks;
    img.prefetchbytes = defaults.prefetchBytes;
    img.reinsert_on_inquiry = false;
    img.reinsert_after_eject = true;
    memset(img.vendor, 0, sizeof(img.vendor));
    memset(img.prodId, 0, sizeof(img.prodId));
    memset(img.revision, 0, sizeof(img.revision));
    memset(img.serial, 0, sizeof(img.serial));
}

// Load values for target configuration from given section if they exist.
// Otherwise, keep current settings.
static void scsiDiskLoadConfig(int target_idx, const char *section)
{
    image_config_t &img = g_DiskImages[target_idx];
    img.deviceType = ini_getl(section, "Type", img.deviceType, CONFIGFILE);
    img.deviceTypeModifier = ini_getl(section, "TypeModifier", img.deviceTypeModifier, CONFIGFILE);
    img.sectorsPerTrack = ini_getl(section, "SectorsPerTrack", img.sectorsPerTrack, CONFIGFILE);
    img.headsPerCylinder = ini_getl(section, "HeadsPerCylinder", img.headsPerCylinder, CONFIGFILE);
    img.bytesPerSector = ini_getl(section, "BlockSize", img.bytesPerSector, CONFIGFILE);
    img.quirks = ini_getl(section, "Quirks", img.quirks, CONFIGFILE);
    img.rightAlignStrings = ini_getbool(section, "RightAlignStrings", 0, CONFIGFILE);
    img.name_from_image = ini_getbool(section, "NameFromImage", 0, CONFIGFILE);
    img.prefetchbytes = ini_getl(section, "PrefetchBytes", img.prefetchbytes, CONFIGFILE);
    img.reinsert_on_inquiry = ini_getbool(section, "ReinsertCDOnInquiry", img.reinsert_on_inquiry, CONFIGFILE);
    img.reinsert_after_eject = ini_getbool(section, "ReinsertAfterEject", img.reinsert_after_eject, CONFIGFILE);
    img.ejectButton = ini_getl(section, "EjectButton", 0, CONFIGFILE);
#ifdef ENABLE_AUDIO_OUTPUT
    uint16_t vol = ini_getl(section, "CDAVolume", DEFAULT_VOLUME_LEVEL, CONFIGFILE) & 0xFF;
    // Set volume on both channels
    audio_set_volume(target_idx, (vol << 8) | vol);
#endif

    char tmp[32];
    memset(tmp, 0, sizeof(tmp));
    ini_gets(section, "Vendor", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0]) memcpy(img.vendor, tmp, sizeof(img.vendor));

    memset(tmp, 0, sizeof(tmp));
    ini_gets(section, "Product", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0]) memcpy(img.prodId, tmp, sizeof(img.prodId));

    memset(tmp, 0, sizeof(tmp));
    ini_gets(section, "Version", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0]) memcpy(img.revision, tmp, sizeof(img.revision));

    memset(tmp, 0, sizeof(tmp));
    ini_gets(section, "Serial", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0]) memcpy(img.serial, tmp, sizeof(img.serial));

    if (strlen(section) == 5 && strncmp(section, "SCSI", 4) == 0) // allow within target [SCSIx] blocks only
    {
        ini_gets(section, "ImgDir", "", tmp, sizeof(tmp), CONFIGFILE);
        getImgDir(target_idx, tmp, sizeof(tmp));
        if (tmp[0])
        {
            log("-- SCSI", target_idx, " using image directory \'", tmp, "'");
            img.image_directory = true;
        }
        else
        {
            strcpy(tmp, "CDX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
                log("-- SCSI ID: ", target_idx, " using Optical image directory \'", tmp, "'");
                img.deviceType = S2S_CFG_OPTICAL;
                img.image_directory = true;
            }
            strcpy(tmp, "HDX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
                log("-- SCSI ID: ", target_idx, " using Drive image directory \'", tmp, "'");
                img.image_directory = true;
            }
            strcpy(tmp, "FDX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
                log("-- SCSI ID: ", target_idx, " using Floppy image directory \'", tmp, "'");
                img.deviceType = S2S_CFG_FLOPPY_14MB;
                img.image_directory = true;
            }
            strcpy(tmp, "ZPX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
                log("-- SCSI ID: ", target_idx, " using Zip 100 image directory \'", tmp, "'");
                img.deviceType = S2S_CFG_ZIP100;
                img.image_directory = true;
            }
            strcpy(tmp, "MOX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
              log("-- SCSI ID: ", target_idx, " using Magneto-optical image directory \'", tmp, "'");
              img.deviceType = S2S_CFG_MO;
              img.image_directory = true;
            }
            strcpy(tmp, "REX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
              log("-- SCSI ID: ", target_idx, " using Removable image directory \'", tmp, "'");
              img.deviceType = S2S_CFG_REMOVEABLE;
              img.image_directory = true;
            }
            strcpy(tmp, "TPX");
            tmp[2] = '0' + target_idx;
            if(SD.exists(tmp))
            {
              log("-- SCSI ID: ", target_idx, " using Tape image directory \'", tmp, "'");
              img.deviceType = S2S_CFG_SEQUENTIAL;
              img.image_directory = true;
            }
        }
    }
}

// Finds filename with the lowest lexical order _after_ the given filename in
// the given folder. If there is no file after the given one, or if there is
// no current file, this will return the lowest filename encountered.
static int findNextImageAfter(image_config_t &img,
        const char* dirname, const char* filename,
        char* buf, size_t buflen)
{
    FsFile dir;
    if (dirname[0] == '\0')
    {
        log("Image directory name invalid for ID", (img.scsiId & S2S_CFG_TARGET_ID_BITS));
        return 0;
    }
    if (!dir.open(dirname))
    {
        log("Image directory '", dirname, "' couldn't be opened");
    }
    if (!dir.isDir())
    {
        log("Can't find images in '", dirname, "', not a directory");
        dir.close();
        return 0;
    }

    char first_name[MAX_FILE_PATH] = {'\0'};
    char candidate_name[MAX_FILE_PATH] = {'\0'};
    FsFile file;
    while (file.openNext(&dir, O_RDONLY))
    {
        if (file.isDir()) continue;
        if (!file.getName(buf, MAX_FILE_PATH))
        {
            log("Image directory '", dirname, "'had invalid file");
            continue;
        }
        if (!scsiDiskFilenameValid(buf)) continue;

        // keep track of the first item to allow wrapping
        // without having to iterate again
        if (first_name[0] == '\0' || strcasecmp(buf, first_name) < 0)
        {
            strncpy(first_name, buf, sizeof(first_name));
        }

        // discard if no selected name, or if candidate is before (or is) selected
        if (filename[0] == '\0' || strcasecmp(buf, filename) <= 0) continue;

        // if we got this far and the candidate is either 1) not set, or 2) is a
        // lower item than what has been encountered thus far, it is the best choice
        if (candidate_name[0] == '\0' || strcasecmp(buf, candidate_name) < 0)
        {
            strncpy(candidate_name, buf, sizeof(candidate_name));
        }
    }

    if (candidate_name[0] != '\0')
    {
        img.image_index++;
        strncpy(img.current_image, candidate_name, sizeof(img.current_image));
        strncpy(buf, candidate_name, buflen);
        return strlen(candidate_name);
    }
    else if (first_name[0] != '\0')
    {
        img.image_index = 0;
        strncpy(img.current_image, first_name, sizeof(img.current_image));
        strncpy(buf, first_name, buflen);
        return strlen(first_name);
    }
    else
    {
        log("Image directory '", dirname, "' was empty");
        return 0;
    }
}

int scsiDiskGetNextImageName(image_config_t &img, char *buf, size_t buf_len)
{
    int target_idx = img.scsiId & S2S_CFG_TARGET_ID_BITS;

    // sanity check: is provided buffer is long enough to store a filename?
    assert(buf_len >= MAX_FILE_PATH);

    if (img.image_directory)
    {
        // image directory was found during startup
        char dirname[MAX_FILE_PATH];
        int dir_len = getImgDir(target_idx, dirname, sizeof(dirname));
        if (!dir_len)
        {
            // If image_directory set but ImgDir is not look for a well known ImgDir
            if(img.deviceType == S2S_CFG_OPTICAL)
                strcpy(dirname, "CDX");
            else if(img.deviceType == S2S_CFG_ZIP100)
                strcpy(dirname, "ZPX");
            else if(img.deviceType == S2S_CFG_FLOPPY_14MB)
                strcpy(dirname, "FDX");
            else if(img.deviceType == S2S_CFG_MO)
                strcpy(dirname, "MOX");
            else if(img.deviceType == S2S_CFG_REMOVEABLE)
                strcpy(dirname, "REX");
            else if(img.deviceType == S2S_CFG_SEQUENTIAL)
                strcpy(dirname, "TPX");
            else
                strcpy(dirname, "HDX");
            dirname[2] = '0' + target_idx;
            if(!SD.exists(dirname))
            {
                log("ERROR: Looking for ", dirname, " to load images, but was not found.");
                return 0;
            }
        }

        // find the next filename
        char nextname[MAX_FILE_PATH];
        int nextlen = findNextImageAfter(img, dirname, img.current_image, nextname, sizeof(nextname));

        if (nextlen == 0)
        {
            log("Image directory was empty for ID", target_idx);
            return 0;
        }
        else if (buf_len < nextlen + dir_len + 2)
        {
            log("Directory '", dirname, "' and file '", nextname, "' exceed allowed length");
            return 0;
        }
        else
        {
            // construct a return value
            strncpy(buf, dirname, buf_len);
            if (buf[strlen(buf) - 1] != '/') strcat(buf, "/");
            strcat(buf, nextname);
            return dir_len + nextlen;
        }
    }
    else
    {
        img.image_index++;
        if (img.image_index > IMAGE_INDEX_MAX || img.image_index < 0)
        {
            img.image_index = 0;
        }

        int ret = getImg(target_idx, img.image_index, buf, buf_len);
        if (buf[0] != '\0')
        {
            return ret;
        }
        else if (img.image_index > 0)
        {
            // there may be more than one image but we've ran out of new ones
            // wrap back to the first image
            img.image_index = -1;
            return scsiDiskGetNextImageName(img, buf, buf_len);
        }
        else
        {
            // images are not defined in config
            img.image_index = -1;
            return 0;
        }
    }
}

void scsiDiskLoadConfig(int target_idx)
{
    char section[6] = "SCSI0";
    section[4] = '0' + target_idx;

    // Set default settings
    scsiDiskConfigDefaults(target_idx);

    // First load global settings
    scsiDiskLoadConfig(target_idx, "SCSI");

    // Then settings specific to target ID
    scsiDiskLoadConfig(target_idx, section);

    // Check if we have image specified by name
    char filename[MAX_FILE_PATH];
    image_config_t &img = g_DiskImages[target_idx];
    img.image_index = IMAGE_INDEX_MAX;
    if (scsiDiskGetNextImageName(img, filename, sizeof(filename)))
    {
        int blocksize = getBlockSize(filename, target_idx, (img.deviceType == S2S_CFG_OPTICAL) ? 2048 : 512);
        log("-- Opening '", filename, "' for ID: ", target_idx);
        scsiDiskOpenHDDImage(filename, target_idx, 0, blocksize);
    }
}

// Check if we have multiple drive images to cycle when drive is ejected.
bool switchNextImage(image_config_t &img, const char* next_filename)
{
    // Check if we have a next image to load, so that drive is closed next time the host asks.
    char filename[MAX_FILE_PATH];
    int target_idx = img.scsiId & S2S_CFG_TARGET_ID_BITS;
    if (next_filename == nullptr)
    {
        scsiDiskGetNextImageName(img, filename, sizeof(filename));
    }
    else
    {
        strncpy(filename, next_filename, MAX_FILE_PATH);
    }

    if (filename[0] != '\0')
    {
        log("Switching to next image for ID: ", target_idx, ": ", filename);
        img.file.close();
        int block_size = getBlockSize(filename, target_idx, (img.deviceType == S2S_CFG_OPTICAL) ? 2048 : 512);
        bool status = scsiDiskOpenHDDImage(filename, target_idx, 0, block_size);

        if (status)
        {
            if (next_filename != nullptr && img.deviceType == S2S_CFG_OPTICAL)
            {
                // present the drive as ejected until the host queries it again,
                // to make sure host properly detects the media change
                img.ejected = true;
                img.reinsert_after_eject = true;
                img.cdrom_events = 2; // New Media
            }
            return true;
        }
    }
    else
    {
        log("Could not switch to image as provide filename was empty.");
    }

    return false;
}

bool scsiDiskCheckAnyImagesConfigured()
{
    for (int i = 0; i < S2S_MAX_TARGETS; i++)
    {
        if (g_DiskImages[i].file.isOpen() && (g_DiskImages[i].scsiId & S2S_CFG_TARGET_ENABLED))
        {
            return true;
        }
    }

    return false;
}

image_config_t &scsiDiskGetImageConfig(int target_idx)
{
    assert(target_idx >= 0 && target_idx < S2S_MAX_TARGETS);
    return g_DiskImages[target_idx];
}

static void diskEjectAction(uint8_t buttonId)
{
    bool found = false;
    for (uint8_t i = 0; i < S2S_MAX_TARGETS; i++)
    {
        image_config_t &img = g_DiskImages[i];
        if (img.ejectButton == buttonId)
        {
            if (img.deviceType == S2S_CFG_OPTICAL)
            {
                found = true;
                log("Eject button ", (int)buttonId, " pressed, passing to CD drive SCSI", (int)i);
                cdromPerformEject(img);
            }
        }
    }

    if (!found)
    {
        log("Eject button ", (int)buttonId, " pressed, but no drives with EjectButton=", (int)buttonId, " setting found!");
    }
}

uint8_t diskEjectButtonUpdate(bool immediate)
{
    // treat '1' to '0' transitions as eject actions
    static uint8_t previous = 0x00;
    uint8_t bitmask = platform_get_buttons();
    uint8_t ejectors = (previous ^ bitmask) & previous;
    previous = bitmask;

    // defer ejection until the bus is idle
    static uint8_t deferred = 0x00;
    if (!immediate)
    {
        deferred |= ejectors;
        return 0;
    }
    else
    {
        ejectors |= deferred;
        deferred = 0;

        if (ejectors)
        {
            uint8_t mask = 1;
            for (uint8_t i = 0; i < 8; i++)
            {
                if (ejectors & mask) diskEjectAction(i + 1);
                mask = mask << 1;
            }
        }
        return ejectors;
    }
}

bool scsiDiskCheckAnyNetworkDevicesConfigured()
{
    for (int i = 0; i < S2S_MAX_TARGETS; i++)
    {
        if (g_DiskImages[i].file.isOpen() && (g_DiskImages[i].scsiId & S2S_CFG_TARGET_ENABLED) && (g_DiskImages[i].deviceType == S2S_CFG_NETWORK || g_DiskImages[i].deviceType == S2S_CFG_AMIGAWIFI))
        {
            return true;
        }
    }

    return false;
}

/*******************************/
/* Config handling for SCSI2SD */
/*******************************/

extern "C"
void s2s_configInit(S2S_BoardCfg* config)
{
    char tmp[64];

    if (SD.exists(CONFIGFILE)) {
        log("Reading configuration from " CONFIGFILE);
    } else {
        if (SD.exists(CONFIGFILE_BAD)) {
          log("ERROR: Please remove the .txt file extension from the config file: ", CONFIGFILE_BAD);
        }
        log("Config file " CONFIGFILE " not found, using defaults");
    }

    // Get default values from system preset, if any
    ini_gets("SCSI", "System", "", tmp, sizeof(tmp), CONFIGFILE);
    preset_config_t defaults = getSystemPreset(tmp);

    if (defaults.presetName)
    {
        log("Active configuration (using system preset \"", defaults.presetName, "\"):");
    }
    else
    {
        log("Active configuration:");
    }

    memset(config, 0, sizeof(S2S_BoardCfg));
    memcpy(config->magic, "BCFG", 4);
    config->flags = 0;
    config->startupDelay = 0;
    config->selectionDelay = ini_getl("SCSI", "SelectionDelay", defaults.selectionDelay, CONFIGFILE);
    config->flags6 = 0;
    config->scsiSpeed = PLATFORM_MAX_SCSI_SPEED;

    int maxSyncSpeed = ini_getl("SCSI", "MaxSyncSpeed", defaults.maxSyncSpeed, CONFIGFILE);
    if (sdSpeedClass < SD_SPEED_CLASS_WARN_BELOW) {
		log_f("---- WARNING: Your SD Card Speed Class is %d.  Class 10 or better is recommended for best performance.", sdSpeedClass);
    }
    if (maxSyncSpeed < 5 && config->scsiSpeed > S2S_CFG_SPEED_ASYNC_50)
        config->scsiSpeed = S2S_CFG_SPEED_ASYNC_50;
    else if (maxSyncSpeed < 10 && config->scsiSpeed > S2S_CFG_SPEED_SYNC_5)
        config->scsiSpeed = S2S_CFG_SPEED_SYNC_5;

    if ((int)config->selectionDelay == defaults.selectionDelay)
    {
        debuglog("-- SelectionDelay: ", (int)config->selectionDelay);
    }
    else
    {
        log("-- SelectionDelay: ", (int)config->selectionDelay);
    }

    if (ini_getbool("SCSI", "EnableUnitAttention", defaults.enableUnitAttention, CONFIGFILE))
    {
        log("-- EnableUnitAttention is on");
        config->flags |= S2S_CFG_ENABLE_UNIT_ATTENTION;
    }
    else
    {
        debuglog("-- EnableUnitAttention is off");
    }

    if (ini_getbool("SCSI", "EnableSCSI2", defaults.enableSCSI2, CONFIGFILE))
    {
        debuglog("-- EnableSCSI2 is on");
        config->flags |= S2S_CFG_ENABLE_SCSI2;
    }
    else
    {
        log("-- EnableSCSI2 is off");
    }

    if (ini_getbool("SCSI", "EnableSelLatch", defaults.enableSelLatch, CONFIGFILE))
    {
        log("-- EnableSelLatch is on");
        config->flags |= S2S_CFG_ENABLE_SEL_LATCH;
    }
    else
    {
        debuglog("-- EnableSelLatch is off");
    }

    if (ini_getbool("SCSI", "MapLunsToIDs", defaults.mapLunsToIDs, CONFIGFILE))
    {
        log("-- MapLunsToIDs is on");
        config->flags |= S2S_CFG_MAP_LUNS_TO_IDS;
    }
    else
    {
        debuglog("-- MapLunsToIDs is off");
    }

    if (ini_getbool("SCSI", "Debug", 0, CONFIGFILE))
    {
        log("-- Debug is enabled");
        g_scsi_log_mask = ini_getl("SCSI", "DebugLogMask", 0xFF, CONFIGFILE) & 0b11111111;
        if(g_scsi_log_mask != 0xFF)
        {
            log("--- DebugLogMask set to ", g_scsi_log_mask, " only SCSI IDs matching this mask will be logged.");
        }
    }

    if (ini_getbool("SCSI", "EnableParity", defaults.enableParity, CONFIGFILE))
    {
        debuglog("-- Parity is enabled");
        config->flags |= S2S_CFG_ENABLE_PARITY;
    }
    else
    {
        log("-- Parity is disabled");
    }

    if (ini_getbool("SCSI", "ReinsertCDOnInquiry", defaults.reinsertOnInquiry, CONFIGFILE))
    {
        log("-- ReinsertCDOnInquiry is enabled");
    }
    else
    {
        debuglog("-- ReinsertCDOnInquiry is disabled");
    }

    memset(tmp, 0, sizeof(tmp));
    ini_gets("SCSI", "WiFiMACAddress", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        // convert from "01:23:45:67:89" to { 0x01, 0x23, 0x45, 0x67, 0x89 }
        int mac[6];
        if (sscanf(tmp, "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6)
        {
            config->wifiMACAddress[0] = mac[0];
            config->wifiMACAddress[1] = mac[1];
            config->wifiMACAddress[2] = mac[2];
            config->wifiMACAddress[3] = mac[3];
            config->wifiMACAddress[4] = mac[4];
            config->wifiMACAddress[5] = mac[5];
        }
        else
        {
            log("Invalid MAC address format: \"", tmp, "\"");
            memset(config->wifiMACAddress, 0, sizeof(config->wifiMACAddress));
        }
    }

    memset(tmp, 0, sizeof(tmp));
    ini_gets("SCSI", "WiFiSSID", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        memcpy(config->wifiSSID, tmp, sizeof(config->wifiSSID));
    }

    memset(tmp, 0, sizeof(tmp));
    ini_gets("SCSI", "WiFiPassword", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        memcpy(config->wifiPassword, tmp, sizeof(config->wifiPassword));
    }
}

extern "C"
void s2s_debugInit(void)
{
}

extern "C"
void s2s_configPoll(void)
{
}

extern "C"
void s2s_configSave(int scsiId, uint16_t byesPerSector)
{
    // Modification of config over SCSI bus is not implemented.
}

extern "C"
const S2S_TargetCfg* s2s_getConfigByIndex(int index)
{
    if (index < 0 || index >= S2S_MAX_TARGETS)
    {
        return NULL;
    }
    else
    {
        return &g_DiskImages[index];
    }
}

extern "C"
const S2S_TargetCfg* s2s_getConfigById(int scsiId)
{
    int i;
    for (i = 0; i < S2S_MAX_TARGETS; ++i)
    {
        const S2S_TargetCfg* tgt = s2s_getConfigByIndex(i);
        if ((tgt->scsiId & S2S_CFG_TARGET_ID_BITS) == scsiId &&
            (tgt->scsiId & S2S_CFG_TARGET_ENABLED))
        {
            return tgt;
        }
    }
    return NULL;
}

/**********************/
/* FormatUnit command */
/**********************/

// Callback once all data has been read in the data out phase.
static void doFormatUnitComplete(void)
{
    scsiDev.phase = STATUS;
}

static void doFormatUnitSkipData(int bytes)
{
    // We may not have enough memory to store the initialisation pattern and
    // defect list data.  Since we're not making use of it yet anyway, just
    // discard the bytes.
    scsiEnterPhase(DATA_OUT);
    int i;
    for (i = 0; i < bytes; ++i)
    {
        scsiReadByte();
    }
}

// Callback from the data out phase.
static void doFormatUnitPatternHeader(void)
{
    int defectLength =
        ((((uint16_t)scsiDev.data[2])) << 8) +
            scsiDev.data[3];

    int patternLength =
        ((((uint16_t)scsiDev.data[4 + 2])) << 8) +
        scsiDev.data[4 + 3];

        doFormatUnitSkipData(defectLength + patternLength);
        doFormatUnitComplete();
}

// Callback from the data out phase.
static void doFormatUnitHeader(void)
{
    int IP = (scsiDev.data[1] & 0x08) ? 1 : 0;
    int DSP = (scsiDev.data[1] & 0x04) ? 1 : 0;

    if (! DSP) // disable save parameters
    {
        // Save the "MODE SELECT savable parameters"
        s2s_configSave(
            scsiDev.target->targetId,
            scsiDev.target->liveCfg.bytesPerSector);
    }

    if (IP)
    {
        // We need to read the initialisation pattern header first.
        scsiDev.dataLen += 4;
        scsiDev.phase = DATA_OUT;
        scsiDev.postDataOutHook = doFormatUnitPatternHeader;
    }
    else
    {
        // Read the defect list data
        int defectLength =
            ((((uint16_t)scsiDev.data[2])) << 8) +
            scsiDev.data[3];
        doFormatUnitSkipData(defectLength);
        doFormatUnitComplete();
    }
}

/************************/
/* ReadCapacity command */
/************************/

static void doReadCapacity()
{
    uint32_t lba = (((uint32_t) scsiDev.cdb[2]) << 24) +
        (((uint32_t) scsiDev.cdb[3]) << 16) +
        (((uint32_t) scsiDev.cdb[4]) << 8) +
        scsiDev.cdb[5];
    int pmi = scsiDev.cdb[8] & 1;

    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t capacity;

    if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_NETWORK) || unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_AMIGAWIFI))
    {
        capacity = 1;
    }
    else
    {
        capacity = img.file.size() / bytesPerSector;
    }

    if (!pmi && lba)
    {
        // error.
        // We don't do anything with the "partial medium indicator", and
        // assume that delays are constant across each block. But the spec
        // says we must return this error if pmi is specified incorrectly.
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
    else if (capacity > 0)
    {
        uint32_t highestBlock = capacity - 1;

        scsiDev.data[0] = highestBlock >> 24;
        scsiDev.data[1] = highestBlock >> 16;
        scsiDev.data[2] = highestBlock >> 8;
        scsiDev.data[3] = highestBlock;

        uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
        scsiDev.data[4] = bytesPerSector >> 24;
        scsiDev.data[5] = bytesPerSector >> 16;
        scsiDev.data[6] = bytesPerSector >> 8;
        scsiDev.data[7] = bytesPerSector;
        scsiDev.dataLen = 8;
        scsiDev.phase = DATA_IN;
    }
    else
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
        scsiDev.phase = STATUS;
    }
}

/*************************/
/* TestUnitReady command */
/*************************/

static bool doTestUnitReady()
{
    bool ready = true;
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    if (unlikely(!scsiDev.target->started || !img.file.isOpen()))
    {
        ready = 0;
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = LOGICAL_UNIT_NOT_READY_INITIALIZING_COMMAND_REQUIRED;
        scsiDev.phase = STATUS;
    }
    else if (img.ejected)
    {
        ready = false;
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
        scsiDev.phase = STATUS;

        if (img.reinsert_after_eject)
        {
            // We are now reporting to host that the drive is open.
            // Simulate a "close" for next time the host polls.
            if(img.deviceType == S2S_CFG_OPTICAL)
                cdromCloseTray(img);
            else if(img.deviceType != S2S_CFG_FIXED)
                removableInsert(img);
        }
    }
    else if (unlikely(!(blockDev.state & DISK_PRESENT)))
    {
        ready = false;
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
        scsiDev.phase = STATUS;
    }
    else if (unlikely(!(blockDev.state & DISK_INITIALISED)))
    {
        ready = false;
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = LOGICAL_UNIT_NOT_READY_CAUSE_NOT_REPORTABLE;
        scsiDev.phase = STATUS;
    }
    return ready;
}

/****************/
/* Seek command */
/****************/

static void doSeek(uint32_t lba)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t capacity = img.file.size() / bytesPerSector;

    if (lba >= capacity)
    {
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        scsiDev.phase = STATUS;
    }
    else
    {
        if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB) ||
            scsiDev.compatMode < COMPAT_SCSI2)
        {
            s2s_delay_ms(10);
        }
        else
        {
            s2s_delay_us(10);
        }
    }
}

/********************************************/
/* Transfer state for read / write commands */
/********************************************/

BlockDevice blockDev = {DISK_PRESENT | DISK_INITIALISED};
Transfer transfer;
static struct {
    uint8_t *buffer;
    uint32_t bytes_sd; // Number of bytes that have been scheduled for transfer on SD card side
    uint32_t bytes_scsi; // Number of bytes that have been scheduled for transfer on SCSI side

    uint32_t bytes_scsi_started;
    uint32_t sd_transfer_start;
    int parityError;
} g_disk_transfer;

#ifdef PREFETCH_BUFFER_SIZE
static struct {
    uint8_t buffer[PREFETCH_BUFFER_SIZE];
    uint32_t sector;
    uint32_t bytes;
    uint8_t scsiId;
} g_scsi_prefetch;
#endif

/*****************/
/* Write command */
/*****************/

void scsiDiskStartWrite(uint32_t lba, uint32_t blocks)
{
    if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB)) {
        // Floppies are supposed to be slow. Some systems can't handle a floppy
        // without an access time
        s2s_delay_ms(10);
    }

    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t capacity = img.file.size() / bytesPerSector;

    debuglog("------ Write ", (int)blocks, "x", (int)bytesPerSector, " starting at ", (int)lba);

    if (unlikely(blockDev.state & DISK_WP) ||
        unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL) ||
        unlikely(!img.file.isWritable()))

    {
        log("WARNING: Host attempted write to read-only drive ID ", (int)(img.scsiId & S2S_CFG_TARGET_ID_BITS));
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = WRITE_PROTECTED;
        scsiDev.phase = STATUS;
    }
    else if (unlikely(((uint64_t) lba) + blocks > capacity))
    {
        log("WARNING: Host attempted write at sector ", (int)lba, "+", (int)blocks,
              ", exceeding image size ", (int)capacity, " sectors (",
              (int)bytesPerSector, "B/sector)");
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        scsiDev.phase = STATUS;
    }
    else
    {
        transfer.multiBlock = true;
        transfer.lba = lba;
        transfer.blocks = blocks;
        transfer.currentBlock = 0;
        scsiDev.phase = DATA_OUT;
        scsiDev.dataLen = 0;
        scsiDev.dataPtr = 0;

#ifdef PREFETCH_BUFFER_SIZE
        // Invalidate prefetch buffer
        g_scsi_prefetch.bytes = 0;
        g_scsi_prefetch.sector = 0;
#endif

        image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
        if (!img.file.seek((uint64_t)transfer.lba * bytesPerSector))
        {
            log("Seek to ", transfer.lba, " failed for SCSI ID", (int)scsiDev.target->targetId);
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = MEDIUM_ERROR;
            scsiDev.target->sense.asc = NO_SEEK_COMPLETE;
            scsiDev.phase = STATUS;
        }
    }
}

// Called to transfer next block from SCSI bus.
// Usually called from SD card driver during waiting for SD card access.
void diskDataOut_callback(uint32_t bytes_complete)
{
    // For best performance, do SCSI reads in blocks of 4 or more bytes
    bytes_complete &= ~3;

    if (g_disk_transfer.bytes_scsi_started < g_disk_transfer.bytes_scsi)
    {
        // How many bytes remaining in the transfer?
        uint32_t remain = g_disk_transfer.bytes_scsi - g_disk_transfer.bytes_scsi_started;
        uint32_t len = remain;
        
        // Split read so that it doesn't wrap around buffer edge
        uint32_t bufsize = sizeof(scsiDev.data);
        uint32_t start = (g_disk_transfer.bytes_scsi_started % bufsize);
        if (start + len > bufsize)
            len = bufsize - start;

        // Apply platform-specific optimized transfer sizes
        if (len > PLATFORM_OPTIMAL_SCSI_READ_BLOCK_SIZE)
        {
            len = PLATFORM_OPTIMAL_SCSI_READ_BLOCK_SIZE;
        }

        // Don't overwrite data that has not yet been written to SD card
        uint32_t sd_ready_cnt = g_disk_transfer.bytes_sd + bytes_complete;
        if (g_disk_transfer.bytes_scsi_started + len > sd_ready_cnt + bufsize)
            len = sd_ready_cnt + bufsize - g_disk_transfer.bytes_scsi_started;

        // Keep transfers a multiple of sector size.
        // Macintosh SCSI driver seems to get confused if we have a delay
        // in middle of a sector.
        uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
        if (remain >= bytesPerSector && len % bytesPerSector != 0)
        {
            len -= len % bytesPerSector;
        }

        if (len == 0)
            return;

        // debuglog("SCSI read ", (int)start, " + ", (int)len);
        scsiStartRead(&scsiDev.data[start], len, &g_disk_transfer.parityError);
        g_disk_transfer.bytes_scsi_started += len;
    }
}

void diskDataOut()
{
    scsiEnterPhase(DATA_OUT);

    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t blockcount = (transfer.blocks - transfer.currentBlock);
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    g_disk_transfer.buffer = scsiDev.data;
    g_disk_transfer.bytes_scsi = blockcount * bytesPerSector;
    g_disk_transfer.bytes_sd = 0;
    g_disk_transfer.bytes_scsi_started = 0;
    g_disk_transfer.sd_transfer_start = 0;
    g_disk_transfer.parityError = 0;

    while (g_disk_transfer.bytes_sd < g_disk_transfer.bytes_scsi
           && scsiDev.phase == DATA_OUT
           && !scsiDev.resetFlag)
    {
        platform_poll();
        diskEjectButtonUpdate(false);

        // Figure out how many contiguous bytes are available for writing to SD card.
        uint32_t bufsize = sizeof(scsiDev.data);
        uint32_t start = g_disk_transfer.bytes_sd % bufsize;
        uint32_t len = 0;

        // How much data until buffer edge wrap?
        uint32_t available = g_disk_transfer.bytes_scsi_started - g_disk_transfer.bytes_sd;
        if (start + available > bufsize)
            available = bufsize - start;

        // Count number of finished sectors
        if (scsiIsReadFinished(&scsiDev.data[start + available - 1]))
        {
            len = available;
        }
        else
        {
            while (len < available && scsiIsReadFinished(&scsiDev.data[start + len + SD_SECTOR_SIZE - 1]))
            {
                len += SD_SECTOR_SIZE;
            }
        }

        // In case the last sector is partial (256 byte SCSI sectors)
        if (len > available)
        {
            len = available;
        }

        // Apply platform-specific write size blocks for optimization
        if (len > PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE)
        {
            len = PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE;
        }

        uint32_t remain_in_transfer = g_disk_transfer.bytes_scsi - g_disk_transfer.bytes_sd;
        if (len < bufsize - start && len < remain_in_transfer)
        {
            // Use large write blocks in middle of transfer and smaller at the end of transfer.
            // This improves performance for large writes and reduces latency at end of request.
            uint32_t min_write_size = PLATFORM_OPTIMAL_MIN_SD_WRITE_SIZE;
            if (remain_in_transfer <= PLATFORM_OPTIMAL_MAX_SD_WRITE_SIZE)
            {
                min_write_size = PLATFORM_OPTIMAL_LAST_SD_WRITE_SIZE;
            }

            if (len < min_write_size)
            {                
                len = 0;
            }
        }

        if (len == 0)
        {
            // Nothing ready to transfer, check if we can read more from SCSI bus
            diskDataOut_callback(0);
        }
        else
        {
            // Finalize transfer on SCSI side
            scsiFinishRead(&scsiDev.data[start], len, &g_disk_transfer.parityError);

            // Check parity error status before writing to SD card
            if (g_disk_transfer.parityError && (scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY))
            {
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = ABORTED_COMMAND;
                scsiDev.target->sense.asc = SCSI_PARITY_ERROR;
                scsiDev.phase = STATUS;
                break;
            }

            // Start writing to SD card and simultaneously start new SCSI transfers
            // when buffer space is freed.
            uint8_t *buf = &scsiDev.data[start];
            g_disk_transfer.sd_transfer_start = start;
            // debuglog("SD write ", (int)start, " + ", (int)len, " ", bytearray(buf, len));
            platform_set_sd_callback(&diskDataOut_callback, buf);
            if (img.file.write(buf, len) != len)
            {
                log("SD card write failed: ", SD.sdErrorCode());
                scsiDev.status = CHECK_CONDITION;
                scsiDev.target->sense.code = MEDIUM_ERROR;
                scsiDev.target->sense.asc = WRITE_ERROR_AUTO_REALLOCATION_FAILED;
                scsiDev.phase = STATUS;
            }
            platform_set_sd_callback(NULL, NULL);
            g_disk_transfer.bytes_sd += len;

            // Reset the watchdog while the transfer is progressing.
            // If the host stops transferring, the watchdog will eventually expire.
            // This is needed to avoid hitting the watchdog if the host performs
            // a large transfer compared to its transfer speed.
            platform_reset_watchdog();
        }
    }

    // Release SCSI bus
    scsiFinishRead(NULL, 0, &g_disk_transfer.parityError);

    transfer.currentBlock += blockcount;
    scsiDev.dataPtr = scsiDev.dataLen = 0;

    if (transfer.currentBlock == transfer.blocks)
    {
        // Verify that all data has been flushed to disk from SdFat cache.
        // Normally does nothing as we do not change image file size and
        // data writes are not cached.
        img.file.flush();
    }
}

/*****************/
/* Read command */
/*****************/

void scsiDiskStartRead(uint32_t lba, uint32_t blocks)
{
    if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_FLOPPY_14MB)) {
        // Floppies are supposed to be slow. Some systems can't handle a floppy
        // without an access time
        s2s_delay_ms(10);
    }

    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t capacity = img.file.size() / bytesPerSector;

    debuglog("------ Read ", (int)blocks, "x", (int)bytesPerSector, " starting at ", (int)lba);

    if (unlikely(((uint64_t) lba) + blocks > capacity))
    {
        log("WARNING: Host attempted read at sector ", (int)lba, "+", (int)blocks,
              ", exceeding image size ", (int)capacity, " sectors (",
              (int)bytesPerSector, "B/sector)");
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        scsiDev.phase = STATUS;
    }
    else
    {
        transfer.multiBlock = 1;
        transfer.lba = lba;
        transfer.blocks = blocks;
        transfer.currentBlock = 0;
        scsiDev.phase = DATA_IN;
        scsiDev.dataLen = 0;
        scsiDev.dataPtr = 0;

#ifdef PREFETCH_BUFFER_SIZE
        uint32_t sectors_in_prefetch = g_scsi_prefetch.bytes / bytesPerSector;
        if (img.scsiId == g_scsi_prefetch.scsiId &&
            transfer.lba >= g_scsi_prefetch.sector &&
            transfer.lba < g_scsi_prefetch.sector + sectors_in_prefetch)
        {
            // We have the some sectors already in prefetch cache
            scsiEnterPhase(DATA_IN);

            uint32_t start_offset = transfer.lba - g_scsi_prefetch.sector;
            uint32_t count = sectors_in_prefetch - start_offset;
            if (count > transfer.blocks) count = transfer.blocks;
            scsiStartWrite(g_scsi_prefetch.buffer + start_offset * bytesPerSector, count * bytesPerSector);
            debuglog("------ Found ", (int)count, " sectors in prefetch cache");
            transfer.currentBlock += count;
        }

        if (transfer.currentBlock == transfer.blocks)
        {
            while (!scsiIsWriteFinished(NULL) && !scsiDev.resetFlag)
            {
                platform_poll();
                diskEjectButtonUpdate(false);
            }

            scsiFinishWrite();
        }
#endif

        if (!img.file.seek((uint64_t)(transfer.lba + transfer.currentBlock) * bytesPerSector))
        {
            log("Seek to ", transfer.lba, " failed for SCSI ID", (int)scsiDev.target->targetId);
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = MEDIUM_ERROR;
            scsiDev.target->sense.asc = NO_SEEK_COMPLETE;
            scsiDev.phase = STATUS;
        }
    }
}

void diskDataIn_callback(uint32_t bytes_complete)
{
    // On SCSI-1 devices the phase change has some extra delays.
    // Doing it here lets the SD card transfer proceed in background.
    scsiEnterPhase(DATA_IN);

    // For best performance, do writes in blocks of 4 or more bytes
    if (bytes_complete < g_disk_transfer.bytes_sd)
    {
        bytes_complete &= ~3;
    }

    // Machintosh SCSI driver can get confused if pauses occur in middle of
    // a sector, so schedule the transfers in sector sized blocks.
    if (bytes_complete < g_disk_transfer.bytes_sd)
    {
        uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
        if (bytes_complete % bytesPerSector != 0)
        {
            bytes_complete -= bytes_complete % bytesPerSector;
        }
    }

    if (bytes_complete > g_disk_transfer.bytes_scsi)
    {
        // DMA is reading from SD card, bytes_complete bytes have already been read.
        // Send them to SCSI bus now.
        uint32_t len = bytes_complete - g_disk_transfer.bytes_scsi;
        scsiStartWrite(g_disk_transfer.buffer + g_disk_transfer.bytes_scsi, len);
        g_disk_transfer.bytes_scsi += len;
    }

    // Provide a chance for polling request processing
    scsiIsWriteFinished(NULL);
}

// Start a data in transfer using given temporary buffer.
// diskDataIn() below divides the scsiDev.data buffer to two halves for double buffering.
static void start_dataInTransfer(uint8_t *buffer, uint32_t count)
{
    g_disk_transfer.buffer = buffer;
    g_disk_transfer.bytes_scsi = 0;
    g_disk_transfer.bytes_sd = count;

    // Verify that previous write using this buffer has finished
    uint32_t start = millis();
    while (!scsiIsWriteFinished(buffer + count - 1) && !scsiDev.resetFlag)
    {
        if ((uint32_t)(millis() - start) > 5000)
        {
            log("start_dataInTransfer() timeout waiting for previous to finish");
            scsiDev.resetFlag = 1;
        }

        platform_poll();
        diskEjectButtonUpdate(false);
    }
    if (scsiDev.resetFlag) return;

    // Start transferring from SD card
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    platform_set_sd_callback(&diskDataIn_callback, buffer);

    if (img.file.read(buffer, count) != count)
    {
        log("SD card read failed: ", SD.sdErrorCode());
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = MEDIUM_ERROR;
        scsiDev.target->sense.asc = UNRECOVERED_READ_ERROR;
        scsiDev.phase = STATUS;
    }

    diskDataIn_callback(count);
    platform_set_sd_callback(NULL, NULL);

    platform_poll();
    diskEjectButtonUpdate(false);

    // Reset the watchdog while the transfer is progressing.
    // If the host stops transferring, the watchdog will eventually expire.
    // This is needed to avoid hitting the watchdog if the host performs
    // a large transfer compared to its transfer speed.
    platform_reset_watchdog();
}

static void diskDataIn()
{
    // Figure out how many blocks we can fit in buffer
    uint32_t bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
    uint32_t maxblocks = sizeof(scsiDev.data) / bytesPerSector;
    uint32_t maxblocks_half = maxblocks / 2;

    // Start transfer in first half of buffer
    // Waits for the previous first half transfer to finish first.
    uint32_t remain = (transfer.blocks - transfer.currentBlock);
    if (remain > 0)
    {
        uint32_t transfer_blocks = std::min(remain, maxblocks_half);
        uint32_t transfer_bytes = transfer_blocks * bytesPerSector;
        start_dataInTransfer(&scsiDev.data[0], transfer_bytes);
        transfer.currentBlock += transfer_blocks;
    }

    // Start transfer in second half of buffer
    // Waits for the previous second half transfer to finish first
    remain = (transfer.blocks - transfer.currentBlock);
    if (remain > 0)
    {
        uint32_t transfer_blocks = std::min(remain, maxblocks_half);
        uint32_t transfer_bytes = transfer_blocks * bytesPerSector;
        start_dataInTransfer(&scsiDev.data[maxblocks_half * bytesPerSector], transfer_bytes);
        transfer.currentBlock += transfer_blocks;
    }

    if (transfer.currentBlock == transfer.blocks)
    {
        // This was the last block, verify that everything finishes

#ifdef PREFETCH_BUFFER_SIZE
        image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
        int prefetchbytes = img.prefetchbytes;
        if (prefetchbytes > PREFETCH_BUFFER_SIZE) prefetchbytes = PREFETCH_BUFFER_SIZE;
        uint32_t prefetch_sectors = prefetchbytes / bytesPerSector;
        uint32_t img_sector_count = img.file.size() / bytesPerSector;
        g_scsi_prefetch.sector = transfer.lba + transfer.blocks;
        g_scsi_prefetch.bytes = 0;
        g_scsi_prefetch.scsiId = scsiDev.target->cfg->scsiId;

        if (g_scsi_prefetch.sector + prefetch_sectors > img_sector_count)
        {
            // Don't try to read past image end.
            prefetch_sectors = img_sector_count - g_scsi_prefetch.sector;
        }

        while (!scsiIsWriteFinished(NULL) && prefetch_sectors > 0 && !scsiDev.resetFlag)
        {
            platform_poll();
            diskEjectButtonUpdate(false);

            // Check if prefetch buffer is free
            g_disk_transfer.buffer = g_scsi_prefetch.buffer + g_scsi_prefetch.bytes;
            if (!scsiIsWriteFinished(g_disk_transfer.buffer) ||
                !scsiIsWriteFinished(g_disk_transfer.buffer + bytesPerSector - 1))
            {
                continue;
            }

            // We still have time, prefetch next sectors in case this SCSI request
            // is part of a longer linear read.
            g_disk_transfer.bytes_sd = bytesPerSector;
            g_disk_transfer.bytes_scsi = bytesPerSector; // Tell callback not to send to SCSI
            platform_set_sd_callback(&diskDataIn_callback, g_disk_transfer.buffer);
            int status = img.file.read(g_disk_transfer.buffer, bytesPerSector);
            if (status <= 0)
            {
                log("Prefetch read failed");
                prefetch_sectors = 0;
                break;
            }
            g_scsi_prefetch.bytes += status;
            platform_set_sd_callback(NULL, NULL);
            prefetch_sectors--;
        }
#endif

        while (!scsiIsWriteFinished(NULL) && !scsiDev.resetFlag)
        {
            platform_poll();
            diskEjectButtonUpdate(false);
        }

        scsiFinishWrite();
    }
}

void removableInsert(image_config_t &img) {
    if(img.ejected) {
      uint8_t target = img.scsiId & S2S_CFG_TARGET_ID_BITS;
      debuglog("------ Removable inserted on ID ", (int)target);
      img.ejected = false;

      if (scsiDev.boardCfg.flags & S2S_CFG_ENABLE_UNIT_ATTENTION)
      {
        debuglog("------ Posting UNIT ATTENTION after medium change");
        scsiDev.targets[target].unitAttention = NOT_READY_TO_READY_TRANSITION_MEDIUM_MAY_HAVE_CHANGED;
      }
    }
}

void removableEject(image_config_t &img)
{
    uint8_t target = img.scsiId & S2S_CFG_TARGET_ID_BITS;
    if(!img.ejected) {
        debuglog(" ----- Ejecting target ID ", (int)target);
        img.ejected = true;
        switchNextImage(img);
    } else {
        removableInsert(img);
    }
}

/********************/
/* Command dispatch */
/********************/

// Handle direct-access scsi device commands
extern "C"
int scsiDiskCommand()
{
    int commandHandled = 1;
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

    uint8_t command = scsiDev.cdb[0];
    if (unlikely(command == 0x1B))
    {
        // START STOP UNIT
        // Enable or disable media access operations.
        //int immed = scsiDev.cdb[1] & 1;
        int start = scsiDev.cdb[4] & 1;

        if (start)
        {
            if(img.deviceType == S2S_CFG_FIXED)
                scsiDev.target->started = true;
            else
                removableInsert(img);
        }
        else // Stop
        {
            if(img.deviceType == S2S_CFG_FIXED)
                scsiDev.target->started = false;
            else
                removableEject(img);
        }
    }
    else if (unlikely(command == 0x00))
    {
        // TEST UNIT READY
        doTestUnitReady();
    }
    else if (unlikely(!doTestUnitReady()))
    {
        // Status and sense codes already set by doTestUnitReady
    }
    else if (likely(command == 0x08))
    {
        // READ(6)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
            (((uint32_t) scsiDev.cdb[2]) << 8) +
            scsiDev.cdb[3];
        uint32_t blocks = scsiDev.cdb[4];
        if (unlikely(blocks == 0)) blocks = 256;
        scsiDiskStartRead(lba, blocks);
    }
    else if (likely(command == 0x28))
    {
        // READ(10)
        // Ignore all cache control bits - we don't support a memory cache.

        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];

        scsiDiskStartRead(lba, blocks);
    }
    else if (likely(command == 0x0A))
    {
        // WRITE(6)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
            (((uint32_t) scsiDev.cdb[2]) << 8) +
            scsiDev.cdb[3];
        uint32_t blocks = scsiDev.cdb[4];
        if (unlikely(blocks == 0)) blocks = 256;
        scsiDiskStartWrite(lba, blocks);
    }
    else if (likely(command == 0x2A) || // WRITE(10)
        unlikely(command == 0x2E)) // WRITE AND VERIFY
    {
        // Ignore all cache control bits - we don't support a memory cache.
        // Don't bother verifying either. The SD card likely stores ECC
        // along with each flash row.

        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];
        uint32_t blocks =
            (((uint32_t) scsiDev.cdb[7]) << 8) +
            scsiDev.cdb[8];

        scsiDiskStartWrite(lba, blocks);
    }
    else if (unlikely(command == 0x04))
    {
        // FORMAT UNIT
        // We don't really do any formatting, but we need to read the correct
        // number of bytes in the DATA_OUT phase to make the SCSI host happy.

        int fmtData = (scsiDev.cdb[1] & 0x10) ? 1 : 0;
        if (fmtData)
        {
            // We need to read the parameter list, but we don't know how
            // big it is yet. Start with the header.
            scsiDev.dataLen = 4;
            scsiDev.phase = DATA_OUT;
            scsiDev.postDataOutHook = doFormatUnitHeader;
        }
        else
        {
            // No data to read, we're already finished!
        }
    }
    else if (unlikely(command == 0x25))
    {
        // READ CAPACITY
        doReadCapacity();
    }
    else if (unlikely(command == 0x0B))
    {
        // SEEK(6)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[1] & 0x1F) << 16) +
            (((uint32_t) scsiDev.cdb[2]) << 8) +
            scsiDev.cdb[3];

        doSeek(lba);
    }

    else if (unlikely(command == 0x2B))
    {
        // SEEK(10)
        uint32_t lba =
            (((uint32_t) scsiDev.cdb[2]) << 24) +
            (((uint32_t) scsiDev.cdb[3]) << 16) +
            (((uint32_t) scsiDev.cdb[4]) << 8) +
            scsiDev.cdb[5];

        doSeek(lba);
    }
    else if (unlikely(command == 0x36))
    {
        // LOCK UNLOCK CACHE
        // We don't have a cache to lock data into. do nothing.
    }
    else if (unlikely(command == 0x34))
    {
        // PRE-FETCH.
        // We don't have a cache to pre-fetch into. do nothing.
    }
    else if (unlikely(command == 0x1E))
    {
        // PREVENT ALLOW MEDIUM REMOVAL
        // Not much we can do to prevent the user removing the SD card.
        // do nothing.
    }
    else if (unlikely(command == 0x01))
    {
        // REZERO UNIT
        // Set the lun to a vendor-specific state. Ignore.
    }
    else if (unlikely(command == 0x35))
    {
        // SYNCHRONIZE CACHE
        // We don't have a cache. do nothing.
    }
    else if (unlikely(command == 0x2F))
    {
        // VERIFY
        // TODO: When they supply data to verify, we should read the data and
        // verify it. If they don't supply any data, just say success.
        if ((scsiDev.cdb[1] & 0x02) == 0)
        {
            // They are asking us to do a medium verification with no data
            // comparison. Assume success, do nothing.
        }
        else
        {
            // TODO. This means they are supplying data to verify against.
            // Technically we should probably grab the data and compare it.
            scsiDev.status = CHECK_CONDITION;
            scsiDev.target->sense.code = ILLEGAL_REQUEST;
            scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
            scsiDev.phase = STATUS;
        }
    }
    else if (unlikely(command == 0x37))
    {
        // READ DEFECT DATA
        uint32_t allocLength = (((uint16_t)scsiDev.cdb[7]) << 8) |
            scsiDev.cdb[8];

        scsiDev.data[0] = 0;
        scsiDev.data[1] = scsiDev.cdb[1];
        scsiDev.data[2] = 0;
        scsiDev.data[3] = 0;
        scsiDev.dataLen = 4;

        if (scsiDev.dataLen > allocLength)
        {
            scsiDev.dataLen = allocLength;
        }

        scsiDev.phase = DATA_IN;
    }
    else if (img.file.isRom())
    {
        // Special handling for ROM drive to make SCSI2SD code report it as read-only
        blockDev.state |= DISK_WP;
        commandHandled = scsiModeCommand();
        blockDev.state &= ~DISK_WP;
    }
    else
    {
        commandHandled = 0;
    }

    return commandHandled;
}

extern "C"
void scsiDiskPoll()
{
    if (scsiDev.phase == DATA_IN &&
        transfer.currentBlock != transfer.blocks)
    {
        diskDataIn();
     }
    else if (scsiDev.phase == DATA_OUT &&
        transfer.currentBlock != transfer.blocks)
    {
        diskDataOut();
    }

    if (scsiDev.phase == STATUS && scsiDev.target)
    {
        // Check if the command is affected by drive geometry.
        // Affected commands are:
        // 0x1A MODE SENSE command of pages 0x03 (device format), 0x04 (disk geometry) or 0x3F (all pages)
        // 0x1C RECEIVE DIAGNOSTICS RESULTS
        uint8_t command = scsiDev.cdb[0];
        uint8_t pageCode = scsiDev.cdb[2] & 0x3F;
        if ((command == 0x1A && (pageCode == 0x03 || pageCode == 0x04 || pageCode == 0x3F)) ||
            command == 0x1C)
        {
            image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
            checkDiskGeometryDivisible(img);
        }

        // Check for Inquiry command to close CD-ROM tray on boot
        if (command == 0x12)
        {
            image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
            if (img.deviceType == S2S_CFG_OPTICAL && img.reinsert_on_inquiry)
            {
                cdromCloseTray(img);
            }
        }
    }
}

extern "C"
void scsiDiskReset()
{
    scsiDev.dataPtr = 0;
    scsiDev.savedDataPtr = 0;
    scsiDev.dataLen = 0;
    // transfer.lba = 0; // Needed in Request Sense to determine failure
    transfer.blocks = 0;
    transfer.currentBlock = 0;
    transfer.multiBlock = 0;

#ifdef PREFETCH_BUFFER_SIZE
    g_scsi_prefetch.bytes = 0;
    g_scsi_prefetch.sector = 0;
#endif

    // Reinsert any ejected CD-ROMs on BUS RESET and restart from first image
    for (int i = 0; i < S2S_MAX_TARGETS; ++i)
    {
        image_config_t &img = g_DiskImages[i];
        if (img.deviceType == S2S_CFG_OPTICAL)
        {
            cdromReinsertFirstImage(img);
        }
    }
}

extern "C"
void scsiDiskInit()
{
    scsiDiskReset();
}

