// This file implements the main SCSI disk emulation and data streaming.
// It is derived from disk.c in SCSI2SD V6.
//
//    Licensed under GPL v3.
//    Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
//    Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com>
//    Copyright (C) 2022 Rabbit Hole Computing

#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
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

#ifndef PLATFORM_HAS_ROM_DRIVE
// Dummy defines for platforms without ROM drive support
#define PLATFORM_ROMDRIVE_PAGE_SIZE 1024
uint32_t platform_get_romdrive_maxsize() { return 0; }
bool platform_read_romdrive(uint8_t *dest, uint32_t start, uint32_t count) { return false; }
bool platform_write_romdrive(const uint8_t *data, uint32_t start, uint32_t count) { return false; }
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

// SD card sector size is always 512 bytes
#define SD_SECTOR_SIZE 512

/************************************************/
/* ROM drive support (in microcontroller flash) */
/************************************************/

struct romdrive_hdr_t {
    char magic[8]; // "ROMDRIVE"
    int scsi_id;
    uint32_t imagesize;
    uint32_t blocksize;
    S2S_CFG_TYPE drivetype;
    uint32_t reserved[32];
};

// Check if the romdrive is present
static bool check_romdrive(romdrive_hdr_t *hdr)
{
    if (!platform_read_romdrive((uint8_t*)hdr, 0, sizeof(romdrive_hdr_t)))
    {
        return false;
    }

    if (memcmp(hdr->magic, "ROMDRIVE", 8) != 0)
    {
        return false;
    }

    if (hdr->imagesize <= 0 || hdr->scsi_id < 0 || hdr->scsi_id > 8)
    {
        return false;
    }

    return true;
}

// Clear the drive metadata header
bool scsiDiskClearRomDrive()
{
#ifndef PLATFORM_HAS_ROM_DRIVE
    logmsg("---- Platform does not support ROM drive");
    return false;
#else
    romdrive_hdr_t hdr = {0x0};

    if (!platform_write_romdrive((const uint8_t*)&hdr, 0, PLATFORM_ROMDRIVE_PAGE_SIZE))
    {
        logmsg("-- Failed to clear ROM drive");
        return false;
    }
    logmsg("-- Cleared ROM drive");
    SD.remove("CLEAR_ROM");
    return true;
#endif
}

// Load an image file to romdrive
bool scsiDiskProgramRomDrive(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type)
{
#ifndef PLATFORM_HAS_ROM_DRIVE
    logmsg("---- Platform does not support ROM drive");
    return false;
#endif

    FsFile file = SD.open(filename, O_RDONLY);
    if (!file.isOpen())
    {
        logmsg("---- Failed to open: ", filename);
        return false;
    }

    uint64_t filesize = file.size();
    uint32_t maxsize = platform_get_romdrive_maxsize() - PLATFORM_ROMDRIVE_PAGE_SIZE;

    logmsg("---- SCSI ID: ", scsi_id, " blocksize ", blocksize, " type ", (int)type);
    logmsg("---- ROM drive maximum size is ", (int)maxsize,
          " bytes, image file is ", (int)filesize, " bytes");
    
    if (filesize > maxsize)
    {
        logmsg("---- Image size exceeds ROM space, not loading");
        file.close();
        return false;
    }

    romdrive_hdr_t hdr = {};
    memcpy(hdr.magic, "ROMDRIVE", 8);
    hdr.scsi_id = scsi_id;
    hdr.imagesize = filesize;
    hdr.blocksize = blocksize;
    hdr.drivetype = type;

    // Program the drive metadata header
    if (!platform_write_romdrive((const uint8_t*)&hdr, 0, PLATFORM_ROMDRIVE_PAGE_SIZE))
    {
        logmsg("---- Failed to program ROM drive header");
        file.close();
        return false;
    }
    
    // Program the drive contents
    uint32_t pages = (filesize + PLATFORM_ROMDRIVE_PAGE_SIZE - 1) / PLATFORM_ROMDRIVE_PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++)
    {
        if (i % 2)
            LED_ON();
        else
            LED_OFF();

        if (file.read(scsiDev.data, PLATFORM_ROMDRIVE_PAGE_SIZE) <= 0 ||
            !platform_write_romdrive(scsiDev.data, (i + 1) * PLATFORM_ROMDRIVE_PAGE_SIZE, PLATFORM_ROMDRIVE_PAGE_SIZE))
        {
            logmsg("---- Failed to program ROM drive page ", (int)i);
            file.close();
            return false;
        }
    }

    LED_OFF();

    file.close();

    char newname[MAX_FILE_PATH * 2] = "";
    strlcat(newname, filename, sizeof(newname));
    strlcat(newname, "_loaded", sizeof(newname));
    SD.rename(filename, newname);
    logmsg("---- ROM drive programming successful, image file renamed to ", newname);

    return true;
}

bool scsiDiskCheckRomDrive()
{
    romdrive_hdr_t hdr = {};
    return check_romdrive(&hdr);
}

// Check if rom drive exists and activate it
bool scsiDiskActivateRomDrive()
{
#ifndef PLATFORM_HAS_ROM_DRIVE
    return false;
#endif

    uint32_t maxsize = platform_get_romdrive_maxsize() - PLATFORM_ROMDRIVE_PAGE_SIZE;
    logmsg("-- Platform supports ROM drive up to ", (int)(maxsize / 1024), " kB");

    romdrive_hdr_t hdr = {};
    if (!check_romdrive(&hdr))
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
    bool status = scsiDiskOpenHDDImage(hdr.scsi_id, "ROM:", hdr.scsi_id, 0, hdr.blocksize, hdr.drivetype);

    if (!status)
    {
        logmsg("---- ROM drive activation failed");
        return false;
    }
    else
    {
        return true;
    }
}


/***********************/
/* Backing image files */
/***********************/

extern SdFs SD;
SdDevice sdDev = {2, 256 * 1024 * 1024 * 2}; /* For SCSI2SD */

// This class wraps SdFat library FsFile to allow access
// through either FAT filesystem or as a raw sector range.
//
// Raw access is activated by using filename like "RAW:0:12345"
// where the numbers are the first and last sector.
//
// If the platform supports a ROM drive, it is activated by using
// filename "ROM:".
class ImageBackingStore
{
public:
    ImageBackingStore()
    {
        m_israw = false;
        m_isrom = false;
        m_isreadonly_attr = false;
        m_blockdev = nullptr;
        m_bgnsector = m_endsector = m_cursector = 0;
    }

    ImageBackingStore(const char *filename, uint32_t scsi_block_size): ImageBackingStore()
    {
        if (strncasecmp(filename, "RAW:", 4) == 0)
        {
            char *endptr, *endptr2;
            m_bgnsector = strtoul(filename + 4, &endptr, 0);
            m_endsector = strtoul(endptr + 1, &endptr2, 0);

            if (*endptr != ':' || *endptr2 != '\0')
            {
                logmsg("Invalid format for raw filename: ", filename);
                return;
            }

            if ((scsi_block_size % SD_SECTOR_SIZE) != 0)
            {
                logmsg("SCSI block size ", (int)scsi_block_size, " is not supported for RAW partitions (must be divisible by 512 bytes)");
                return;
            }

            m_israw = true;
            m_blockdev = SD.card();

            uint32_t sectorCount = SD.card()->sectorCount();
            if (m_endsector >= sectorCount)
            {
                logmsg("Limiting RAW image mapping to SD card sector count: ", (int)sectorCount);
                m_endsector = sectorCount - 1;
            }
        }
        else if (strncasecmp(filename, "ROM:", 4) == 0)
        {
            if (!check_romdrive(&m_romhdr))
            {
                m_romhdr.imagesize = 0;
            }
            else
            {
                m_isrom = true;
            }
        }
        else
        {
            m_isreadonly_attr = !!(FS_ATTRIB_READ_ONLY & SD.attrib(filename));
            if (m_isreadonly_attr)
            {
                m_fsfile = SD.open(filename, O_RDONLY);
                logmsg("---- Image file is read-only, writes disabled");
            }
            else
            {
                m_fsfile = SD.open(filename, O_RDWR);
            }

            uint32_t sectorcount = m_fsfile.size() / SD_SECTOR_SIZE;
            uint32_t begin = 0, end = 0;
            if (m_fsfile.contiguousRange(&begin, &end) && end >= begin + sectorcount
                && (scsi_block_size % SD_SECTOR_SIZE) == 0)
            {
                // Convert to raw mapping, this avoids some unnecessary
                // access overhead in SdFat library.
                m_israw = true;
                m_blockdev = SD.card();
                m_bgnsector = begin;

                if (end != begin + sectorcount)
                {
                    uint32_t allocsize = end - begin + 1;
                    // Due to issue #80 in ZuluSCSI version 1.0.8 and 1.0.9 the allocated size was mistakenly reported to SCSI controller.
                    // If the drive was formatted using those versions, you may have problems accessing it with newer firmware.
                    // The old behavior can be restored with setting  [SCSI] UseFATAllocSize = 1 in config file.

                    if (ini_getbool("SCSI", "UseFATAllocSize", 0, CONFIGFILE))
                    {
                        sectorcount = allocsize;
                    }
                }

                m_endsector = begin + sectorcount - 1;
                m_fsfile.close();
            }
        }
    }

    bool isWritable()
    {
        return !m_isrom && !m_isreadonly_attr;
    }

    bool isRom()
    {
        return m_isrom;
    }

    bool isOpen()
    {
        if (m_israw)
            return (m_blockdev != NULL);
        else if (m_isrom)
            return (m_romhdr.imagesize > 0);
        else
            return m_fsfile.isOpen();
    }

    bool close()
    {
        if (m_israw)
        {
            m_blockdev = nullptr;
            return true;
        }
        else if (m_isrom)
        {
            m_romhdr.imagesize = 0;
            return true;
        }
        else
        {
            return m_fsfile.close();
        }
    }

    uint64_t size()
    {
        if (m_israw && m_blockdev)
        {
            return (uint64_t)(m_endsector - m_bgnsector + 1) * SD_SECTOR_SIZE;
        }
        else if (m_isrom)
        {
            return m_romhdr.imagesize;
        }
        else
        {
            return m_fsfile.size();
        }
    }

    bool contiguousRange(uint32_t* bgnSector, uint32_t* endSector)
    {
        if (m_israw && m_blockdev)
        {
            *bgnSector = m_bgnsector;
            *endSector = m_endsector;
            return true;
        }
        else if (m_isrom)
        {
            *bgnSector = 0;
            *endSector = 0;
            return true;
        }
        else
        {
            return m_fsfile.contiguousRange(bgnSector, endSector);
        }
    }

    bool seek(uint64_t pos)
    {
        if (m_israw)
        {
            uint32_t sectornum = pos / SD_SECTOR_SIZE;
            assert((uint64_t)sectornum * SD_SECTOR_SIZE == pos);
            m_cursector = m_bgnsector + sectornum;
            return (m_cursector <= m_endsector);
        }
        else if (m_isrom)
        {
            uint32_t sectornum = pos / SD_SECTOR_SIZE;
            assert((uint64_t)sectornum * SD_SECTOR_SIZE == pos);
            m_cursector = sectornum;
            return m_cursector * SD_SECTOR_SIZE < m_romhdr.imagesize;
        }
        else
        {
            return m_fsfile.seek(pos);
        }
    }

    int read(void* buf, size_t count)
    {
        if (m_israw && m_blockdev)
        {
            uint32_t sectorcount = count / SD_SECTOR_SIZE;
            assert((uint64_t)sectorcount * SD_SECTOR_SIZE == count);
            if (m_blockdev->readSectors(m_cursector, (uint8_t*)buf, sectorcount))
            {
                m_cursector += sectorcount;
                return count;
            }
            else
            {
                return -1;
            }
        }
        else if (m_isrom)
        {
            uint32_t sectorcount = count / SD_SECTOR_SIZE;
            assert((uint64_t)sectorcount * SD_SECTOR_SIZE == count);
            uint32_t start = m_cursector * SD_SECTOR_SIZE + PLATFORM_ROMDRIVE_PAGE_SIZE;
            if (platform_read_romdrive((uint8_t*)buf, start, count))
            {
                m_cursector += sectorcount;
                return count;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return m_fsfile.read(buf, count);
        }
    }

    size_t write(const void* buf, size_t count)
    {
        if (m_israw && m_blockdev)
        {
            uint32_t sectorcount = count / SD_SECTOR_SIZE;
            assert((uint64_t)sectorcount * SD_SECTOR_SIZE == count);
            if (m_blockdev->writeSectors(m_cursector, (const uint8_t*)buf, sectorcount))
            {
                m_cursector += sectorcount;
                return count;
            }
            else
            {
                return 0;
            }
        }
        else if (m_isrom)
        {
            logmsg("ERROR: attempted to write to ROM drive");
            return 0;
        }
        else  if (m_isreadonly_attr)
        {
            logmsg("ERROR: attempted to write to a read only image");
            return 0;
        }
        else
        {
            return m_fsfile.write(buf, count);
        }
    }

    void flush()
    {
        if (!m_israw && !m_isrom && !m_isreadonly_attr)
        {
            m_fsfile.flush();
        }
    }

private:
    bool m_israw;
    bool m_isrom;
    bool m_isreadonly_attr;
    romdrive_hdr_t m_romhdr;
    FsFile m_fsfile;
    SdCard *m_blockdev;
    uint32_t m_bgnsector;
    uint32_t m_endsector;
    uint32_t m_cursector;
};

struct image_config_t: public S2S_TargetCfg
{
    ImageBackingStore file;

    // For CD-ROM drive ejection
    bool ejected;
    uint8_t cdrom_events;
    bool reinsert_on_inquiry;

    // Index of image, for when image on-the-fly switching is used for CD drives
    int image_index;

    // Right-align vendor / product type strings (for Apple)
    // Standard SCSI uses left alignment
    // This field uses -1 for default when field is not set in .ini
    int rightAlignStrings;

    // Maximum amount of bytes to prefetch
    int prefetchbytes;

    // Warning about geometry settings
    bool geometrywarningprinted;
};

static image_config_t g_DiskImages[S2S_MAX_TARGETS];

void scsiDiskResetImages()
{
    memset(g_DiskImages, 0, sizeof(g_DiskImages));
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
    static const char *driveinfo_tape[4]      = DRIVEINFO_TAPE;

    static const char *apl_driveinfo_fixed[4]     = APPLE_DRIVEINFO_FIXED;
    static const char *apl_driveinfo_removable[4] = APPLE_DRIVEINFO_REMOVABLE;
    static const char *apl_driveinfo_optical[4]   = APPLE_DRIVEINFO_OPTICAL;
    static const char *apl_driveinfo_floppy[4]    = APPLE_DRIVEINFO_FLOPPY;
    static const char *apl_driveinfo_magopt[4]    = APPLE_DRIVEINFO_MAGOPT;
    static const char *apl_driveinfo_tape[4]      = APPLE_DRIVEINFO_TAPE;

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
            case S2S_CFG_SEQUENTIAL:    driveinfo = apl_driveinfo_tape; break;
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
            case S2S_CFG_SEQUENTIAL:    driveinfo = driveinfo_tape; break;
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

bool scsiDiskOpenHDDImage(int target_idx, const char *filename, int scsi_id, int scsi_lun, int blocksize, S2S_CFG_TYPE type)
{
    image_config_t &img = g_DiskImages[target_idx];
    img.file = ImageBackingStore(filename, blocksize);

    if (img.file.isOpen())
    {
        img.bytesPerSector = blocksize;
        img.scsiSectors = img.file.size() / blocksize;
        img.scsiId = scsi_id | S2S_CFG_TARGET_ENABLED;
        img.sdSectorStart = 0;
        
        if (img.scsiSectors == 0)
        {
            logmsg("---- Error: image file ", filename, " is empty");
            img.file.close();
            return false;
        }

        uint32_t sector_begin = 0, sector_end = 0;
        if (img.file.isRom())
        {
            // ROM is always contiguous, no need to log
        }
        else if (img.file.contiguousRange(&sector_begin, &sector_end))
        {
            dbgmsg("---- Image file is contiguous, SD card sectors ", (int)sector_begin, " to ", (int)sector_end);
        }
        else
        {
            logmsg("---- WARNING: file ", filename, " is not contiguous. This will increase read latency.");
        }

        if (type == S2S_CFG_OPTICAL)
        {
            logmsg("---- Configuring as CD-ROM drive based on image name");
            img.deviceType = S2S_CFG_OPTICAL;
        }
        else if (type == S2S_CFG_FLOPPY_14MB)
        {
            logmsg("---- Configuring as floppy drive based on image name");
            img.deviceType = S2S_CFG_FLOPPY_14MB;
        }
        else if (type == S2S_CFG_MO)
        {
            logmsg("---- Configuring as magneto-optical based on image name");
            img.deviceType = S2S_CFG_MO;
        }
        else if (type == S2S_CFG_REMOVEABLE)
        {
            logmsg("---- Configuring as removable drive based on image name");
            img.deviceType = S2S_CFG_REMOVEABLE;
        }
        else if (type == S2S_CFG_SEQUENTIAL)
        {
            logmsg("---- Configuring as tape drive based on image name");
            img.deviceType = S2S_CFG_SEQUENTIAL;
        }

#ifdef PLATFORM_CONFIG_HOOK
        PLATFORM_CONFIG_HOOK(&img);
#endif

        setDefaultDriveInfo(target_idx);

        if (img.prefetchbytes > 0)
        {
            logmsg("---- Read prefetch enabled: ", (int)img.prefetchbytes, " bytes");
        }
        else
        {
            logmsg("---- Read prefetch disabled");
        }

        return true;
    }

    return false;
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

// Set target configuration to default values
static void scsiDiskConfigDefaults(int target_idx)
{
    image_config_t &img = g_DiskImages[target_idx];
    img.deviceType = S2S_CFG_FIXED;
    img.deviceTypeModifier = 0;
    img.sectorsPerTrack = 63;
    img.headsPerCylinder = 255;
    img.quirks = S2S_CFG_QUIRKS_NONE;
    img.prefetchbytes = PREFETCH_BUFFER_SIZE;
    memset(img.vendor, 0, sizeof(img.vendor));
    memset(img.prodId, 0, sizeof(img.prodId));
    memset(img.revision, 0, sizeof(img.revision));
    memset(img.serial, 0, sizeof(img.serial));
}

// Load values for target configuration from given section if they exist.
// Otherwise keep current settings.
static void scsiDiskLoadConfig(int target_idx, const char *section)
{
    image_config_t &img = g_DiskImages[target_idx];
    img.deviceType = ini_getl(section, "Type", img.deviceType, CONFIGFILE);
    img.deviceTypeModifier = ini_getl(section, "TypeModifier", img.deviceTypeModifier, CONFIGFILE);
    img.sectorsPerTrack = ini_getl(section, "SectorsPerTrack", img.sectorsPerTrack, CONFIGFILE);
    img.headsPerCylinder = ini_getl(section, "HeadsPerCylinder", img.headsPerCylinder, CONFIGFILE);
    img.quirks = ini_getl(section, "Quirks", img.quirks, CONFIGFILE);
    img.rightAlignStrings = ini_getbool(section, "RightAlignStrings", 0, CONFIGFILE);
    img.prefetchbytes = ini_getl(section, "PrefetchBytes", img.prefetchbytes, CONFIGFILE);
    img.reinsert_on_inquiry = ini_getbool(section, "ReinsertCDOnInquiry", 1, CONFIGFILE);
    
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
}

// Check if image file name is overridden in config
static bool get_image_name(int target_idx, char *buf, size_t buflen)
{
    image_config_t &img = g_DiskImages[target_idx];

    char section[6] = "SCSI0";
    section[4] = '0' + target_idx;

    char key[5] = "IMG0";
    key[3] = '0' + img.image_index;

    ini_gets(section, key, "", buf, buflen, CONFIGFILE);
    return buf[0] != '\0';
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
    if (get_image_name(target_idx, filename, sizeof(filename)))
    {
        image_config_t &img = g_DiskImages[target_idx];
        int blocksize = (img.deviceType == S2S_CFG_OPTICAL) ? 2048 : 512;
        logmsg("-- Opening ", filename, " for id:", target_idx, ", specified in " CONFIGFILE);
        scsiDiskOpenHDDImage(target_idx, filename, target_idx, 0, blocksize);
    }
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

/*******************************/
/* Config handling for SCSI2SD */
/*******************************/

extern "C"
void s2s_configInit(S2S_BoardCfg* config)
{
    if (SD.exists(CONFIGFILE))
    {
        logmsg("Reading configuration from " CONFIGFILE);
    }
    else
    {
        logmsg("Config file " CONFIGFILE " not found, using defaults");
    }

    logmsg("Active configuration:");
    memset(config, 0, sizeof(S2S_BoardCfg));
    memcpy(config->magic, "BCFG", 4);
    config->flags = 0;
    config->startupDelay = 0;
    config->selectionDelay = ini_getl("SCSI", "SelectionDelay", 255, CONFIGFILE);
    config->flags6 = 0;
    config->scsiSpeed = PLATFORM_MAX_SCSI_SPEED;

    int maxSyncSpeed = ini_getl("SCSI", "MaxSyncSpeed", 10, CONFIGFILE);
    if (maxSyncSpeed < 5 && config->scsiSpeed > S2S_CFG_SPEED_ASYNC_50)
        config->scsiSpeed = S2S_CFG_SPEED_ASYNC_50;
    else if (maxSyncSpeed < 10 && config->scsiSpeed > S2S_CFG_SPEED_SYNC_5)
        config->scsiSpeed = S2S_CFG_SPEED_SYNC_5;
    
    logmsg("-- SelectionDelay: ", (int)config->selectionDelay);

    if (ini_getbool("SCSI", "EnableUnitAttention", false, CONFIGFILE))
    {
        logmsg("-- EnableUnitAttention is on");
        config->flags |= S2S_CFG_ENABLE_UNIT_ATTENTION;
    }

    if (ini_getbool("SCSI", "EnableSCSI2", true, CONFIGFILE))
    {
        logmsg("-- EnableSCSI2 is on");
        config->flags |= S2S_CFG_ENABLE_SCSI2;
    }

    if (ini_getbool("SCSI", "EnableSelLatch", false, CONFIGFILE))
    {
        logmsg("-- EnableSelLatch is on");
        config->flags |= S2S_CFG_ENABLE_SEL_LATCH;
    }

    if (ini_getbool("SCSI", "MapLunsToIDs", false, CONFIGFILE))
    {
        logmsg("-- MapLunsToIDs is on");
        config->flags |= S2S_CFG_MAP_LUNS_TO_IDS;
    }

#ifdef PLATFORM_HAS_PARITY_CHECK
    if (ini_getbool("SCSI", "EnableParity", true, CONFIGFILE))
    {
        logmsg("-- EnableParity is on");
        config->flags |= S2S_CFG_ENABLE_PARITY;
    }
    else
    {
        logmsg("-- EnableParity is off");
    }
#endif
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
    uint32_t capacity = img.file.size() / bytesPerSector;

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

// Check if we have multiple CD-ROM images to cycle when drive is ejected.
static bool checkNextCDImage()
{
    // Check if we have a next image to load, so that drive is closed next time the host asks.
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
    img.image_index++;
    char filename[MAX_FILE_PATH];
    int target_idx = img.scsiId & 7;
    if (!get_image_name(target_idx, filename, sizeof(filename)))
    {
        img.image_index = 0;
        get_image_name(target_idx, filename, sizeof(filename));
    }

    if (filename[0] != '\0')
    {
        logmsg("Switching to next CD-ROM image for ", target_idx, ": ", filename);
        image_config_t &img = g_DiskImages[target_idx];
        img.file.close();
        bool status = scsiDiskOpenHDDImage(target_idx, filename, target_idx, 0, 2048);

        if (status)
        {
            img.ejected = false;
            img.cdrom_events = 2; // New media
            return true;
        }
    }

    return false;
}

// Reinsert any ejected CDROMs on reboot
static void reinsertCDROM(image_config_t &img)
{
    if (img.image_index > 0)
    {
        // Multiple images for this drive, force restart from first one
        dbgmsg("---- Restarting from first CD-ROM image");
        img.image_index = 9;
        checkNextCDImage();
    }
    else if (img.ejected)
    {
        // Reinsert the single image
        dbgmsg("---- Closing CD-ROM tray");
        img.ejected = false;
        img.cdrom_events = 2; // New media
    }
}

static int doTestUnitReady()
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

        // We are now reporting to host that the drive is open.
        // Simulate a "close" for next time the host polls.
        checkNextCDImage();
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

static void doGetEventStatusNotification(bool immed)
{
    image_config_t &img = *(image_config_t*)scsiDev.target->cfg;

    if (!immed)
    {
        // Asynchronous notification not supported
        scsiDev.status = CHECK_CONDITION;
        scsiDev.target->sense.code = ILLEGAL_REQUEST;
        scsiDev.target->sense.asc = INVALID_FIELD_IN_CDB;
        scsiDev.phase = STATUS;
    }
    else if (img.cdrom_events)
    {
        scsiDev.data[0] = 0;
        scsiDev.data[1] = 6; // EventDataLength
        scsiDev.data[2] = 0x04; // Media status events
        scsiDev.data[3] = 0x04; // Supported events
        scsiDev.data[4] = img.cdrom_events;
        scsiDev.data[5] = 0x01; // Power status
        scsiDev.data[6] = 0; // Start slot
        scsiDev.data[7] = 0; // End slot
        scsiDev.dataLen = 8;
        scsiDev.phase = DATA_IN;
        img.cdrom_events = 0;

        if (img.ejected)
        {
            // We are now reporting to host that the drive is open.
            // Simulate a "close" for next time the host polls.
            checkNextCDImage();
        }
    }
    else
    {
        scsiDev.data[0] = 0;
        scsiDev.data[1] = 2; // EventDataLength
        scsiDev.data[2] = 0x00; // Media status events
        scsiDev.data[3] = 0x04; // Supported events
        scsiDev.dataLen = 4;
        scsiDev.phase = DATA_IN;
    }
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

static void doWrite(uint32_t lba, uint32_t blocks)
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

static void doRead(uint32_t lba, uint32_t blocks)
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

        while (!scsiIsWriteFinished(NULL) && prefetch_sectors > 0)
        {
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
	    int loadEject = scsiDev.cdb[4] & 2;
	
        if (loadEject && img.deviceType == S2S_CFG_OPTICAL)
        {
            if (start)
            {
                dbgmsg("------ CDROM close tray");
                img.ejected = false;
                img.cdrom_events = 2; // New media
            }
            else
            {
                dbgmsg("------ CDROM open tray");
                img.ejected = true;
                img.cdrom_events = 3; // Media removal
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
    else if (unlikely(command == 0x00))
    {
        // TEST UNIT READY
        doTestUnitReady();
    }
    else if (command == 0x4A)
    {
        bool immed = scsiDev.cdb[1] & 1;
        doGetEventStatusNotification(immed);
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
        doRead(lba, blocks);
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

        doRead(lba, blocks);
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
        doWrite(lba, blocks);
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

        doWrite(lba, blocks);
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

        // Check for Inquiry command to reinsert CD-ROMs on boot
        if (command == 0x12)
        {
            image_config_t &img = *(image_config_t*)scsiDev.target->cfg;
            if (img.deviceType == S2S_CFG_OPTICAL && img.reinsert_on_inquiry)
            {
                reinsertCDROM(img);
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

    // Reinsert any ejected CD-ROMs
    for (int i = 0; i < S2S_MAX_TARGETS; ++i)
    {
        image_config_t &img = g_DiskImages[i];
        if (img.deviceType == S2S_CFG_OPTICAL)
        {
            reinsertCDROM(img);
        }
    }
}

extern "C"
void scsiDiskInit()
{
    scsiDiskReset();
}

