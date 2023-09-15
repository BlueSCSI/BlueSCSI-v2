#include "ZuluSCSI_presets.h"
#include "ZuluSCSI_disk.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_config.h"
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
    cfg.quirks = S2S_CFG_QUIRKS_NONE;
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
    cfg.initPreDelay = 0;

    // System-specific defaults
    if (strequals(presetName, ""))
    {
        // Preset name is empty, use default configuration
    }
    else if (strequals(presetName, "Mac"))
    {
        cfg.presetName = "Mac";
        cfg.quirks = S2S_CFG_QUIRKS_APPLE;
    }
    else if (strequals(presetName, "MacPlus"))
    {
        cfg.presetName = "MacPlus";
        cfg.quirks = S2S_CFG_QUIRKS_APPLE;
        cfg.enableSelLatch = true;
        cfg.enableSCSI2 = false;
        cfg.selectionDelay = 0;
    }
    else if (strequals(presetName, "MPC3000"))
    {
        cfg.initPreDelay = 600;
    }
    else
    {
        logmsg("Unknown preset name ", presetName, ", using default settings");
    }

    return cfg;
}
