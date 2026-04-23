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
#include "BlueSCSI_settings.h"
#include "BlueSCSI_disk_as400.h"
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

// Per-SCSI-ID override for the 8-byte AS/400 serial, supplied via the
// AS400_DiskSerialNumber key in [SCSI<n>] sections. When length == 8,
// injectSerial() uses this value instead of the SD CID / MCU-derived default.
// Values shorter than 8 characters are right-padded with ASCII spaces;
// longer values are truncated to 8.
#define AS400_DISK_SERIAL_LEN 8
static struct {
    uint8_t length;
    uint8_t data[AS400_DISK_SERIAL_LEN];
} g_as400_serial_override[8];

// Per-SCSI-ID override for the 7-byte IBM FRU part number stamped in
// VPD page 0x01. Supplied via the AS400_DiskPartNumber key in [SCSI<n>]
// sections. The page holds the FRU twice: once in ASCII at byte 5 and
// once in IBM CP037 EBCDIC at byte 29. Both slots are populated
// together when an override is set.
#define AS400_DISK_PART_LEN 7
static struct {
    uint8_t length;
    uint8_t ascii[AS400_DISK_PART_LEN];
    uint8_t ebcdic[AS400_DISK_PART_LEN];
} g_as400_part_override[8];

// Map the subset of ASCII actually used in IBM DASD FRU strings
// ([0-9 A-Z ]) to IBM CP037 EBCDIC. Unsupported characters fall through
// to EBCDIC space (0x40). CP037 places the digits and letters in
// discontiguous ranges — pulling in a full 256-byte table would add
// rodata for no benefit given the constrained input alphabet.
static uint8_t asciiToEbcdic(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(0xF0 + (c - '0'));
    if (c >= 'A' && c <= 'I') return (uint8_t)(0xC1 + (c - 'A'));
    if (c >= 'J' && c <= 'R') return (uint8_t)(0xD1 + (c - 'J'));
    if (c >= 'S' && c <= 'Z') return (uint8_t)(0xE2 + (c - 'S'));
    return 0x40; // EBCDIC space
}

// Write the FRU override (if set) into the ASCII slot at asciiOffset and,
// when ebcdicOffset >= 0, into the EBCDIC slot at ebcdicOffset of the
// given buffer. Pass a negative ebcdicOffset when the target buffer has
// no EBCDIC slot (e.g. the standard INQUIRY response, which only carries
// an ASCII copy of the FRU). Does nothing when no override was configured
// for this SCSI ID.
static void injectPartNumber(uint8_t *data, int asciiOffset, int ebcdicOffset, uint8_t scsiId)
{
    uint8_t id = scsiId & 7;
    if (g_as400_part_override[id].length != AS400_DISK_PART_LEN) return;
    memcpy(data + asciiOffset, g_as400_part_override[id].ascii, AS400_DISK_PART_LEN);
    if (ebcdicOffset >= 0)
        memcpy(data + ebcdicOffset, g_as400_part_override[id].ebcdic, AS400_DISK_PART_LEN);
}

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

// Check if a custom VPD page already exists for a given SCSI ID and page code
static bool hasCustomVPD(uint8_t scsiId, uint8_t pageCode)
{
    for (int i = 0; i < g_custom_vpd_count; i++)
    {
        if (g_custom_vpd[i].scsiId == scsiId && g_custom_vpd[i].pageCode == pageCode)
            return true;
    }
    return false;
}

// Inject the generated serial number into a VPD page at the given offset.
// If AS400_DiskSerialNumber was set for this SCSI ID, use that; otherwise fall
// back to the SD CID / MCU-derived default.
static void injectSerial(uint8_t *data, int offset, uint8_t scsiId)
{
    uint8_t serial[AS400_DISK_SERIAL_LEN];
    uint8_t id = scsiId & 7;
    if (g_as400_serial_override[id].length == AS400_DISK_SERIAL_LEN)
    {
        memcpy(serial, g_as400_serial_override[id].data, AS400_DISK_SERIAL_LEN);
    }
    else
    {
        as400_get_serial_8(scsiId, serial);
    }
    memcpy(data + offset, serial, AS400_DISK_SERIAL_LEN);
}

// Populate default AS/400 inquiry and VPD data for all SCSI IDs.
// Only fills in data that wasn't already provided via INI.
static void loadAS400Defaults(void)
{
    logmsg("Loading default AS/400 inquiry data");

    for (int id = 0; id < 8; id++)
    {
        // AS/400 inquiry data describes a fixed hard disk. Applying it to a
        // CDROM, tape, ZIP, MO, or other removable device would cause the host
        // to see a fixed drive instead — skip non-fixed device types.
        if (g_scsi_settings.getDevice(id)->deviceType != S2S_CFG_FIXED)
        {
            continue;
        }

        // Default standard inquiry (SPD) with serial and part number
        // injected. The SPD carries only an ASCII copy of the 7-char IBM
        // disk part number at offsets 114-120 — there is no EBCDIC slot
        // here, unlike VPD page 0x01.
        if (g_custom_spd[id].length == 0)
        {
            size_t len = as400_default_inquiry_len;
            if (len > MAX_SPD_SIZE) len = MAX_SPD_SIZE;
            memcpy(g_custom_spd[id].data, as400_default_inquiry, len);
            // Inject serial at offset 38
            if (len >= 46)
                injectSerial(g_custom_spd[id].data, 38, id);
            if (len >= 121)
                injectPartNumber(g_custom_spd[id].data, 114, -1, id);
            g_custom_spd[id].length = len;
        }

        // Default VPD pages
        for (size_t p = 0; p < as400_default_vpd_page_count && g_custom_vpd_count < MAX_CUSTOM_VPD_ENTRIES; p++)
        {
            uint8_t pageLen = as400_default_vpd_pages[p][0]; // first byte is length
            if (pageLen < 2) continue;
            uint8_t pageCode = as400_default_vpd_pages[p][2]; // page code at offset 2 in data

            if (hasCustomVPD(id, pageCode))
                continue; // INI override takes precedence

            int idx = g_custom_vpd_count;
            g_custom_vpd[idx].scsiId = id;
            g_custom_vpd[idx].pageCode = pageCode;
            g_custom_vpd[idx].length = pageLen;
            if (pageLen > MAX_VPD_DATA_SIZE) g_custom_vpd[idx].length = MAX_VPD_DATA_SIZE;
            memcpy(g_custom_vpd[idx].data, &as400_default_vpd_pages[p][1], g_custom_vpd[idx].length);

            // Inject serial into pages that contain it
            if (pageCode == 0x80 && g_custom_vpd[idx].length >= 20)
                injectSerial(g_custom_vpd[idx].data, 12, id); // offset 12 in page data
            else if (pageCode == 0x82 && g_custom_vpd[idx].length >= 24)
                injectSerial(g_custom_vpd[idx].data, 16, id);
            else if (pageCode == 0x83 && g_custom_vpd[idx].length >= 42)
                injectSerial(g_custom_vpd[idx].data, 34, id);
            else if (pageCode == 0xD1 && g_custom_vpd[idx].length >= 78)
                injectSerial(g_custom_vpd[idx].data, 70, id);

            // Inject IBM FRU part number into VPD 0x01 (ASCII at offset 5,
            // EBCDIC at offset 29). Page must be long enough to hold both
            // 7-byte slots.
            if (pageCode == 0x01 && g_custom_vpd[idx].length >= 36)
                injectPartNumber(g_custom_vpd[idx].data, 5, 29, id);

            g_custom_vpd_count++;
        }
    }
}

void parseCustomInquiryData(void)
{
    char tmp[512];
    char section[8];
    char key[8];

    g_custom_vpd_count = 0;
    memset(g_custom_spd, 0, sizeof(g_custom_spd));
    memset(g_as400_serial_override, 0, sizeof(g_as400_serial_override));
    memset(g_as400_part_override, 0, sizeof(g_as400_part_override));

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

        // Parse AS/400 disk-serial override: AS400_DiskSerialNumber=<up to 8 chars>
        // Shorter values are right-padded with ASCII spaces; longer values
        // are truncated to 8 characters.
        if (ini_gets(section, "AS400_DiskSerialNumber", "", tmp, sizeof(tmp), CONFIGFILE))
        {
            size_t slen = strlen(tmp);
            if (slen > 0)
            {
                memset(g_as400_serial_override[id].data, ' ', AS400_DISK_SERIAL_LEN);
                if (slen > AS400_DISK_SERIAL_LEN) slen = AS400_DISK_SERIAL_LEN;
                memcpy(g_as400_serial_override[id].data, tmp, slen);
                g_as400_serial_override[id].length = AS400_DISK_SERIAL_LEN;
                logmsg("Custom AS/400 serial for SCSI ID ", id, ": \"", tmp, "\"");
            }
        }

        // Parse AS/400 disk part-number override: AS400_DiskPartNumber=<up
        // to 7 chars>. Uppercased, space-padded, mapped to both ASCII and
        // IBM CP037 EBCDIC for the two-slot VPD 0x01 layout.
        if (ini_gets(section, "AS400_DiskPartNumber", "", tmp, sizeof(tmp), CONFIGFILE))
        {
            size_t slen = strlen(tmp);
            if (slen > 0)
            {
                memset(g_as400_part_override[id].ascii, ' ', AS400_DISK_PART_LEN);
                memset(g_as400_part_override[id].ebcdic, 0x40, AS400_DISK_PART_LEN); // EBCDIC space
                if (slen > AS400_DISK_PART_LEN) slen = AS400_DISK_PART_LEN;
                for (size_t i = 0; i < slen; i++)
                {
                    char c = tmp[i];
                    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                    g_as400_part_override[id].ascii[i] = (uint8_t)c;
                    g_as400_part_override[id].ebcdic[i] = asciiToEbcdic(c);
                }
                g_as400_part_override[id].length = AS400_DISK_PART_LEN;
                logmsg("Custom AS/400 disk part number for SCSI ID ", id, ": \"", tmp, "\"");
            }
        }
    }

    // Load AS/400 defaults for any IDs that don't have INI overrides
    if (g_scsi_settings.getSystemPreset() == SYS_PRESET_AS400)
    {
        loadAS400Defaults();
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
