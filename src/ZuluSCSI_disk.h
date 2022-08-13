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
bool scsiDiskOpenHDDImage(int target_idx, const char *filename, int scsi_id, int scsi_lun, int blocksize, bool is_cd, bool is_fd);
void scsiDiskLoadConfig(int target_idx);
