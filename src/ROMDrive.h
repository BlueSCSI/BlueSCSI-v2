/* Access layer to microcontroller internal flash ROM drive storage.
 * Can store a small disk image.
 */

#ifndef ROMDRIVE_H
#define ROMDRIVE_H
#include <stdint.h>
#include <unistd.h>
#include <scsi2sd.h>

// Header used for storing the rom drive parameters in flash
struct romdrive_hdr_t {
    char magic[8]; // "ROMDRIVE"
    int scsi_id;
    uint32_t imagesize;
    uint32_t blocksize;
    S2S_CFG_TYPE drivetype;
    uint32_t reserved[32];
};

// Return true if ROM drive is found.
// If hdr is not NULL, it will receive the ROM drive header information.
// If flash is empty, returns false.
bool romDriveCheckPresent(romdrive_hdr_t *hdr = nullptr);

// Clear any existing ROM drive, returning flash to empty state
bool romDriveClear();

// Program ROM drive image to flash
bool romDriveProgram(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type);

// Read data from rom drive main data area
bool romDriveRead(uint8_t *buf, uint32_t start, uint32_t count);

#endif /* ROMDRIVE_H */
