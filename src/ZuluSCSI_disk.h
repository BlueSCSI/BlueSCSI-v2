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
#include "ImageBackingStore.h"
#include "ZuluSCSI_config.h"

extern "C" {
#include <disk.h>
#include <config.h>
#include <scsi.h>
}

#define IMAGE_INDEX_MAX 9

// Extended configuration stored alongside the normal SCSI2SD target information
struct image_config_t: public S2S_TargetCfg
{
    // There should be only one global instance of this struct per device, so disallow copy constructor.
    image_config_t() = default;
    image_config_t(const image_config_t&) = delete;

    ImageBackingStore file;

    // For CD-ROM drive ejection
    bool ejected;
    uint8_t cdrom_events;
    bool reinsert_on_inquiry; // Reinsert on Inquiry command (to reinsert automatically after boot)
    bool reinsert_after_eject; // Reinsert next image after ejection

    // selects a physical button channel that will cause an eject action
    // default option of '0' disables this functionality
    uint8_t ejectButton;

    // For tape drive emulation, current position in blocks
    uint32_t tape_pos;

    // True if there is a subdirectory of images for this target
    bool image_directory;
    // the name of the currently mounted image in a dynamic image directory
    char current_image[MAX_FILE_PATH];
    // Index of image, for when image on-the-fly switching is used for CD drives
    uint8_t image_index;

    // Cue sheet file for CD-ROM images
    FsFile cuesheetfile;

    // Right-align vendor / product type strings (for Apple)
    // Standard SCSI uses left alignment
    // This field uses -1 for default when field is not set in .ini
    int rightAlignStrings;

    // Maximum amount of bytes to prefetch
    int prefetchbytes;

    // Warning about geometry settings
    bool geometrywarningprinted;
};

// Should be polled intermittently to update the platform eject buttons.
// Call with 'true' only if ejections should be performed immediately (typically when not busy)
// Returns a mask of the buttons that registered an 'eject' action.
uint8_t diskEjectButtonUpdate(bool immediate);

// Reset all image configuration to empty reset state, close all images.
void scsiDiskResetImages();

// Close any files opened from SD card (prepare for remounting SD)
void scsiDiskCloseSDCardImages();

bool scsiDiskOpenHDDImage(int target_idx, const char *filename, int scsi_id, int scsi_lun, int blocksize, S2S_CFG_TYPE type = S2S_CFG_FIXED);
void scsiDiskLoadConfig(int target_idx);

// During loading, checks if a filename is appropriate for further processing as a disk image
bool scsiDiskFilenameValid(const char* name);

// Clear the ROM drive header from flash
bool scsiDiskClearRomDrive();
// Program ROM drive and rename image file
bool scsiDiskProgramRomDrive(const char *filename, int scsi_id, int blocksize, S2S_CFG_TYPE type);

// Check if there is ROM drive configured in microcontroller flash
bool scsiDiskCheckRomDrive();
bool scsiDiskActivateRomDrive();

// Returns true if there is at least one image active
bool scsiDiskCheckAnyImagesConfigured();

// Gets the next image filename for the target, if configured for multiple
// images. As a side effect this advances image tracking to the next image.
// Returns the length of the new image filename, or 0 if the target is not
// configured for multiple images.
int scsiDiskGetNextImageName(image_config_t &img, char *buf, size_t buflen);

// Get pointer to extended image configuration based on target idx
image_config_t &scsiDiskGetImageConfig(int target_idx);

// Start data transfer from disk image to SCSI bus
// Can be called by device type specific command implementations (such as READ CD)
void scsiDiskStartRead(uint32_t lba, uint32_t blocks);

// Start data transfer from SCSI bus to disk image
void scsiDiskStartWrite(uint32_t lba, uint32_t blocks);
