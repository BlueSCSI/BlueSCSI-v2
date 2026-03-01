/*
 * Copyright (C) 2026 Eric Helgeson
 *
 * BlueSCSI - Fixed VHD footer generation
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

#include "BlueSCSI_vhd.h"
#include <string.h>

/* Write a big-endian uint16 to buffer */
static void write_be16(uint8_t *p, uint16_t val) {
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val);
}

/* Write a big-endian uint32 to buffer */
static void write_be32(uint8_t *p, uint32_t val) {
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)(val);
}

/* Write a big-endian uint64 to buffer */
static void write_be64(uint8_t *p, uint64_t val) {
    write_be32(p, (uint32_t)(val >> 32));
    write_be32(p + 4, (uint32_t)(val));
}

uint32_t vhd_compute_checksum(const uint8_t *footer, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += footer[i];
    }
    return ~sum;
}

vhd_geometry_t vhd_compute_geometry(uint64_t total_bytes)
{
    /*
     * Microsoft VHD Specification geometry algorithm.
     * Input: total disk size in bytes.
     * The algorithm works on 512-byte sector count.
     */
    vhd_geometry_t geo;
    uint32_t totalSectors = (uint32_t)(total_bytes / 512);

    /* Cap at maximum CHS addressable sectors */
    if (totalSectors > 65535U * 16 * 255) {
        totalSectors = 65535U * 16 * 255;
    }

    uint32_t cylinderTimesHeads;
    uint32_t heads;
    uint32_t sectorsPerTrack;

    if (totalSectors >= 65535U * 16 * 63) {
        sectorsPerTrack = 255;
        heads = 16;
        cylinderTimesHeads = totalSectors / sectorsPerTrack;
    } else {
        sectorsPerTrack = 17;
        cylinderTimesHeads = totalSectors / sectorsPerTrack;

        heads = (cylinderTimesHeads + 1023) / 1024;
        if (heads < 4) {
            heads = 4;
        }

        if (cylinderTimesHeads >= (heads * 1024) || heads > 16) {
            sectorsPerTrack = 31;
            heads = 16;
            cylinderTimesHeads = totalSectors / sectorsPerTrack;
        }

        if (cylinderTimesHeads >= (heads * 1024)) {
            sectorsPerTrack = 63;
            heads = 16;
            cylinderTimesHeads = totalSectors / sectorsPerTrack;
        }
    }

    uint32_t cyl32 = cylinderTimesHeads / heads;
    if (cyl32 > 65535) {
        cyl32 = 65535;
    }

    geo.cylinders = (uint16_t)cyl32;
    geo.heads = (uint8_t)heads;
    geo.sectors_per_track = (uint8_t)sectorsPerTrack;
    return geo;
}

/*
 * Simple deterministic hash for UUID generation.
 * Mixes timestamp, SCSI ID, and disk size into 16 bytes.
 * Uses a basic multiplicative hash (FNV-1a inspired).
 */
static void vhd_generate_uuid(uint8_t *uuid, uint32_t timestamp,
                               uint8_t scsi_id, uint64_t total_bytes)
{
    /* Seed with input data packed into a buffer */
    uint8_t seed[16];
    memset(seed, 0, sizeof(seed));
    write_be32(&seed[0], timestamp);
    seed[4] = scsi_id;
    write_be64(&seed[5], total_bytes);
    /* seed[13..15] remain zero, providing padding */

    /* FNV-1a-like mixing to produce 16 bytes */
    uint32_t h0 = 0x811C9DC5U;
    uint32_t h1 = 0x01000193U;
    uint32_t h2 = 0xDEADBEEFU;
    uint32_t h3 = 0xCAFEBABEU;

    for (int i = 0; i < 16; i++) {
        h0 ^= seed[i];
        h0 *= 0x01000193U;
        h1 ^= seed[(i + 3) % 16];
        h1 *= 0x01000193U;
        h2 ^= seed[(i + 7) % 16];
        h2 *= 0x01000193U;
        h3 ^= seed[(i + 11) % 16];
        h3 *= 0x01000193U;
    }

    write_be32(&uuid[0], h0);
    write_be32(&uuid[4], h1);
    write_be32(&uuid[8], h2);
    write_be32(&uuid[12], h3);
}

void vhd_build_fixed_footer(uint8_t *footer, uint64_t total_bytes,
                             uint32_t sectorcount, uint32_t timestamp,
                             uint8_t scsi_id)
{
    (void)sectorcount; /* Reserved for future use / geometry fallback */

    /* Zero entire footer first */
    memset(footer, 0, VHD_FOOTER_SIZE);

    /* Offset 0: Cookie "conectix" */
    memcpy(&footer[0], "conectix", 8);

    /* Offset 8: Features (0x00000002 = reserved, always set) */
    write_be32(&footer[8], 0x00000002);

    /* Offset 12: File Format Version (1.0) */
    write_be32(&footer[12], 0x00010000);

    /* Offset 16: Data Offset (fixed disk = no dynamic header) */
    write_be64(&footer[16], 0xFFFFFFFFFFFFFFFFULL);

    /* Offset 24: Time Stamp */
    write_be32(&footer[24], timestamp);

    /* Offset 28: Creator Application */
    memcpy(&footer[28], "bsci", 4);

    /* Offset 32: Creator Version (1.0) */
    write_be32(&footer[32], 0x00010000);

    /* Offset 36: Creator Host OS ("Wi2k" = Windows) */
    memcpy(&footer[36], "Wi2k", 4);

    /* Offset 40: Original Size */
    write_be64(&footer[40], total_bytes);

    /* Offset 48: Current Size */
    write_be64(&footer[48], total_bytes);

    /* Offset 56: Disk Geometry (CHS) */
    vhd_geometry_t geo = vhd_compute_geometry(total_bytes);
    write_be16(&footer[56], geo.cylinders);
    footer[58] = geo.heads;
    footer[59] = geo.sectors_per_track;

    /* Offset 60: Disk Type (2 = Fixed) */
    write_be32(&footer[60], 0x00000002);

    /* Offset 64: Checksum — must be computed last */
    /* Leave as zero for now */

    /* Offset 68: Unique Id (16 bytes) */
    vhd_generate_uuid(&footer[68], timestamp, scsi_id, total_bytes);

    /* Offset 84: Saved State = 0 (already zeroed) */
    /* Offset 85-511: Reserved = 0 (already zeroed) */

    /* Now compute and store checksum */
    uint32_t checksum = vhd_compute_checksum(footer, VHD_FOOTER_SIZE);
    write_be32(&footer[64], checksum);
}
