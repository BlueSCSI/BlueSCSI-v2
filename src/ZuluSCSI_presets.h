// Preset configurations for various system types.
// Set by "System" config option in config ini.

#pragma once

// The settings set here will be used as defaults but
// can be overridden by the ini file settings.
struct preset_config_t {
    // Informative name of the preset configuration, or NULL for defaults
    const char *presetName;

    // Default settings that apply per SCSI ID
    int quirks;
    int deviceTypeModifier;
    int sectorsPerTrack;
    int headsPerCylinder;
    int prefetchBytes;
    bool rightAlignStrings;
    bool reinsertOnInquiry;

    // Default settings that apply to all SCSI IDs
    int selectionDelay;
    int maxSyncSpeed;
    bool enableUnitAttention;
    bool enableSCSI2;
    bool enableSelLatch;
    bool mapLunsToIDs;
    bool enableParity;
};

// Fetch a preset configuration, or return the default config if unknown system type.
preset_config_t getSystemPreset(const char *presetName);
