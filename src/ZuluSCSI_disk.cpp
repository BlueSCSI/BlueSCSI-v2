/**
 * SCSI2SD V6 - Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
 * Portions Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
 * Portions Copyright (C) 2023 Eric Helgeson
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 *
 * This file is licensed under the GPL version 3 or any later version. 
 * It is derived from disk.c in SCSI2SD V6
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


// This file implements the main SCSI disk emulation and data streaming.
// It is derived from disk.c in SCSI2SD V6.

#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include "ZuluSCSI_settings.h"
#ifdef ENABLE_AUDIO_OUTPUT
#  include "ZuluSCSI_audio.h"
#endif
#include "ZuluSCSI_cdrom.h"
#include "ImageBackingStore.h"
#include "ROMDrive.h"
#include "QuirksCheck.h"
#include <minIni.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <SdFat.h>

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

    uint32_t maxsize = platform_get_romdrive_maxsize() - PLATFORM_ROMDRIVE_PAGE_SIZE;
    logmsg("-- Platform supports ROM drive up to ", (int)(maxsize / 1024), " kB");

    romdrive_hdr_t hdr = {};
    if (!romDriveCheckPresent(&hdr))
    {
        logmsg("---- ROM drive image not detected");
        return false;
    }

    if (ini_getbool("SCSI", "DisableROMDrive", 0, CONFIGFILE))
    {
        logmsg("---- ROM drive disabled in ini file, not enabling");
        return false;
    }

    long rom_scsi_id = ini_getl("SCSI", "ROMDriveSCSIID", -1, CONFIGFILE);
    if (rom_scsi_id >= 0 && rom_scsi_id <= 7)
    {
        hdr.scsi_id = rom_scsi_id;
        logmsg("---- ROM drive SCSI id overriden in ini file, changed to ", (int)hdr.scsi_id);
    }

    if (s2s_getConfigById(hdr.scsi_id))
    {
        logmsg("---- ROM drive SCSI id ", (int)hdr.scsi_id, " is already in use, not enabling");
        return false;
    }

    logmsg("---- Activating ROM drive, SCSI id ", (int)hdr.scsi_id, " size ", (int)(hdr.imagesize / 1024), " kB");
    g_scsi_settings.initDevice(hdr.scsi_id, hdr.drivetype);
    bool status = scsiDiskOpenHDDImage(hdr.scsi_id, "ROM:", 0, hdr.blocksize, hdr.drivetype);

    if (!status)
    {
        logmsg("---- ROM drive activation failed");
        return false;
    }
    else
    {
        return true;
    }

#endif
}


/***********************/
/* Image configuration */
/***********************/

extern SdFs SD;
SdDevice sdDev = {2, 256 * 1024 * 1024 * 2}; /* For SCSI2SD */

image_config_t g_DiskImages[S2S_MAX_TARGETS];

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

uint32_t image_config_t::get_capacity_lba()
{
    if (bin_container.isOpen() && cuesheetfile.isOpen())
    {
        size_t halfbufsize = sizeof(scsiDev.data) / 2;
        char *cuebuf = (char*)&scsiDev.data[halfbufsize];
        cuesheetfile.seekSet(0);
        int len = cuesheetfile.read(cuebuf, halfbufsize);
        if (len == 0)
            return 0;
        CUEParser parser(cuebuf);
        CUETrackInfo const *track;
        CUETrackInfo last_track = {0};
        if (bin_container.isDir())
        {
            FsFile bin_file;
            uint64_t prev_capacity = 0;
            // Find last track
            while((track = parser.next_track(prev_capacity)) != nullptr)
            {
                last_track = *track;
                if (!bin_file.open(&bin_container, track->filename, O_RDONLY | O_BINARY))
                {
                    dbgmsg("Unable to open cue/multi-bin image file \"", track->filename, "\" to determine total capacity");
                    return 0;
                }
                prev_capacity = bin_file.size();
                bin_file.close();
            }
            if (last_track.track_number != 0)
                return last_track.data_start + prev_capacity / last_track.sector_length;
            else
                return 0;
        }
        else
        {
            // Single bin file
            track = parser.next_track();
            return track->data_start + bin_container.size() / track->sector_length;
        }
    }
    else
        return file.size() / bytesPerSector;

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

// Load values for target image configuration from given section if they exist.
// Otherwise keep current settings.
static void scsiDiskSetImageConfig(uint8_t target_idx)
{
    image_config_t &img = g_DiskImages[target_idx];
    scsi_system_settings_t *devSys = g_scsi_settings.getSystem();
    scsi_device_settings_t *devCfg = g_scsi_settings.getDevice(target_idx);
    img.scsiId = target_idx;
    memset(img.vendor, 0, sizeof(img.vendor));
    memset(img.prodId, 0, sizeof(img.prodId));
    memset(img.revision, 0, sizeof(img.revision));
    memset(img.serial, 0, sizeof(img.serial));

    img.deviceType = devCfg->deviceType;
    img.deviceTypeModifier = devCfg->deviceTypeModifier;
    img.sectorsPerTrack = devCfg->sectorsPerTrack;
    img.headsPerCylinder = devCfg->headsPerCylinder;
    img.quirks = devSys->quirks;
    img.rightAlignStrings = devCfg->rightAlignStrings;
    img.name_from_image = devCfg->nameFromImage;
    img.prefetchbytes = devCfg->prefetchBytes;
    img.reinsert_on_inquiry = devCfg->reinsertOnInquiry;
    img.reinsert_after_eject = devCfg->reinsertAfterEject;
    img.ejectButton = devCfg->ejectButton;
    img.vendorExtensions = devCfg->vendorExtensions;

#ifdef ENABLE_AUDIO_OUTPUT
    uint16_t vol = devCfg->vol;
    // Set volume on both channels
    audio_set_volume(target_idx, (vol << 8) | vol);
#endif

    memcpy(img.vendor, devCfg->vendor, sizeof(img.vendor));
    memcpy(img.prodId, devCfg->prodId, sizeof(img.prodId));
    memcpy(img.revision, devCfg->revision, sizeof(img.revision));
    memcpy(img.serial, devCfg->serial, sizeof(img.serial));
}

static bool find_chs_capacity(uint64_t lba, uint16_t max_cylinders, uint8_t min_heads, uint16_t &c, uint8_t &h, uint8_t &s)
{
    bool found_chs = false;
    uint32_t cylinders;
    for (uint8_t heads = 16 ; heads >= min_heads; heads--)
    {
        if (lba % heads != 0)
            continue;
        for (uint8_t sectors = 63; sectors >= 1; sectors--)
        {
            if (lba % (heads * sectors) == 0)
            {
                cylinders = lba / (heads * sectors);
                if (cylinders > max_cylinders)
                    continue;
                found_chs = true;
                c = (uint16_t) cylinders;
                h = heads;
                s = sectors;
                break;
            }
        }
        if (found_chs)
            break;
    }
    return found_chs;
}

static void autoConfigGeometry(image_config_t &img)
{
    const char *method = "INI config";
    if (img.sectorsPerTrack == 0 || img.headsPerCylinder == 0)
    {
        uint16_t cyl = 0;
        uint8_t head = 255;
        uint8_t sect = 63;
        bool found_chs = false;

        if (img.deviceType == S2S_CFG_FLOPPY_14MB && img.scsiSectors <= 2880)
        {
            method = "device type floppy";
            sect = 18;
            head = 80;
        }
        else if (img.scsiSectors <= 1032192)
        {
            found_chs = find_chs_capacity(img.scsiSectors, 1024, 1, cyl, head, sect);
            method = "image size";
        }
        else if (img.scsiSectors <= 16514064)
        {
            found_chs = find_chs_capacity(img.scsiSectors, 16383, 9, cyl, head, sect);
            if (!found_chs)
                found_chs = find_chs_capacity(img.scsiSectors, 32767, 5, cyl, head, sect);
            if (!found_chs)
                found_chs = find_chs_capacity(img.scsiSectors, 65535, 1, cyl, head, sect);
            method = "image size";
        }

        if (!found_chs)
        {
            head = 255;
            sect = 63;
            method = "defaults";
        }

        img.sectorsPerTrack = sect;
        img.headsPerCylinder = head;
    }

    bool divisible = (img.scsiSectors % ((uint32_t)img.sectorsPerTrack * img.headsPerCylinder)) == 0;
    logmsg("---- Drive geometry from ", method,
        ": SectorsPerTrack=", (int)img.sectorsPerTrack,
        " HeadsPerCylinder=", (int)img.headsPerCylinder,
        " total sectors ", (int)img.scsiSectors,
        divisible ? " (divisible)" : " (not divisible)"
        );
}

bool scsiDiskOpenHDDImage(int target_idx, const char *filename, int scsi_lun, int blocksize, S2S_CFG_TYPE type, bool use_prefix)
{
    image_config_t &img = g_DiskImages[target_idx];
    img.cuesheetfile.close();
    img.bin_container.close();
    img.cdrom_binfile_index = -1;
    scsiDiskSetImageConfig(target_idx);
    img.file = ImageBackingStore(filename, blocksize);

    if (img.file.isOpen())
    {
        img.bytesPerSector = blocksize;
        img.scsiSectors = img.file.size() / blocksize;
        img.scsiId = target_idx | S2S_CFG_TARGET_ENABLED;
        img.sdSectorStart = 0;

        if (img.scsiSectors == 0 && type != S2S_CFG_NETWORK && !img.file.isFolder())
        {
            logmsg("---- Error: image file ", filename, " is empty");
            img.file.close();
            return false;
        }
        uint32_t sector_begin = 0, sector_end = 0;
        if (img.file.isRom() || type == S2S_CFG_NETWORK || img.file.isFolder())
        {
            // ROM is always contiguous, no need to log
        }
        else if (img.file.contiguousRange(&sector_begin, &sector_end))
        {
#ifdef ZULUSCSI_HARDWARE_CONFIG
            if (g_hw_config.is_active())
            {
                dbgmsg("----  Device spans SD card sectors ", (int)sector_begin, " to ", (int)sector_end);
            }
            else
#endif // ZULUSCSI_HARDWARE_CONFIG
            {
                dbgmsg("---- Image file is contiguous, SD card sectors ", (int)sector_begin, " to ", (int)sector_end);
            }
        }
        else
        {
            logmsg("---- WARNING: file ", filename, " is not contiguous. This will increase read latency.");
        }

        S2S_CFG_TYPE setting_type = (S2S_CFG_TYPE) g_scsi_settings.getDevice(target_idx)->deviceType;
        if ( setting_type != S2S_CFG_NOT_SET)
        {
            type = setting_type;
        }

        if (type == S2S_CFG_FIXED)
        {
            logmsg("---- Configuring as disk drive drive");
            img.deviceType = S2S_CFG_FIXED;
        }
        else if (type == S2S_CFG_OPTICAL)
        {
            logmsg("---- Configuring as CD-ROM drive");
            img.deviceType = S2S_CFG_OPTICAL;
            if (g_scsi_settings.getDevice(target_idx)->vendorExtensions & VENDOR_EXTENSION_OPTICAL_PLEXTOR)
            {
                logmsg("---- Plextor 0xD8 vendor extension enabled");
            }
        }
        else if (type == S2S_CFG_FLOPPY_14MB)
        {
            logmsg("---- Configuring as floppy drive");
            img.deviceType = S2S_CFG_FLOPPY_14MB;
        }
        else if (type == S2S_CFG_MO)
        {
            logmsg("---- Configuring as magneto-optical");
            img.deviceType = S2S_CFG_MO;
        }
#ifdef ZULUSCSI_NETWORK
        else if (type == S2S_CFG_NETWORK)
        {
            logmsg("---- Configuring as network based on image name");
            img.deviceType = S2S_CFG_NETWORK;
        }
#endif // ZULUSCSI_NETWORK
        else if (type == S2S_CFG_REMOVABLE)
        {
            logmsg("---- Configuring as removable drive");
            img.deviceType = S2S_CFG_REMOVABLE;
        }
        else if (type == S2S_CFG_SEQUENTIAL)
        {
            logmsg("---- Configuring as tape drive");
            img.deviceType = S2S_CFG_SEQUENTIAL;
            img.tape_mark_count = 0;
            scsiDev.target->sense.filemark = false;
            scsiDev.target->sense.eom = false;
        }
        else if (type == S2S_CFG_ZIP100)
        {
            logmsg("---- Configuration as Iomega Zip100");
            img.deviceType = S2S_CFG_ZIP100;
            if(img.file.size() != ZIP100_DISK_SIZE)
            {
                logmsg("---- Zip 100 disk (", (int)img.file.size(), " bytes) is not exactly ", ZIP100_DISK_SIZE, " bytes, may not work correctly");
            }
        }

        if (type != S2S_CFG_OPTICAL && type != S2S_CFG_NETWORK)
        {
            autoConfigGeometry(img);
        }

        quirksCheck(&img);

        if (img.name_from_image)
        {
            setNameFromImage(img, filename);
            logmsg("---- Vendor / product id set from image file name");
        }

        if (type == S2S_CFG_NETWORK)
        {
            // prefetch not used, skip emitting log message
        }
        else if (img.prefetchbytes > 0)
        {
            logmsg("---- Read prefetch enabled: ", (int)img.prefetchbytes, " bytes");
        }
        else
        {
            logmsg("---- Read prefetch disabled");
        }

        if (img.deviceType == S2S_CFG_OPTICAL &&
            strncasecmp(filename + strlen(filename) - 4, ".bin", 4) == 0)
        {
            // Check for .cue sheet with single .bin file
            char cuesheetname[MAX_FILE_PATH + 1] = {0};
            strncpy(cuesheetname, filename, strlen(filename) - 4);
            strlcat(cuesheetname, ".cue", sizeof(cuesheetname));
            img.cuesheetfile = SD.open(cuesheetname, O_RDONLY);

            if (img.cuesheetfile.isOpen())
            {
                logmsg("---- Found CD-ROM CUE sheet at ", cuesheetname);
                if (!cdromValidateCueSheet(img))
                {
                    logmsg("---- Failed to parse cue sheet, using as plain binary image");
                    img.cuesheetfile.close();
                }
                else
                {
                    // Set bin container to single bin file
                    img.bin_container.open(filename);
                    // If bin container is a directory close the file
                    if (img.bin_container.isDir())
                        img.bin_container.close();
                }
            }
            else
            {
                logmsg("---- No CUE sheet found at ", cuesheetname, ", using as plain binary image");
            }
        }
        else if (img.deviceType == S2S_CFG_OPTICAL && img.file.isFolder())
        {
            // The folder should contain .cue sheet and one or several .bin files
            char foldername[MAX_FILE_PATH + 1] = {0};
            char cuesheetname[MAX_FILE_PATH + 1] = {0};
            img.file.getFoldername(foldername, sizeof(foldername));
            FsFile folder = SD.open(foldername, O_RDONLY);
            bool valid = false;
            img.cuesheetfile.close();
            while (!valid && img.cuesheetfile.openNext(&folder, O_RDONLY))
            {
                img.cuesheetfile.getName(cuesheetname, sizeof(cuesheetname));
                if (strncasecmp(cuesheetname + strlen(cuesheetname) - 4, ".cue", 4) == 0)
                {
                    valid = cdromValidateCueSheet(img);
                }
            }

            if (valid)
            {
                img.bin_container.open(foldername);
            }
            else
            {
                logmsg("No valid .cue sheet found in folder '", foldername, "'");
                img.cuesheetfile.close();
            }
        }
        else if (img.deviceType == S2S_CFG_SEQUENTIAL && img.file.isFolder())
        {
            // multi file tape that implements tape markers
            char name[MAX_FILE_PATH + 1] = {0};
            img.file.getFoldername(name, sizeof(name));
            img.bin_container.open(name);
            FsFile file;
            bool valid = false;

            while(file.openNext(&img.bin_container))
            {
                file.getName(name, sizeof(name));
                if(!file.isDir() && !file.isHidden() && scsiDiskFilenameValid(name))
                {
                    valid = true;
                    img.tape_mark_count++;
                }
            }
            if (!valid)
            {
                // if there are no valid image files, create one
                file.open(&img.bin_container, TAPE_DEFAULT_NAME, O_CREAT);
                file.close();
            }
            img.tape_mark_index = 0;
            img.tape_mark_block_offset = 0;
            img.tape_load_next_file = false;
        }

        img.use_prefix = use_prefix;
        img.file.getFilename(img.current_image, sizeof(img.current_image));
        return true;
    }
    else
    {
        logmsg("---- Failed to load image '", filename, "', ignoring");
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
            logmsg("WARNING: Host used command ", scsiDev.cdb[0],
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
            ".rom_loaded", ".cue", ".txt", ".rtf", ".md", ".nfo", ".pdf", ".doc", 
	    ".ini", ".mid", ".midi", ".aiff", ".mp3", ".m4a",
            NULL
        };
        const char *archive_exts[] = {
            ".tar", ".tgz", ".gz", ".bz2", ".tbz2", ".xz", ".zst", ".z",
            ".zip", ".zipx", ".rar", ".lzh", ".lha", ".lzo", ".lz4", ".arj",
            ".dmg", ".hqx", ".cpt", ".7z", ".s7z", ".mid", ".wav", ".aiff",
            NULL
        };

        for (int i = 0; ignore_exts[i]; i++)
        {
            if (strcasecmp(extension, ignore_exts[i]) == 0)
            {
                // ignore these without log message
                return false;
            }
        }
        for (int i = 0; archive_exts[i]; i++)
        {
            if (strcasecmp(extension, archive_exts[i]) == 0)
            {
                logmsg("-- Ignoring compressed file ", name);
                return false;
            }
        }
    }
    // Check first character
    if (!isalnum(name[0]))
    {
        // ignore files that don't start with a letter or a number
        return false;
    }
    return true;
}

bool scsiDiskFolderContainsCueSheet(FsFile *dir)
{
    FsFile file;
    char filename[MAX_FILE_PATH + 1];
    while (file.openNext(dir, O_RDONLY))
    {
        if (file.getName(filename, sizeof(filename)) &&
            (strncasecmp(filename + strlen(filename) - 4, ".cue", 4) == 0))
        {
            return true;
        }
    }

    return false;
}

bool scsiDiskFolderIsTapeFolder(FsFile *dir)
{
    char filename[MAX_FILE_PATH + 1];
    dir->getName(filename, sizeof(filename));
    // string starts with 'tp', the 3rd character is a SCSI ID, and it has more 3 charters
    // e.g. "tp0 - tape 01"
    if (strlen(filename) > 3 && strncasecmp("tp", filename, 2) == 0 
        && filename[2] >= '0' && filename[2] - '0' < NUM_SCSIID)
    {
        return true;
    }
    return false;
}

static void scsiDiskCheckDir(char * dir_name, int target_idx, image_config_t* img, S2S_CFG_TYPE type, const char* type_name)
{
    if (SD.exists(dir_name))
    {
        if (img->image_directory)
        {
            logmsg("-- Already found an image directory, skipping '", dir_name, "'");
        }
        else
        {
            img->deviceType = type;
            img->image_directory = true;
            logmsg("SCSI", target_idx, " searching default ", type_name, " image directory '", dir_name, "'");
        }
    }
}


// Load values for target configuration from given section if they exist.
// Otherwise keep current settings.
static void scsiDiskSetConfig(int target_idx)
{
  
    image_config_t &img = g_DiskImages[target_idx];
    img.scsiId = target_idx;

    scsiDiskSetImageConfig(target_idx);

    char section[6] = "SCSI0";
    section[4] += target_idx;
    char tmp[32];

    ini_gets(section, "ImgDir", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        logmsg("SCSI", target_idx, " using image directory '", tmp, "'");
        img.image_directory = true;
    }
    else
    {
        strcpy(tmp, "HD0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_FIXED, "disk");

        strcpy(tmp, "CD0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_OPTICAL, "optical");

        strcpy(tmp, "RE0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_REMOVABLE, "removable");

        strcpy(tmp, "MO0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_MO, "magneto-optical");

        strcpy(tmp, "TP0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_SEQUENTIAL, "tape");

        strcpy(tmp, "FD0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_FLOPPY_14MB, "floppy");

        strcpy(tmp, "ZP0");
        tmp[2] += target_idx;
        scsiDiskCheckDir(tmp, target_idx, &img, S2S_CFG_ZIP100, "Iomega Zip 100");

    }
    g_scsi_settings.initDevice(target_idx, (S2S_CFG_TYPE)img.deviceType);
}

// Compares the prefix of both files and the scsi ID
// cd3-name.iso and CD3-otherfile.bin matches, zp3.img or cd4-name.iso would not
static bool compare_prefix(const char* name, const char* compare)
{
    if (strlen(name) >= 3 && strlen(compare) >= 3)
    {
        if (tolower(name[0]) == tolower(compare[0])
            && tolower(name[1]) == tolower(compare[1])
            && tolower(name[2]) == tolower(compare[2])
        )
        return true;
    }
    return false;
}

/***********************/
/* Start/stop commands */
/***********************/
static void doCloseTray(image_config_t &img)
{
    if (img.ejected)
    {
        uint8_t target = img.scsiId & 7;
        dbgmsg("------ Device close tray on ID ", (int)target);
        img.ejected = false;

        if (scsiDev.boardCfg.flags & S2S_CFG_ENABLE_UNIT_ATTENTION)
        {
            dbgmsg("------ Posting UNIT ATTENTION after medium change");
            scsiDev.targets[target].unitAttention = NOT_READY_TO_READY_TRANSITION_MEDIUM_MAY_HAVE_CHANGED;
        }
    }
}

 
// Eject and switch image
static void doPerformEject(image_config_t &img)
{
    uint8_t target = img.scsiId & 7;
    if (!img.ejected)
    {
        dbgmsg("------ Device open tray on ID ", (int)target);
        img.ejected = true;
        switchNextImage(img); // Switch media for next time
    }
    else
    {
        doCloseTray(img);
    }
}

int findNextImageAfter(image_config_t &img,
        const char* dirname, const char* filename,
        char* buf, size_t buflen, bool ignore_prefix)
{
    FsFile dir;
    if (dirname[0] == '\0')
    {
        logmsg("Image directory name invalid for ID", (img.scsiId & S2S_CFG_TARGET_ID_BITS));
        return 0;
    }
    if (!dir.open(dirname))
    {
        logmsg("Image directory '", dirname, "' couldn't be opened");
        return 0;
    }
    if (!dir.isDir())
    {
        logmsg("Can't find images in '", dirname, "', not a directory");
        dir.close();
        return 0;
    }
    if (dir.isHidden())
    {
        logmsg("Image directory '", dirname, "' is hidden, skipping");
        dir.close();
        return 0;
    }

    char first_name[MAX_FILE_PATH] = {'\0'};
    char candidate_name[MAX_FILE_PATH] = {'\0'};
    FsFile file;
    while (file.openNext(&dir, O_RDONLY))
    {
        if (file.isDir() && !scsiDiskFolderContainsCueSheet(&file)) continue;
        if (!file.getName(buf, MAX_FILE_PATH))
        {
            logmsg("Image directory '", dirname, "' had invalid file");
            continue;
        }
        if (!scsiDiskFilenameValid(buf)) continue;
        if (file.isHidden()) {
            logmsg("Image '", dirname, "/", buf, "' is hidden, skipping file");
            continue;
        }

        if (!ignore_prefix && img.use_prefix && !compare_prefix(filename, buf)) continue;

        // keep track of the first item to allow wrapping
        // without having to iterate again
        if (first_name[0] == '\0' || strcasecmp(buf, first_name) < 0)
        {
            strncpy(first_name, buf, sizeof(first_name));
        }

        // discard if no selected name, or if candidate is before (or is) selected
        // or prefix searching is enabled and file doesn't contain current prefix
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
        logmsg("Image directory '", dirname, "' was empty");
        img.image_directory = false;
        return 0;
    }
}

int scsiDiskGetNextImageName(image_config_t &img, char *buf, size_t buflen)
{
    int target_idx = img.scsiId & S2S_CFG_TARGET_ID_BITS;

    char section[6] = "SCSI0";
    section[4] = '0' + target_idx;

    // sanity check: is provided buffer is long enough to store a filename?
    assert(buflen >= MAX_FILE_PATH);

    // find the next filename
    char nextname[MAX_FILE_PATH];
    int nextlen;

    if (img.image_directory)
    {
        // image directory was found during startup
        char dirname[MAX_FILE_PATH];
        char key[] = "ImgDir";
        int dirlen = ini_gets(section, key, "", dirname, sizeof(dirname), CONFIGFILE);
        if (!dirlen)
        {
            switch (img.deviceType)
            {
                case S2S_CFG_FIXED:
                    strcpy(dirname ,"HD0");
                    break;
                case S2S_CFG_OPTICAL:
                    strcpy(dirname, "CD0");
                break;
                case S2S_CFG_REMOVABLE:
                    strcpy(dirname, "RE0");
                break;
                case S2S_CFG_MO:
                    strcpy(dirname, "MO0");
                break;
                case S2S_CFG_SEQUENTIAL:
                    strcpy(dirname ,"TP0");
                break;
                case S2S_CFG_FLOPPY_14MB:
                    strcpy(dirname, "FD0");
                break;
                case S2S_CFG_ZIP100:
                    strcpy(dirname, "ZP0");
                break;
                default:
                    dbgmsg("No matching device type for default directory found");
                    return 0;
            }
            dirname[2] += target_idx;
            if (!SD.exists(dirname))
            {
                dbgmsg("Default image directory, ", dirname, " does not exist");
                return 0;
            }
        }

        // find the next filename
        nextlen = findNextImageAfter(img, dirname, img.current_image, nextname, sizeof(nextname));

        if (nextlen == 0)
        {
            logmsg("Image directory was empty for ID", target_idx);
            return 0;
        }
        else if (buflen < nextlen + dirlen + 2)
        {
            logmsg("Directory '", dirname, "' and file '", nextname, "' exceed allowed length");
            return 0;
        }
        else
        {
            // construct a return value
            strncpy(buf, dirname, buflen);
            if (buf[strlen(buf) - 1] != '/') strcat(buf, "/");
            strcat(buf, nextname);
            return dirlen + nextlen;
        }
    }
    else if (img.use_prefix)
    {
        nextlen = findNextImageAfter(img, "/", img.current_image, nextname, sizeof(nextname));
        if (nextlen == 0)
        {
            logmsg("Next file with the same prefix as ", img.current_image," not found for ID", target_idx);
        }
        else if (buflen < nextlen + 1)
        {
            logmsg("Next file exceeds, '",nextname, "' exceed allowed length");
        }
        else
        {
            // construct a return value
            strncpy(buf, nextname, buflen);
            return nextlen;
        }
        img.image_index = -1;
        return 0;
    }
    else
    {
        img.image_index++;
        if (img.image_index > IMAGE_INDEX_MAX || img.image_index < 0)
        {
            img.image_index = 0;
        }

        char key[5] = "IMG0";
        key[3] = '0' + img.image_index;

        int ret = ini_gets(section, key, "", buf, buflen, CONFIGFILE);
        if (buf[0] != '\0')
        {
            img.deviceType = g_scsi_settings.getDevice(target_idx)->deviceType;
            return ret;
        }
        else if (img.image_index > 0)
        {
            // there may be more than one image but we've ran out of new ones
            // wrap back to the first image
            img.image_index = -1;
            return scsiDiskGetNextImageName(img, buf, buflen);
        }
        else
        {

            img.image_index = -1;
            return 0;
        }
    }
}

void scsiDiskLoadConfig(int target_idx)
{
    // Then settings specific to target ID
    scsiDiskSetConfig(target_idx);

    // Check if we have image specified by name
    char filename[MAX_FILE_PATH];
    image_config_t &img = g_DiskImages[target_idx];
    img.image_index = IMAGE_INDEX_MAX;
    if (scsiDiskGetNextImageName(img, filename, sizeof(filename)))
    {
        // set the default block size now that we know the device type
        if (g_scsi_settings.getDevice(target_idx)->blockSize == 0)
        {
          g_scsi_settings.getDevice(target_idx)->blockSize = img.deviceType == S2S_CFG_OPTICAL ?  DEFAULT_BLOCKSIZE_OPTICAL : DEFAULT_BLOCKSIZE;
        }
        int blocksize = getBlockSize(filename, target_idx);
        logmsg("-- Opening '", filename, "' for id: ", target_idx);
        scsiDiskOpenHDDImage(target_idx, filename, 0, blocksize, (S2S_CFG_TYPE) img.deviceType, img.use_prefix);
    }
}

uint32_t getBlockSize(char *filename, uint8_t scsi_id)
{
    // Parse block size (HD00_NNNN)
    uint32_t block_size = g_scsi_settings.getDevice(scsi_id)->blockSize;
    const char *blksizestr = strchr(filename, '_');
    if (blksizestr)
    {
        int blktmp = strtoul(blksizestr + 1, NULL, 10);
        if (8 <= blktmp && blktmp <= 64 * 1024)
        {
            block_size = blktmp;
            logmsg("-- Using custom block size, ",(int) block_size," from filename: ", filename);
        }
    }
    return block_size;
}

uint8_t getEjectButton(uint8_t idx)
{
    return g_DiskImages[idx].ejectButton;
}

void setEjectButton(uint8_t idx, int8_t eject_button)
{
    g_DiskImages[idx].ejectButton = eject_button;
    g_scsi_settings.getDevice(idx)->ejectButton = eject_button;
}

// Check if we have multiple drive images to cycle when drive is ejected.
bool switchNextImage(image_config_t &img, const char* next_filename)
{
    // Check if we have a next image to load, so that drive is closed next time the host asks.
    
    int target_idx = img.scsiId & 7;
    char filename[MAX_FILE_PATH];
    if (next_filename == nullptr)
    {
        scsiDiskGetNextImageName(img, filename, sizeof(filename));
    }
    else
    {
        strncpy(filename, next_filename, MAX_FILE_PATH);
    }

#ifdef ENABLE_AUDIO_OUTPUT
    // if in progress for this device, terminate audio playback immediately (Annex C)
    audio_stop(target_idx);
    // Reset position tracking for the new image
    audio_get_status_code(target_idx); // trash audio status code
#endif

    if (filename[0] != '\0')
    {
        logmsg("Switching to next image for id ", target_idx, ": ", filename);
        img.file.close();

        // set default blocksize for CDs
        int block_size = getBlockSize(filename, target_idx);
        bool status = scsiDiskOpenHDDImage(target_idx, filename, 0, block_size, (S2S_CFG_TYPE) img.deviceType, img.use_prefix);

        if (status)
        {
            if (next_filename != nullptr)
            {
                // present the drive as ejected until the host queries it again,
                // to make sure host properly detects the media change
                img.ejected = true;
                img.reinsert_after_eject = true;
                if (img.deviceType == S2S_CFG_OPTICAL) img.cdrom_events = 2; // New Media
            }
            return true;
        }
    }
    else
    {
        logmsg("Switch to next image failed because target filename was empty.");
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
        if (img.ejectButton & buttonId)
        {
            if (img.deviceType == S2S_CFG_OPTICAL)
            {
                found = true;
                logmsg("Eject button ", (int)buttonId, " pressed, passing to CD drive SCSI", (int)i);
                cdromPerformEject(img);
            }
            else if (img.deviceType == S2S_CFG_ZIP100 
                    || img.deviceType == S2S_CFG_REMOVABLE 
                    || img.deviceType == S2S_CFG_FLOPPY_14MB 
                    || img.deviceType == S2S_CFG_MO
                    || img.deviceType == S2S_CFG_SEQUENTIAL)
            {
                found = true;
                logmsg("Eject button ", (int)buttonId, " pressed, passing to SCSI device", (int)i);
                doPerformEject(img);
            }
        }
    }

    if (!found)
    {
        logmsg("Eject button ", (int)buttonId, " pressed, but no drives with EjectButton=", (int)buttonId, " setting found!");
    }
}

uint8_t diskEjectButtonUpdate(bool immediate)
{
    // treat '1' to '0' transitions as eject actions
    static uint8_t previous = 0x00;
    uint8_t bitmask = platform_get_buttons() & EJECT_BTN_MASK;
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
                if (ejectors & mask) diskEjectAction(ejectors & mask);
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
        if (g_DiskImages[i].file.isOpen() && (g_DiskImages[i].scsiId & S2S_CFG_TARGET_ENABLED) && g_DiskImages[i].deviceType == S2S_CFG_NETWORK)
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

    if (SD.exists(CONFIGFILE))
    {
        logmsg("Reading configuration from " CONFIGFILE);
    }
    else
    {
        logmsg("Config file " CONFIGFILE " not found, using defaults");
    }

    // Get default values from system preset, if any
    ini_gets("SCSI", "System", "", tmp, sizeof(tmp), CONFIGFILE);
    scsi_system_settings_t *sysCfg = g_scsi_settings.initSystem(tmp);

    if (g_scsi_settings.getSystemPreset() != SYS_PRESET_NONE)
    {
        logmsg("Active configuration (using system preset \"", g_scsi_settings.getSystemPresetName(), "\"):");
    }
    else
    {
        logmsg("Active configuration:");
    }

    memset(config, 0, sizeof(S2S_BoardCfg));
    memcpy(config->magic, "BCFG", 4);
    config->flags = 0;
    config->startupDelay = 0;
    config->selectionDelay = sysCfg->selectionDelay;
    config->flags6 = 0;
    config->scsiSpeed = PLATFORM_MAX_SCSI_SPEED;
    int maxSyncSpeed = sysCfg->maxSyncSpeed;
    if (maxSyncSpeed < 5 && config->scsiSpeed > S2S_CFG_SPEED_ASYNC_50)
        config->scsiSpeed = S2S_CFG_SPEED_ASYNC_50;
    else if (maxSyncSpeed < 10 && config->scsiSpeed > S2S_CFG_SPEED_SYNC_5)
        config->scsiSpeed = S2S_CFG_SPEED_SYNC_5;
    else if (maxSyncSpeed < 20 && config->scsiSpeed > S2S_CFG_SPEED_SYNC_10)
        config->scsiSpeed = S2S_CFG_SPEED_SYNC_10;

    logmsg("-- SelectionDelay = ", (int)config->selectionDelay);

    if (sysCfg->enableUnitAttention)
    {
        logmsg("-- EnableUnitAttention = Yes");
        config->flags |= S2S_CFG_ENABLE_UNIT_ATTENTION;
    }
    else
    {
        logmsg("-- EnableUnitAttention = No");
    }

    if (sysCfg->enableSCSI2)
    {
        logmsg("-- EnableSCSI2 = Yes");
        config->flags |= S2S_CFG_ENABLE_SCSI2;
    }
    else
    {
        logmsg("-- EnableSCSI2 = No");
    }

    if (sysCfg->enableSelLatch)
    {
        logmsg("-- EnableSelLatch = Yes");
        config->flags |= S2S_CFG_ENABLE_SEL_LATCH;
    }
    else
    {
        logmsg("-- EnableSelLatch = No");
    }

    if (sysCfg->mapLunsToIDs)
    {
        logmsg("-- MapLunsToIDs = Yes");
        config->flags |= S2S_CFG_MAP_LUNS_TO_IDS;
    }
    else
    {
        logmsg("-- MapLunsToIDs = No");
    }

#ifdef PLATFORM_HAS_PARITY_CHECK
    if (sysCfg->enableParity)
    {
        logmsg("-- EnableParity = Yes");
        config->flags |= S2S_CFG_ENABLE_PARITY;
    }
    else
    {
        logmsg("-- EnableParity = No");
    }
#endif
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
            logmsg("Invalid MAC address format: \"", tmp, "\"");
            memset(config->wifiMACAddress, 0, sizeof(config->wifiMACAddress));
        }
    }

    memset(tmp, 0, sizeof(tmp));
    ini_gets("SCSI", "WiFiSSID", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0]) memcpy(config->wifiSSID, tmp, sizeof(config->wifiSSID));

    memset(tmp, 0, sizeof(tmp));
    ini_gets("SCSI", "WiFiPassword", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0]) memcpy(config->wifiPassword, tmp, sizeof(config->wifiPassword));

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

    if (unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_NETWORK))
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

	if (pmi && scsiDev.target->cfg->quirks == S2S_CFG_QUIRKS_EWSD)
	{
		highestBlock = 0x00001053;
	}
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
extern "C"
int doTestUnitReady()
{
    int ready = 1;
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
        ready = 0;
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
        scsiDev.phase = STATUS;

        if (img.reinsert_after_eject)
        {
            // We are now reporting to host that the drive is open.
            // Simulate a "close" for next time the host polls.
            if (img.deviceType == S2S_CFG_OPTICAL) cdromCloseTray(img);
            else doCloseTray(img);

        }
    }
    else if (unlikely(!(blockDev.state & DISK_PRESENT)))
    {
        ready = 0;
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = NOT_READY;
        scsiDev.target->sense.asc = MEDIUM_NOT_PRESENT;
        scsiDev.phase = STATUS;
    }
    else if (unlikely(!(blockDev.state & DISK_INITIALISED)))
    {
        ready = 0;
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
    uint32_t capacity = img.get_capacity_lba();

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
#ifdef ENABLE_AUDIO_OUTPUT
            if (scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL)
            {
                // Uses audio play with a length of 0. CD audio won't actually play,
                // but Read Subchannel will report the proper LBA location 
                if (!audio_play(scsiDev.target->targetId, &img, lba, 0, false))
                    dbgmsg("Failed to seek to audio track lba position ", (int) lba);
            }
#endif
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

    dbgmsg("------ Write ", (int)blocks, "x", (int)bytesPerSector, " starting at ", (int)lba);

    if (unlikely(blockDev.state & DISK_WP) ||
        unlikely(scsiDev.target->cfg->deviceType == S2S_CFG_OPTICAL) ||
        unlikely(!img.file.isWritable()))

    {
        logmsg("WARNING: Host attempted write to read-only drive ID ", (int)(img.scsiId & S2S_CFG_TARGET_ID_BITS));
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = WRITE_PROTECTED;
        scsiDev.phase = STATUS;
    }
    else if (unlikely(((uint64_t) lba) + blocks > capacity))
    {
        logmsg("WARNING: Host attempted write at sector ", (int)lba, "+", (int)blocks,
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
            logmsg("Seek to ", transfer.lba, " failed for SCSI ID", (int)scsiDev.target->targetId);
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

        // dbgmsg("SCSI read ", (int)start, " + ", (int)len);
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
            if (g_disk_transfer.parityError)
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
            // dbgmsg("SD write ", (int)start, " + ", (int)len, " ", bytearray(buf, len));
            platform_set_sd_callback(&diskDataOut_callback, buf);
            if (img.file.write(buf, len) != len)
            {
                logmsg("SD card write failed: ", SD.sdErrorCode());
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

    dbgmsg("------ Read ", (int)blocks, "x", (int)bytesPerSector, " starting at ", (int)lba);

    if (unlikely(((uint64_t) lba) + blocks > capacity))
    {
        logmsg("WARNING: Host attempted read at sector ", (int)lba, "+", (int)blocks,
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
            dbgmsg("------ Found ", (int)count, " sectors in prefetch cache");
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
            logmsg("Seek to ", transfer.lba, " failed for SCSI ID", (int)scsiDev.target->targetId);
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
            logmsg("start_dataInTransfer() timeout waiting for previous to finish");
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
        logmsg("SD card read failed: ", SD.sdErrorCode());
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
                logmsg("Prefetch read failed");
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
        if ((scsiDev.cdb[4] & 2) || img.deviceType == S2S_CFG_ZIP100)
        {
            // Device load & eject
            if (start)
            {
                doCloseTray(img);
            }
            else
            {
                // Eject and switch image
                doPerformEject(img);
            }
        }
        else if (start)
        {
            scsiDev.target->started = 1;
        }
        else
        {
            scsiDev.target->started = 0;
        }

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
            if (img.reinsert_on_inquiry)
            {
                if (img.deviceType == S2S_CFG_OPTICAL) cdromCloseTray(img);
                else doCloseTray(img);
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

