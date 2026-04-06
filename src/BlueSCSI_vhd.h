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

#ifdef __cplusplus
}
#endif

#endif /* BLUESCSI_VHD_H */
