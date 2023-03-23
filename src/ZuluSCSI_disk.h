/** 
 * SCSI2SD V6 - Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
 * Copyright (C) 2014 Doug Brown <doug@downtowndougbrown.com
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
 * 
 * It is derived from disk.h in SCSI2SD V6.
 * 
 * This file is licensed under the GPL version 3 or any later version. 
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

// Clear the ROM drive header from flash
bool scsiDiskClearRomDrive();
// Program ROM drive and rename image file
bool scsiDiskProgramRomDrive(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type);

// Check if there is ROM drive configured in microcontroller flash
bool scsiDiskCheckRomDrive();
bool scsiDiskActivateRomDrive();

// Returns true if there is at least one image active
bool scsiDiskCheckAnyImagesConfigured();