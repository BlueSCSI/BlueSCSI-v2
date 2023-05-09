// Advanced CD-ROM drive emulation.
// Adds a few capabilities on top of the SCSI2SD CD-ROM emulation:
//
// - bin/cue support for support of multiple tracks
// - on the fly image switching

#pragma once

#include "ZuluSCSI_disk.h"

// Called by scsi.c from SCSI2SD
extern "C" int scsiCDRomCommand(void);

// Reinsert ejected CD-ROM and restart from first image
void cdromReinsertFirstImage(image_config_t &img);

// Switch to next CD-ROM image if multiple have been configured
bool cdromSwitchNextImage(image_config_t &img);
