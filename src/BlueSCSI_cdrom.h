// Advanced CD-ROM drive emulation.
// Adds a few capabilities on top of the SCSI2SD CD-ROM emulation:
//
// - bin/cue support for support of multiple tracks
// - on the fly image switching

#pragma once

#include "BlueSCSI_disk.h"

// Called by scsi.c from SCSI2SD
extern "C" int scsiCDRomCommand(void);

// Close CDROM tray and note media change event
void cdromCloseTray(image_config_t &img);

// Eject CDROM tray if closed, close if open
// Switch image on ejection.
void cdromPerformEject(image_config_t &img);

// Reinsert ejected CD-ROM and restart from first image
void cdromReinsertFirstImage(image_config_t &img);

// Check if the currently loaded cue sheet for the image can be parsed
// and print warnings about unsupported track types
bool cdromValidateCueSheet(image_config_t &img);

// Audio playback status
// boolean flag is true if just basic mechanism status (playback true/false)
// is desired, or false if historical audio status codes should be returned
void cdromGetAudioPlaybackStatus(uint8_t *status, uint32_t *current_lba, bool current_only);