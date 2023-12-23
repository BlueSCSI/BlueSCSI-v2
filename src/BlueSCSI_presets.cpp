#include "BlueSCSI_presets.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include <strings.h>

// Helper function for case-insensitive string compare
static bool strequals(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
}

preset_config_t getSystemPreset(const char *presetName)
{
    // Default configuration
    preset_config_t cfg = {};
    cfg.quirks = S2S_CFG_QUIRKS_APPLE;
    cfg.deviceTypeModifier = 0;
    cfg.sectorsPerTrack = 63;
    cfg.headsPerCylinder = 255;
    cfg.prefetchBytes = PREFETCH_BUFFER_SIZE;

    cfg.selectionDelay = 255;
    cfg.maxSyncSpeed = 10;
    cfg.enableUnitAttention = false;
    cfg.enableSCSI2 = true;
    cfg.enableSelLatch = false;
    cfg.mapLunsToIDs = false;
    cfg.enableParity = true;
    cfg.reinsertOnInquiry = false;

    // System-specific defaults
    if (strequals(presetName, ""))
    {
        // Preset name is empty, use default configuration
    }
    else if (strequals(presetName, "Generic"))
    {
        cfg.presetName = "Generic";
        cfg.quirks = S2S_CFG_QUIRKS_NONE;
    }
    else if (strequals(presetName, "MacPlus"))
    {
        cfg.presetName = "MacPlus";
        cfg.quirks = S2S_CFG_QUIRKS_APPLE;
        cfg.enableSelLatch = true;
        cfg.enableSCSI2 = false;
        cfg.selectionDelay = 0;
    }
    else if (strequals(presetName, "MegaSTE"))
    {
        cfg.presetName = "MegaSTE";
        cfg.quirks = S2S_CFG_QUIRKS_NONE;
        cfg.mapLunsToIDs = true;
        cfg.enableParity = false;
    }
    else if (strequals(presetName, "X68000"))
    {
        cfg.presetName = "X68000";
        cfg.selectionDelay = 0;
        cfg.quirks = S2S_CFG_QUIRKS_X68000;
        cfg.enableSCSI2 = 0;
        cfg.maxSyncSpeed = 5;
    }
    else
    {
        log("Unknown preset name ", presetName, ", using default settings");
    }

    return cfg;
}
