/**
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
 * 
 * This file is licensed under the GPL version 3 or any later version.  
 * It is derived from scsiPhy.c in SCSI2SD V6.
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
#pragma once
#include <stdint.h>

// Index 8 is the system defaults
// Index 0-7 represent device settings
#define SCSI_SETTINGS_SYS_IDX 8

// This struct should only have new settings added to the end
// as it maybe saved and restored directly from flash memory
struct __attribute__((__packed__)) scsi_system_settings_t 
{
    // Settings for host compatibility 
    uint8_t quirks;
    uint8_t selectionDelay;
    uint8_t maxSyncSpeed;
    uint8_t phyMode;
    uint16_t initPreDelay;
    uint16_t initPostDelay;

    bool enableUnitAttention;
    bool enableSCSI2;
    bool enableSelLatch;
    bool mapLunsToIDs;
    bool enableParity;
    bool useFATAllocSize;
};

// This struct should only have new setting added to the end
// as it maybe saved and restored directly from flash memory
struct __attribute__((__packed__)) scsi_device_settings_t
{
    // Settings that can be set on all or specific device
    int prefetchBytes;
    uint16_t sectorsPerTrack;
    uint16_t headsPerCylinder;

    char prodId[16];
    char serial[16];
    char vendor[8];
    char revision[4];

    uint16_t vol;
    uint8_t deviceType;
    uint8_t deviceTypeModifier;
    uint8_t ejectButton;
    bool nameFromImage;
    bool rightAlignStrings;
    bool reinsertOnInquiry;
    bool reinsertAfterEject;
    bool disableMacSanityCheck;
};


struct scsi_settings_t {
    // Informative name of the preset configuration, or NULL for defaults
    // The last presetName is for the System preset name. The rest are for
    // corresponding SCSI Ids.
    char presetName[9][32];

    // These are setting for host compatibility
    scsi_system_settings_t sys;

    // The last dev will be copied over the other dev scsi Id for device defaults.
    // It is set during when the system settings are initialized
    scsi_device_settings_t dev[9];
};


// Initialize settings for all devices with a preset configuration,
//  or return the default config if unknown system type.
// Then overwrite any settings with those in the CONFIGFILE
scsi_system_settings_t *initSystemSetting(const char *presetName);

// Copy any shared device setting done the initSystemSettings as default settings, 
// or return the default config if unknown device type.
// Then overwrite any settings with those in the CONFIGFILE
scsi_device_settings_t *initDeviceSettings(uint8_t scsiId, const char *presetName);

// return the system settings struct to read values
scsi_system_settings_t *getSystemSetting();

// return the device settings struct to read values
scsi_device_settings_t *getDeviceSettings(uint8_t scsiId);

// return the system preset name
const char* getSystemPresetName();

// return the device preset name
const char* getDevicePresetName(uint8_t scsiId);
