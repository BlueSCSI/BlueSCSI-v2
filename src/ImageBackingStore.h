/* Access layer to image files associated with a SCSI device.
 * Currently supported image storage modes:
 *
 * - Files on SD card
 * - Raw SD card partitions
 * - Microcontroller flash ROM drive
 */

#pragma once
#include <stdint.h>
#include <unistd.h>
#include <SdFat.h>
#include "ROMDrive.h"

extern "C" {
#include <scsi.h>
}

// SD card sector size is always 512 bytes
extern SdFs SD;
#define SD_SECTOR_SIZE 512

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
    // Empty image, cannot be accessed
    ImageBackingStore();

    // Parse image file parameters from filename.
    // Special filename formats:
    //    RAW:start:end
    //    ROM:
    ImageBackingStore(const char *filename, uint32_t scsi_block_size);

    // Can the image be read?
    bool isOpen();

    // Can the image be written?
    bool isWritable();

    // Is this internal ROM drive in microcontroller flash?
    bool isRom();

    // Close the image so that .isOpen() will return false.
    bool close();

    // Return image size in bytes
    uint64_t size();

    // Check if the image sector range is contiguous, and the image is on
    // SD card, return the sector numbers.
    bool contiguousRange(uint32_t* bgnSector, uint32_t* endSector);

    // Set current position for following read/write operations
    bool seek(uint64_t pos);

    // Read data from the image file, returns number of bytes read, or negative on error.
    ssize_t read(void* buf, size_t count);

    // Write data to image file, returns number of bytes written, or negative on error.
    ssize_t write(const void* buf, size_t count);

    // Flush any pending changes to filesystem
    void flush();

    // Gets current position for following read/write operations
    // Result is only valid for regular files, not raw or flash access
    uint64_t position();

protected:
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
