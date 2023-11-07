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

#include "ZuluSCSI_settings.h"
#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_audio.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
#include <strings.h>
#include <minIni.h>
#include <minIni_cache.h>

// SCSI system and device settings
static scsi_settings_t scsiSetting;

// Helper function for case-insensitive string compare
static bool strequals(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
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
static void setDefaultDriveInfo(uint8_t scsiId, const char *presetName)
{
    char section[6] = "SCSI0";
    section[4] += scsiId;

    scsi_device_settings_t &cfg = scsiSetting.dev[scsiId];
    scsi_device_settings_t &cfgDefault = scsiSetting.dev[SCSI_SETTINGS_SYS_IDX];

    static const char *driveinfo_fixed[4]     = DRIVEINFO_FIXED;
    static const char *driveinfo_removable[4] = DRIVEINFO_REMOVABLE;
    static const char *driveinfo_optical[4]   = DRIVEINFO_OPTICAL;
    static const char *driveinfo_floppy[4]    = DRIVEINFO_FLOPPY;
    static const char *driveinfo_magopt[4]    = DRIVEINFO_MAGOPT;
    static const char *driveinfo_network[4]   = DRIVEINFO_NETWORK;
    static const char *driveinfo_tape[4]      = DRIVEINFO_TAPE;

    static const char *apl_driveinfo_fixed[4]     = APPLE_DRIVEINFO_FIXED;
    static const char *apl_driveinfo_removable[4] = APPLE_DRIVEINFO_REMOVABLE;
    static const char *apl_driveinfo_optical[4]   = APPLE_DRIVEINFO_OPTICAL;
    static const char *apl_driveinfo_floppy[4]    = APPLE_DRIVEINFO_FLOPPY;
    static const char *apl_driveinfo_magopt[4]    = APPLE_DRIVEINFO_MAGOPT;
    static const char *apl_driveinfo_network[4]   = APPLE_DRIVEINFO_NETWORK;
    static const char *apl_driveinfo_tape[4]      = APPLE_DRIVEINFO_TAPE;

    const char **driveinfo = NULL;
    bool known_preset = false;
    scsi_system_settings_t& cfgSys = scsiSetting.sys;

    strncpy(scsiSetting.presetName[scsiId], presetName, sizeof(scsiSetting.presetName[scsiId]));
    // Make sure string is null terminated
    scsiSetting.presetName[scsiId][sizeof(scsiSetting.presetName[scsiId])-1] = '\0';

    if (strequals("", presetName))
    {
        // empty preset, use default
        known_preset = true;
    }

    if (strequals("ST32430N", presetName))
    {
        static const char *st32430n[4] = {"SEAGATE", "STN32F30N", PLATFORM_REVISION, ""};
        driveinfo = st32430n;
        known_preset = true;
    }
    else if (cfgSys.quirks == S2S_CFG_QUIRKS_APPLE)
    {
        // Use default drive IDs that are recognized by Apple machines
        switch (cfg.deviceType)
        {
            case S2S_CFG_FIXED:         driveinfo = apl_driveinfo_fixed; break;
            case S2S_CFG_REMOVABLE:     driveinfo = apl_driveinfo_removable; break;
            case S2S_CFG_OPTICAL:       driveinfo = apl_driveinfo_optical; break;
            case S2S_CFG_FLOPPY_14MB:   driveinfo = apl_driveinfo_floppy; break;
            case S2S_CFG_MO:            driveinfo = apl_driveinfo_magopt; break;
            case S2S_CFG_NETWORK:       driveinfo = apl_driveinfo_network; break;
            case S2S_CFG_SEQUENTIAL:    driveinfo = apl_driveinfo_tape; break;
            default:                    driveinfo = apl_driveinfo_fixed; break;
        }
    }
    else
    {
        // Generic IDs
        switch (cfg.deviceType)
        {
            case S2S_CFG_FIXED:         driveinfo = driveinfo_fixed; break;
            case S2S_CFG_REMOVABLE:     driveinfo = driveinfo_removable; break;
            case S2S_CFG_OPTICAL:       driveinfo = driveinfo_optical; break;
            case S2S_CFG_FLOPPY_14MB:   driveinfo = driveinfo_floppy; break;
            case S2S_CFG_MO:            driveinfo = driveinfo_magopt; break;
            case S2S_CFG_NETWORK:       driveinfo = driveinfo_network; break;
            case S2S_CFG_SEQUENTIAL:    driveinfo = driveinfo_tape; break;
            default:                    driveinfo = driveinfo_fixed; break;
        }
    }

    if (!known_preset)
    {
        scsiSetting.presetName[scsiId][0] = '\0';
        logmsg("Unknown Device preset name ", presetName, ", using default settings");
    }

    // If the scsi string has not been set system wide use default scsi string
    if (!cfgDefault.vendor[0] && driveinfo[0][0])
        strncpy(cfg.vendor, driveinfo[0], sizeof(cfg.vendor));
    if (!cfgDefault.prodId[0] &&driveinfo[1][0])
        strncpy(cfg.prodId, driveinfo[1], sizeof(cfg.prodId));
    if (!cfgDefault.revision[0] &&driveinfo[2][0])
        strncpy(cfg.revision, driveinfo[2], sizeof(cfg.revision));
    if (!cfgDefault.serial[0] &&driveinfo[3][0])
        strncpy(cfg.serial, driveinfo[3], sizeof(cfg.serial));
}

// Read device settings
static void readIniSCSIDeviceSetting(scsi_device_settings_t &cfg, const char *section)
{
    cfg.deviceType = ini_getl(section, "Type", cfg.deviceType, CONFIGFILE);
    cfg.deviceTypeModifier = ini_getl(section, "TypeModifier", cfg.deviceTypeModifier, CONFIGFILE);
    cfg.sectorsPerTrack = ini_getl(section, "SectorsPerTrack", cfg.sectorsPerTrack, CONFIGFILE);
    cfg.headsPerCylinder = ini_getl(section, "HeadsPerCylinder", cfg.headsPerCylinder, CONFIGFILE);
    cfg.prefetchBytes = ini_getl(section, "PrefetchBytes", cfg.prefetchBytes, CONFIGFILE);
    cfg.ejectButton = ini_getl(section, "EjectButton", cfg.ejectButton, CONFIGFILE);

    cfg.vol = ini_getl(section, "CDAVolume", cfg.vol, CONFIGFILE) & 0xFF;

    cfg.nameFromImage = ini_getbool(section, "NameFromImage", cfg.nameFromImage, CONFIGFILE);
    cfg.rightAlignStrings = ini_getbool(section, "RightAlignStrings", cfg.rightAlignStrings , CONFIGFILE);
    cfg.reinsertOnInquiry = ini_getbool(section, "ReinsertCDOnInquiry", cfg.reinsertOnInquiry, CONFIGFILE);
    cfg.reinsertAfterEject = ini_getbool(section, "ReinsertAfterEject", cfg.reinsertAfterEject, CONFIGFILE);
    cfg.disableMacSanityCheck = ini_getbool(section, "DisableMacSanityCheck", cfg.disableMacSanityCheck, CONFIGFILE);

    char tmp[32];
    ini_gets(section, "Vendor", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        memset(cfg.vendor, 0, sizeof(cfg.vendor));
        strncpy(cfg.vendor, tmp, sizeof(cfg.vendor));
    }
    memset(tmp, 0, sizeof(tmp));

    ini_gets(section, "Product", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        memset(cfg.prodId, 0, sizeof(cfg.prodId));
        strncpy(cfg.prodId, tmp, sizeof(cfg.prodId));

    }
    memset(tmp, 0, sizeof(tmp));

    ini_gets(section, "Version", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        memset(cfg.revision, 0, sizeof(cfg.revision));
        strncpy(cfg.revision, tmp, sizeof(cfg.revision));
    }
    memset(tmp, 0, sizeof(tmp));

    ini_gets(section, "Serial", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0])
    {
        memset(cfg.serial, 0, sizeof(cfg.serial));
        strncpy(cfg.serial, tmp, sizeof(cfg.serial));
    }
}

scsi_system_settings_t *initSystemSetting(const char *presetName)
{
    scsi_system_settings_t &cfgSys = scsiSetting.sys;
    scsi_device_settings_t &cfgDev = scsiSetting.dev[SCSI_SETTINGS_SYS_IDX];
    // This is a hack to figure out if apple quirks is on via a dip switch
    S2S_TargetCfg img;

    img.quirks = S2S_CFG_QUIRKS_NONE;
    #ifdef PLATFORM_CONFIG_HOOK
            PLATFORM_CONFIG_HOOK(&img);
    #endif

    // Default settings for host compatibility 
    cfgSys.quirks = img.quirks;
    cfgSys.selectionDelay = 255;
    cfgSys.maxSyncSpeed = 10;
    cfgSys.initPreDelay = 0;
    cfgSys.initPostDelay = 0;
    cfgSys.phyMode = 0;

    cfgSys.enableUnitAttention = false;
    cfgSys.enableSCSI2 = true;
    cfgSys.enableSelLatch = false;
    cfgSys.mapLunsToIDs = false;
    cfgSys.enableParity = true;
    cfgSys.useFATAllocSize = false;
    
    // setting set for all or specific devices
    cfgDev.deviceType = 0;
    cfgDev.deviceTypeModifier = 0;
    cfgDev.sectorsPerTrack = 63;
    cfgDev.headsPerCylinder = 255;
    cfgDev.prefetchBytes = PREFETCH_BUFFER_SIZE;
    cfgDev.ejectButton = 0;
    cfgDev.vol = DEFAULT_VOLUME_LEVEL;
    
    cfgDev.nameFromImage = false;
    cfgDev.rightAlignStrings = false;
    cfgDev.reinsertOnInquiry = true;
    cfgDev.reinsertAfterEject = true;
    cfgDev.disableMacSanityCheck = false;

    // System-specific defaults
    strncpy(scsiSetting.presetName[SCSI_SETTINGS_SYS_IDX],
            presetName,
            sizeof(scsiSetting.presetName[SCSI_SETTINGS_SYS_IDX]));
    // Make sure string is null terminated
    scsiSetting.presetName[SCSI_SETTINGS_SYS_IDX][sizeof(scsiSetting.presetName[SCSI_SETTINGS_SYS_IDX])-1] = '\0';

    if (strequals(presetName, ""))
    {
        // Preset name is empty, use default configuration
    }
    else if (strequals(presetName, "Mac"))
    {
        cfgSys.quirks = S2S_CFG_QUIRKS_APPLE;
    }
    else if (strequals(presetName, "MacPlus"))
    {
        cfgSys.quirks = S2S_CFG_QUIRKS_APPLE;
        cfgSys.enableSelLatch = true;
        cfgSys.enableSCSI2 = false;
        cfgSys.selectionDelay = 0;
    }
    else if (strequals(presetName, "MPC3000"))
    {
        cfgSys.initPreDelay = 600;
    }
    else
    {
        scsiSetting.presetName[SCSI_SETTINGS_SYS_IDX][0] = '\0';
        logmsg("Unknown System preset name ", presetName, ", using default settings");
    }

    // Clear SCSI device strings
    memset(cfgDev.vendor, 0, sizeof(cfgDev.vendor));
    memset(cfgDev.prodId, 0, sizeof(cfgDev.prodId));
    memset(cfgDev.revision, 0, sizeof(cfgDev.revision));
    memset(cfgDev.serial, 0, sizeof(cfgDev.serial));

    // Read default setting overrides from ini file for each SCSI device
    readIniSCSIDeviceSetting(cfgDev, "SCSI");

    // Read settings from ini file that apply to all SCSI device
    cfgSys.quirks = ini_getl("SCSI", "Quirks", cfgSys.quirks, CONFIGFILE);
    cfgSys.selectionDelay = ini_getl("SCSI", "SelectionDelay", cfgSys.selectionDelay, CONFIGFILE);
    cfgSys.maxSyncSpeed = ini_getl("SCSI", "MaxSyncSpeed", cfgSys.maxSyncSpeed, CONFIGFILE);
    cfgSys.initPreDelay = ini_getl("SCSI", "InitPreDelay", cfgSys.initPreDelay, CONFIGFILE);
    cfgSys.initPostDelay = ini_getl("SCSI", "InitPostDelay", cfgSys.initPostDelay, CONFIGFILE);
    cfgSys.phyMode = ini_getl("SCSI", "PhyMode", cfgSys.phyMode, CONFIGFILE);

    cfgSys.enableUnitAttention = ini_getbool("SCSI", "EnableUnitAttention", cfgSys.enableUnitAttention, CONFIGFILE);
    cfgSys.enableSCSI2 = ini_getbool("SCSI", "EnableSCSI2", cfgSys.enableSCSI2, CONFIGFILE);
    cfgSys.enableSelLatch = ini_getbool("SCSI", "EnableSelLatch", cfgSys.enableSelLatch, CONFIGFILE);
    cfgSys.mapLunsToIDs = ini_getbool("SCSI", "MapLunsToIDs", cfgSys.mapLunsToIDs, CONFIGFILE);
    cfgSys.enableParity =  ini_getbool("SCSI", "EnableParity", cfgSys.enableParity, CONFIGFILE);
    cfgSys.useFATAllocSize = ini_getbool("SCSI", "UseFATAllocSize", cfgSys.useFATAllocSize, CONFIGFILE);

    return &cfgSys;
}

scsi_device_settings_t* initDeviceSettings(uint8_t scsiId, const char *presetName)
{
    scsi_device_settings_t& cfg = scsiSetting.dev[scsiId];

    // Write default configuration from system setting initialization
    memcpy(&cfg, &scsiSetting.dev[SCSI_SETTINGS_SYS_IDX], sizeof(cfg));
    
    char section[6] = "SCSI0";
    section[4] += scsiId;
    setDefaultDriveInfo(scsiId, presetName);
    readIniSCSIDeviceSetting(cfg, section);

    if (cfg.serial[0] == '\0')
    {
        // Use SD card serial number
        cid_t sd_cid;
        uint32_t sd_sn = 0;
        if (SD.card()->readCID(&sd_cid))
        {
            sd_sn = sd_cid.psn();
        }

        memset(cfg.serial, 0, sizeof(cfg.serial));
        const char *nibble = "0123456789ABCDEF";
        cfg.serial[0] = nibble[(sd_sn >> 28) & 0xF];
        cfg.serial[1] = nibble[(sd_sn >> 24) & 0xF];
        cfg.serial[2] = nibble[(sd_sn >> 20) & 0xF];
        cfg.serial[3] = nibble[(sd_sn >> 16) & 0xF];
        cfg.serial[4] = nibble[(sd_sn >> 12) & 0xF];
        cfg.serial[5] = nibble[(sd_sn >>  8) & 0xF];
        cfg.serial[6] = nibble[(sd_sn >>  4) & 0xF];
        cfg.serial[7] = nibble[(sd_sn >>  0) & 0xF];
    }


    formatDriveInfoField(cfg.vendor, sizeof(cfg.vendor), cfg.rightAlignStrings);
    formatDriveInfoField(cfg.prodId, sizeof(cfg.prodId), cfg.rightAlignStrings);
    formatDriveInfoField(cfg.revision, sizeof(cfg.revision), cfg.rightAlignStrings);
    formatDriveInfoField(cfg.serial, sizeof(cfg.serial), true);

    return &cfg;
}

scsi_system_settings_t* getSystemSetting()
{
    return &scsiSetting.sys;
}

scsi_device_settings_t *getDeviceSettings(uint8_t scsiId)
{
    return &scsiSetting.dev[scsiId];
}

const char* getSystemPresetName()
{
    return scsiSetting.presetName[SCSI_SETTINGS_SYS_IDX];
}

const char* getDevicePresetName(uint8_t scsiId)
{
    return scsiSetting.presetName[scsiId];
}
