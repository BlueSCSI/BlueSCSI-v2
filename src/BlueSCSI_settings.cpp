/**
 * This file is originally part of ZuluSCSI adopted for BlueSCSI
 *
 * ZuluSCSI™ - Copyright (c) 2023-2025 Rabbit Hole Computing™
 * Copyright (c) 2023 Eric Helgeson
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


#include "BlueSCSI_disk.h"
#include "BlueSCSI_audio.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_settings.h"
#include "BlueSCSI_platform.h"
#include <strings.h>
#include <minIni.h>
#include <minIni_cache.h>

// SCSI system and device settings
BlueSCSISettings g_scsi_settings;

const char *systemPresetName[] = {"", "Mac", "MacPlus", "MPC3000", "MegaSTE", "X68000"};
const char *devicePresetName[] = {"", "ST32430N"};

// must be in the same order as bluescsi_speed_grade_t in BlueSCSI_settings.h
const char * const speed_grade_strings[] =
{
    "Default",
    "TurboMax",
    "Custom",
    "AudioSPDIF",
    "AudioI2S",
    "A",
    "B",
    "C",
    "WifiRM2"
};

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

const char **BlueSCSISettings::deviceInitST32430N(uint8_t scsiId)
{
    static const char *st32430n[4] = {"SEAGATE", devicePresetName[DEV_PRESET_ST32430N], PLATFORM_REVISION, ""};
    m_dev[scsiId].deviceType = S2S_CFG_FIXED;
    m_dev[scsiId].sectorSDBegin = 0;
    m_dev[scsiId].sectorSDEnd = 4397055; // 2147MB into bytes and divide 512 - 1
    m_devPreset[scsiId] = DEV_PRESET_ST32430N;
    return st32430n;
}


void BlueSCSISettings::setDefaultDriveInfo(uint8_t scsiId, const char *presetName, S2S_CFG_TYPE type)
{
    char section[6] = "SCSI0";
    section[4] += scsiId;

    scsi_device_settings_t &cfgDev = m_dev[scsiId];
    scsi_device_settings_t &cfgDefault = m_dev[SCSI_SETTINGS_SYS_IDX];
    


    static const char * const driveinfo_fixed[4]     = DRIVEINFO_FIXED;
    static const char * const driveinfo_removable[4] = DRIVEINFO_REMOVABLE;
    static const char * const driveinfo_optical[4]   = DRIVEINFO_OPTICAL;
    static const char * const driveinfo_floppy[4]    = DRIVEINFO_FLOPPY;
    static const char * const driveinfo_magopt[4]    = DRIVEINFO_MAGOPT;
    static const char * const driveinfo_network[4]   = DRIVEINFO_NETWORK;
    static const char * const driveinfo_tape[4]      = DRIVEINFO_TAPE;

    static const char * const apl_driveinfo_fixed[4]     = APPLE_DRIVEINFO_FIXED;
    static const char * const apl_driveinfo_removable[4] = APPLE_DRIVEINFO_REMOVABLE;
    static const char * const apl_driveinfo_optical[4]   = APPLE_DRIVEINFO_OPTICAL;
    static const char * const apl_driveinfo_floppy[4]    = APPLE_DRIVEINFO_FLOPPY;
    static const char * const apl_driveinfo_magopt[4]    = APPLE_DRIVEINFO_MAGOPT;
    static const char * const apl_driveinfo_network[4]   = APPLE_DRIVEINFO_NETWORK;
    static const char * const apl_driveinfo_tape[4]      = APPLE_DRIVEINFO_TAPE;

    static const char * const iomega_driveinfo_removeable[4] = IOMEGA_DRIVEINFO_ZIP100;
    
    const char * const * driveinfo = NULL;
    bool known_preset = false;
    scsi_system_settings_t& cfgSys = m_sys;


#ifdef BLUESCSI_HARDWARE_CONFIG
    if (g_hw_config.is_active() && g_hw_config.device_preset() ==  DEV_PRESET_NONE)
    {
        // empty preset, use default
        known_preset = true;
        m_devPreset[scsiId] = DEV_PRESET_NONE;
    }
    else if (g_hw_config.is_active() && g_hw_config.device_preset() ==  DEV_PRESET_ST32430N)
    {
        driveinfo = deviceInitST32430N(scsiId);
        m_devPreset[scsiId] = DEV_PRESET_ST32430N;
        known_preset = true;
    }
    else
#endif //BLUESCSI_HARDWARE_CONFIG
    if (strequals(devicePresetName[DEV_PRESET_NONE], presetName))
    {
        // empty preset, use default
        known_preset = true;
        m_devPreset[scsiId] = DEV_PRESET_NONE;
    }
    else if (strequals(devicePresetName[DEV_PRESET_ST32430N], presetName))
    {
        driveinfo = deviceInitST32430N(scsiId);
        known_preset = true;
    }

    if (!known_preset)
    {
        m_devPreset[scsiId] = DEV_PRESET_NONE;
        logmsg("Unknown Device preset name ", presetName, ", using default settings");
    }

    if (m_devPreset[scsiId] == DEV_PRESET_NONE)
    {
        cfgDev.deviceType = type;
        cfgDev.deviceType = ini_getl(section, "Type", cfgDev.deviceType, CONFIGFILE);
        
        if (cfgSys.quirks == S2S_CFG_QUIRKS_APPLE)
        {
            // Use default drive IDs that are recognized by Apple machines
            switch (cfgDev.deviceType)
            {
                case S2S_CFG_FIXED:         driveinfo = apl_driveinfo_fixed; break;
                case S2S_CFG_REMOVABLE:     driveinfo = apl_driveinfo_removable; break;
                case S2S_CFG_OPTICAL:       driveinfo = apl_driveinfo_optical; break;
                case S2S_CFG_FLOPPY_14MB:   driveinfo = apl_driveinfo_floppy; break;
                case S2S_CFG_MO:            driveinfo = apl_driveinfo_magopt; break;
                case S2S_CFG_NETWORK:       driveinfo = apl_driveinfo_network; break;
                case S2S_CFG_SEQUENTIAL:    driveinfo = apl_driveinfo_tape; break;
                case S2S_CFG_ZIP100:        driveinfo = iomega_driveinfo_removeable; break;
                default:                    driveinfo = apl_driveinfo_fixed; break;
            }
        }
        else
        {
            // Generic IDs
            switch (cfgDev.deviceType)
            {
                case S2S_CFG_FIXED:         driveinfo = driveinfo_fixed; break;
                case S2S_CFG_REMOVABLE:     driveinfo = driveinfo_removable; break;
                case S2S_CFG_OPTICAL:       driveinfo = driveinfo_optical; break;
                case S2S_CFG_FLOPPY_14MB:   driveinfo = driveinfo_floppy; break;
                case S2S_CFG_MO:            driveinfo = driveinfo_magopt; break;
                case S2S_CFG_NETWORK:       driveinfo = driveinfo_network; break;
                case S2S_CFG_SEQUENTIAL:    driveinfo = driveinfo_tape; break;
                case S2S_CFG_ZIP100:        driveinfo = iomega_driveinfo_removeable; break;
                default:                    driveinfo = driveinfo_fixed; break;
            }
        }
    }

    // If the scsi string has not been set system wide use default scsi string
    if (!cfgDefault.vendor[0] && driveinfo[0][0])
        strncpy(cfgDev.vendor, driveinfo[0], sizeof(cfgDev.vendor));
    if (!cfgDefault.prodId[0] && driveinfo[1][0])
        strncpy(cfgDev.prodId, driveinfo[1], sizeof(cfgDev.prodId));
    if (!cfgDefault.revision[0] && driveinfo[2][0])
        strncpy(cfgDev.revision, driveinfo[2], sizeof(cfgDev.revision));
    if (!cfgDefault.serial[0] && driveinfo[3][0])
        strncpy(cfgDev.serial, driveinfo[3], sizeof(cfgDev.serial));
}

// Read device settings
static void readIniSCSIDeviceSetting(scsi_device_settings_t &cfg, const char *section)
{
    cfg.deviceTypeModifier = ini_getl(section, "TypeModifier", cfg.deviceTypeModifier, CONFIGFILE);
    cfg.sectorsPerTrack = ini_getl(section, "SectorsPerTrack", cfg.sectorsPerTrack, CONFIGFILE);
    cfg.headsPerCylinder = ini_getl(section, "HeadsPerCylinder", cfg.headsPerCylinder, CONFIGFILE);
    cfg.prefetchBytes = ini_getl(section, "PrefetchBytes", cfg.prefetchBytes, CONFIGFILE);
    cfg.ejectButton = ini_getl(section, "EjectButton", cfg.ejectButton, CONFIGFILE);
    cfg.ejectBlinkTimes = ini_getl(section, "EjectBlinkTimes", cfg.ejectBlinkTimes, CONFIGFILE);
    cfg.ejectBlinkPeriod = ini_getl(section, "EjectBlinkPeriod", cfg.ejectBlinkPeriod, CONFIGFILE);

    cfg.vol = ini_getl(section, "CDAVolume", cfg.vol, CONFIGFILE) & 0xFF;

    cfg.nameFromImage = ini_getbool(section, "NameFromImage", cfg.nameFromImage, CONFIGFILE);
    cfg.rightAlignStrings = ini_getbool(section, "RightAlignStrings", cfg.rightAlignStrings , CONFIGFILE);
    cfg.reinsertOnInquiry = ini_getbool(section, "ReinsertCDOnInquiry", cfg.reinsertOnInquiry, CONFIGFILE);
    cfg.reinsertAfterEject = ini_getbool(section, "ReinsertAfterEject", cfg.reinsertAfterEject, CONFIGFILE);
    cfg.disableMacSanityCheck = ini_getbool(section, "DisableMacSanityCheck", cfg.disableMacSanityCheck, CONFIGFILE);

    cfg.sectorSDBegin = ini_getl(section, "SectorSDBegin", cfg.sectorSDBegin, CONFIGFILE);
    cfg.sectorSDEnd = ini_getl(section, "SectorSDEnd", cfg.sectorSDEnd, CONFIGFILE);

    cfg.vendorExtensions = ini_getl(section, "VendorExtensions", cfg.vendorExtensions, CONFIGFILE);

    cfg.blockSize = ini_getl(section, "BlockSize", cfg.blockSize, CONFIGFILE);

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

scsi_system_settings_t *BlueSCSISettings::initSystem(const char *presetName)
{
    scsi_system_settings_t &cfgSys = m_sys;
    scsi_device_settings_t &cfgDev = m_dev[SCSI_SETTINGS_SYS_IDX];
    // This is a hack to figure out if apple quirks is on via a dip switch
    S2S_TargetCfg img;

    img.quirks = S2S_CFG_QUIRKS_NONE;
    #ifdef PLATFORM_CONFIG_HOOK
            PLATFORM_CONFIG_HOOK(&img);
    #endif

    // Default settings for host compatibility 
    cfgSys.quirks = img.quirks;
    cfgSys.selectionDelay = 255;
    cfgSys.maxSyncSpeed = PLATFORM_DEFAULT_SCSI_SPEED_SETTING;
    cfgSys.initPreDelay = 0;
    cfgSys.initPostDelay = 0;
    cfgSys.phyMode = 0;

    cfgSys.enableUnitAttention = false;
    cfgSys.enableSCSI2 = true;
    cfgSys.enableSelLatch = false;
    cfgSys.mapLunsToIDs = false;
    cfgSys.enableParity = true;
    cfgSys.useFATAllocSize = false;
    cfgSys.enableCDAudio = false;
    cfgSys.maxVolume = 100;
    cfgSys.enableUSBMassStorage = false;
    cfgSys.usbMassStorageWaitPeriod = 1000;
    cfgSys.usbMassStoragePresentImages = false;
    cfgSys.invertStatusLed = false;

    cfgSys.speedGrade = bluescsi_speed_grade_t::SPEED_GRADE_DEFAULT;

    // setting set for all or specific devices
    cfgDev.deviceType = S2S_CFG_NOT_SET;
    cfgDev.deviceTypeModifier = 0;
    cfgDev.sectorsPerTrack = 0;
    cfgDev.headsPerCylinder = 0;
    cfgDev.prefetchBytes = PREFETCH_BUFFER_SIZE;
    cfgDev.ejectButton = 0;
    cfgDev.ejectBlinkTimes = 20;
    cfgDev.ejectBlinkPeriod = 50;
    cfgDev.vol = DEFAULT_VOLUME_LEVEL;
    
    cfgDev.nameFromImage = false;
    cfgDev.rightAlignStrings = false;
    cfgDev.reinsertOnInquiry = true;
    cfgDev.reinsertAfterEject = true;
    cfgDev.disableMacSanityCheck = false;

    cfgDev.sectorSDBegin = 0;
    cfgDev.sectorSDEnd = 0;

    cfgDev.vendorExtensions = 0;

    cfgDev.blockSize = 0;

    // System-specific defaults

    if (strequals(systemPresetName[SYS_PRESET_NONE], presetName))
    {
        // Preset name is empty, use default configuration
        m_sysPreset = SYS_PRESET_NONE;
    }
    else if (strequals(systemPresetName[SYS_PRESET_MAC], presetName))
    {
        m_sysPreset = SYS_PRESET_MAC;
        cfgSys.quirks = S2S_CFG_QUIRKS_APPLE;
    }
    else if (strequals(systemPresetName[SYS_PRESET_MACPLUS], presetName))
    {
        m_sysPreset = SYS_PRESET_MACPLUS;
        cfgSys.quirks = S2S_CFG_QUIRKS_APPLE;
        cfgSys.enableSelLatch = true;
        cfgSys.enableSCSI2 = false;
        cfgSys.selectionDelay = 0;
    }
    else if (strequals(systemPresetName[SYS_PRESET_MPC3000], presetName))
    {
        m_sysPreset = SYS_PRESET_MPC3000;
        cfgSys.initPreDelay = 700;
    }
    else if (strequals(systemPresetName[SYS_PRESET_MEGASTE], presetName))
    {
        m_sysPreset = SYS_PRESET_MEGASTE;
        cfgSys.quirks = S2S_CFG_QUIRKS_NONE;
        cfgSys.mapLunsToIDs = true;
        cfgSys.enableParity = false;
    }
    else if (strequals(systemPresetName[SYS_PRESET_X68000], presetName))
    {
        m_sysPreset = SYS_PRESET_X68000;
        cfgSys.selectionDelay = 0;
        cfgSys.quirks = S2S_CFG_QUIRKS_X68000;
        cfgSys.enableSCSI2 = false;
        cfgSys.maxSyncSpeed = 5;
    }
    else
    {
        m_sysPreset = SYS_PRESET_NONE;
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
    cfgSys.enableCDAudio = ini_getbool("SCSI", "EnableCDAudio", cfgSys.enableCDAudio, CONFIGFILE);
    cfgSys.maxVolume =  ini_getl("SCSI", "MaxVolume", cfgSys.maxVolume, CONFIGFILE);

    cfgSys.enableUSBMassStorage = ini_getbool("SCSI", "EnableUSBMassStorage", cfgSys.enableUSBMassStorage, CONFIGFILE);
    cfgSys.usbMassStorageWaitPeriod = ini_getl("SCSI", "USBMassStorageWaitPeriod", cfgSys.usbMassStorageWaitPeriod, CONFIGFILE);
    cfgSys.usbMassStoragePresentImages = ini_getbool("SCSI", "USBMassStoragePresentImages", cfgSys.usbMassStoragePresentImages, CONFIGFILE);

    cfgSys.invertStatusLed = ini_getbool("SCSI", "InvertStatusLED", cfgSys.invertStatusLed, CONFIGFILE);
    
    char tmp[32];
    ini_gets("SCSI", "SpeedGrade", "", tmp, sizeof(tmp), CONFIGFILE);
    if (tmp[0] != '\0')
    {
        if (platform_reclock_supported())
        {
            cfgSys.speedGrade = stringToSpeedGrade(tmp, sizeof(tmp));
        }
        else
        {
            logmsg("Speed grade setting ignored, reclocking the MCU is not supported by this device");
        }
    }

    return &cfgSys;
}

scsi_device_settings_t* BlueSCSISettings::initDevice(uint8_t scsiId, S2S_CFG_TYPE type)
{
    scsi_device_settings_t& cfg = m_dev[scsiId];
    char presetName[32] = {};
    char section[6] = "SCSI0";
    section[4] = '0' + scsiId;

#ifdef BLUESCSI_HARDWARE_CONFIG
    const char *hwDevicePresetName = g_scsi_settings.getDevicePresetName(scsiId);
    if (g_hw_config.is_active())
    {
        if (strlen(hwDevicePresetName) < sizeof(presetName))
        {
            strncpy(presetName, hwDevicePresetName, sizeof(presetName) - 1);
        }
    }
    else
#endif
    {
        ini_gets(section, "Device", "", presetName, sizeof(presetName), CONFIGFILE);
    }


    // Write default configuration from system setting initialization
    memcpy(&cfg, &m_dev[SCSI_SETTINGS_SYS_IDX], sizeof(cfg));
    setDefaultDriveInfo(scsiId, presetName, type);
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

scsi_system_settings_t *BlueSCSISettings::getSystem()
{
    return &m_sys;
}

scsi_device_settings_t *BlueSCSISettings::getDevice(uint8_t scsiId)
{
    return &m_dev[scsiId];
}

scsi_system_preset_t BlueSCSISettings::getSystemPreset()
{
    return m_sysPreset;
}

const char* BlueSCSISettings::getSystemPresetName()
{
    return systemPresetName[m_sysPreset];
}

scsi_device_preset_t BlueSCSISettings::getDevicePreset(uint8_t scsiId)
{
    return m_devPreset[scsiId];
}

const char* BlueSCSISettings::getDevicePresetName(uint8_t scsiId)
{
    return devicePresetName[m_devPreset[scsiId]];
}


bluescsi_speed_grade_t BlueSCSISettings::stringToSpeedGrade(const char *speed_grade_target, size_t length)
{
    bluescsi_speed_grade_t grade = bluescsi_speed_grade_t::SPEED_GRADE_DEFAULT;

    bool found_speed_grade = false;
    // search the list of speed grade strings for a matching target
    for (uint8_t i = 0; i < sizeof(speed_grade_strings)/sizeof(speed_grade_strings[0]); i++)
    {
        if (strncasecmp(speed_grade_target, speed_grade_strings[i], length) == 0)
        {
            grade = (bluescsi_speed_grade_t)i;
            found_speed_grade = true;
            break;
        }
    }
    if (!found_speed_grade)
    {
      logmsg("Setting \"", speed_grade_target, "\" does not match any known speed grade, using default");
      grade = SPEED_GRADE_DEFAULT;
    }

    return grade;
}

const char *BlueSCSISettings::getSpeedGradeString()
{
    return speed_grade_strings[m_sys.speedGrade];
}


const bool BlueSCSISettings::isEjectButtonSet()
{
    bool is_set = false;
    for (uint8_t i = 0; i < S2S_MAX_TARGETS; i++)
    {
        if (m_dev[i].ejectButton != 0)
        {
            is_set = true;
            break;
        }
    }
    return is_set;
}