/**
 * Copyright (C) 2025-2026 Kevin Moonlight <me@yyzkevin.com>
 *
 * This file is part of BlueSCSI
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// Custom SCSI inquiry data (VPD/SPD) from INI configuration

#include "BlueSCSI_vendor_inquiry.h"

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)

#include "BlueSCSI_log.h"
#include "BlueSCSI_config.h"
#include <minIni.h>
#include <string.h>
#include <stdlib.h>

// Storage for custom VPD pages: up to 16 entries across all SCSI IDs
// Each entry: [0]=scsiId, [1]=pageCode, [2]=length, [3..]=data
#define MAX_CUSTOM_VPD_ENTRIES 16
#define MAX_VPD_DATA_SIZE 128
static struct {
    uint8_t scsiId;
    uint8_t pageCode;
    uint8_t length;
    uint8_t data[MAX_VPD_DATA_SIZE];
} g_custom_vpd[MAX_CUSTOM_VPD_ENTRIES];
static int g_custom_vpd_count = 0;

// Storage for custom standard inquiry data per SCSI ID
#define MAX_SPD_SIZE 128
static struct {
    uint8_t length;
    uint8_t data[MAX_SPD_SIZE];
} g_custom_spd[8];

// Parse space/comma-separated hex values from a string into a byte buffer.
// Returns number of bytes parsed.
static int parseHexString(const char *str, uint8_t *buf, int maxlen)
{
    const char *ptr = str;
    char *end;
    int count = 0;

    while (*ptr != '\0' && count < maxlen)
    {
        buf[count++] = (uint8_t)strtol(ptr, &end, 16);
        if (ptr == end) break; // No conversion possible
        ptr = end;
        while (*ptr == ' ' || *ptr == ',') ptr++;
    }
    return count;
}

void parseCustomInquiryData(void)
{
    char tmp[512];
    char section[8];
    char key[8];

    g_custom_vpd_count = 0;
    memset(g_custom_spd, 0, sizeof(g_custom_spd));

    for (int id = 0; id < 8; id++)
    {
        snprintf(section, sizeof(section), "SCSI%d", id);

        // Parse VPD pages: vpd00, vpd80, etc.
        for (int page = 0; page < 0xFF && g_custom_vpd_count < MAX_CUSTOM_VPD_ENTRIES; page++)
        {
            snprintf(key, sizeof(key), "vpd%02x", page);
            if (ini_gets(section, key, "", tmp, sizeof(tmp), CONFIGFILE))
            {
                int idx = g_custom_vpd_count;
                g_custom_vpd[idx].scsiId = id;
                g_custom_vpd[idx].pageCode = page;
                g_custom_vpd[idx].length = parseHexString(tmp, g_custom_vpd[idx].data, MAX_VPD_DATA_SIZE);
                if (g_custom_vpd[idx].length > 0)
                {
                    logmsg("Custom VPD page 0x", key + 3, " for SCSI ID ", id,
                           ": ", (int)g_custom_vpd[idx].length, " bytes");
                    g_custom_vpd_count++;
                }
            }
        }

        // Parse standard inquiry override: spd=
        if (ini_gets(section, "spd", "", tmp, sizeof(tmp), CONFIGFILE))
        {
            g_custom_spd[id].length = parseHexString(tmp, g_custom_spd[id].data, MAX_SPD_SIZE);
            if (g_custom_spd[id].length > 0)
            {
                logmsg("Custom SPD for SCSI ID ", id, ": ", (int)g_custom_spd[id].length, " bytes");
            }
        }
    }
}

bool getCustomVPD(uint8_t scsiId, uint8_t pageCode, uint8_t *buf, uint8_t *length)
{
    for (int i = 0; i < g_custom_vpd_count; i++)
    {
        if (g_custom_vpd[i].scsiId == (scsiId & 7) && g_custom_vpd[i].pageCode == pageCode)
        {
            *length = g_custom_vpd[i].length;
            memcpy(buf, g_custom_vpd[i].data, g_custom_vpd[i].length);
            return true;
        }
    }
    return false;
}

bool getCustomSPD(uint8_t scsiId, uint8_t *buf, uint16_t *length)
{
    uint8_t id = scsiId & 7;
    if (g_custom_spd[id].length > 0)
    {
        *length = g_custom_spd[id].length;
        memcpy(buf, g_custom_spd[id].data, g_custom_spd[id].length);
        return true;
    }
    return false;
}

#endif // BLUESCSI_ULTRA || BLUESCSI_ULTRA_WIDE
