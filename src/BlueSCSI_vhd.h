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

#ifndef BLUESCSI_VHD_H
#define BLUESCSI_VHD_H

#include <stdint.h>
#include <stddef.h>

#define VHD_FOOTER_SIZE 512

#ifdef __cplusplus
extern "C" {
#endif

/* CHS geometry as returned by vhd_compute_geometry */
typedef struct {
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
} vhd_geometry_t;

/* VHD disk types (footer offset 60) */
#define VHD_DISK_TYPE_FIXED        2
#define VHD_DISK_TYPE_DYNAMIC      3
#define VHD_DISK_TYPE_DIFFERENCING 4

/* Return codes for vhd_parse_fixed_footer */
typedef enum {
    VHD_PARSE_OK               = 0,
    VHD_PARSE_ERR_COOKIE       = -1, /* magic "conectix" missing or len mismatch */
    VHD_PARSE_ERR_VERSION      = -2, /* file-format version != 0x00010000 */
    VHD_PARSE_ERR_CHECKSUM     = -3, /* one's-complement checksum mismatch */
    VHD_PARSE_ERR_TYPE_DYNAMIC = -4, /* disk type 3 (not supported on MCU) */
    VHD_PARSE_ERR_TYPE_DIFF    = -5, /* disk type 4 (not supported on MCU) */
    VHD_PARSE_ERR_TYPE_UNKNOWN = -6, /* disk type not in {2,3,4} */
    VHD_PARSE_ERR_SIZE         = -7  /* current_size zero or implausibly large */
} vhd_parse_result_t;

/* Parsed fields from a fixed VHD footer */
typedef struct {
    uint64_t current_size;        /* offset 48: data bytes, not including footer */
    uint64_t original_size;       /* offset 40 */
    uint16_t cylinders;           /* offset 56 */
    uint8_t  heads;               /* offset 58 */
    uint8_t  sectors_per_track;   /* offset 59 */
    uint32_t disk_type;           /* offset 60 */
    uint32_t timestamp;           /* offset 24 */
    uint32_t creator_version;     /* offset 32 */
    char     creator_app[5];      /* offset 28, 4 chars + NUL */
    char     creator_os[5];       /* offset 36, 4 chars + NUL */
    uint8_t  uuid[16];            /* offset 68 */
    uint8_t  saved_state;         /* offset 84 */
} vhd_footer_info_t;

/**
 * Build a complete 512-byte Fixed VHD footer.
 *
 * @param footer      Output buffer, must be at least VHD_FOOTER_SIZE bytes
 * @param total_bytes Raw disk data size in bytes
 * @param sectorcount Number of SCSI sectors (used for geometry fallback)
 * @param timestamp   Seconds since 2000-01-01 00:00:00 UTC
 * @param scsi_id     SCSI target ID (used in UUID generation)
 */
void vhd_build_fixed_footer(uint8_t *footer, uint64_t total_bytes,
                             uint32_t sectorcount, uint32_t timestamp,
                             uint8_t scsi_id);

/**
 * Compute CHS geometry per the Microsoft VHD specification algorithm.
 *
 * @param total_bytes Raw disk data size in bytes
 * @return CHS geometry struct
 */
vhd_geometry_t vhd_compute_geometry(uint64_t total_bytes);

/**
 * Compute the VHD one's complement checksum over a 512-byte footer.
 * The 4 checksum bytes at offset 64-67 must be zero during computation.
 *
 * @param footer Footer buffer (512 bytes)
 * @param len    Length of footer (should be VHD_FOOTER_SIZE)
 * @return One's complement of the sum of all bytes
 */
uint32_t vhd_compute_checksum(const uint8_t *footer, size_t len);

/**
 * Validate and parse a 512-byte fixed VHD footer.
 *
 * Rejects Dynamic and Differencing VHDs (disk type 3/4) since they require
 * an in-RAM Block Allocation Table that does not fit on the MCU.
 *
 * @param footer Buffer of exactly VHD_FOOTER_SIZE bytes.
 * @param len    Length of buffer (must equal VHD_FOOTER_SIZE).
 * @param out    Populated on VHD_PARSE_OK; untouched otherwise.
 * @return VHD_PARSE_OK (0) on success, negative vhd_parse_result_t on failure.
 */
int vhd_parse_fixed_footer(const uint8_t *footer, size_t len,
                           vhd_footer_info_t *out);

/**
 * Format a VHD UUID into a 16-character SCSI serial-number buffer.
 * Writes lowercase hex of UUID bytes 0..7. The serial field is a fixed
 * 16-byte field — the buffer is NOT NUL-terminated on return.
 *
 * @param uuid   16-byte UUID.
 * @param serial 16-byte buffer (NOT NUL-terminated on return).
 */
void vhd_uuid_to_serial(const uint8_t *uuid, char *serial);

#ifdef __cplusplus
}
#endif

#endif /* BLUESCSI_VHD_H */
