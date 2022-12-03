// SCSI disk access routines
// Implements both SCSI2SD V6 disk.h functions and some extra.

#pragma once

#include <stdint.h>
#include <scsi2sd.h>
#include <scsiPhy.h>

extern "C" {
#include <disk.h>
#include <config.h>
#include <scsi.h>
}

void scsiDiskResetImages();
bool scsiDiskOpenHDDImage(int target_idx, const char *filename, int scsi_id, int scsi_lun, int blocksize, S2S_CFG_TYPE type = S2S_CFG_FIXED);
void scsiDiskLoadConfig(int target_idx);

// Program ROM drive and rename image file
bool scsiDiskProgramRomDrive(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type);

// Check if there is ROM drive configured in microcontroller flash
bool scsiDiskCheckRomDrive();
bool scsiDiskActivateRomDrive();

// Returns true if there is at least one image active
bool scsiDiskCheckAnyImagesConfigured();