/**
 * Copyright (C) 2026 Eric Helgeson
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

// Pure math for converting between an absolute CD LBA and a byte offset
// inside the BIN file that backs a given track. Header-only and with no
// dependency beyond CUEParser.h so the unit tests can link against the
// same implementation the SPDIF and I2S audio backends use, without the
// test stub of BlueSCSI_audio.h shadowing it.
//
// For a track with unstored PREGAP, file_offset corresponds to data_start
// (INDEX 01), so the conversion is always relative to data_start — never
// track_start.

#pragma once

#include <stdint.h>
#include <CUEParser.h>

static inline uint64_t audio_offset_from_lba(const CUETrackInfo &t, uint32_t lba)
{
    return t.file_offset
         + (int64_t)t.sector_length * ((int64_t)lba - (int64_t)t.data_start);
}

static inline uint32_t audio_lba_from_offset(const CUETrackInfo &t, uint64_t fpos)
{
    // Fallback when no track is cached: assume a trivial single-track audio
    // BIN starting at LBA 0. Matches the pre-fix behaviour on SPDIF and is
    // only correct for that degenerate case.
    if (t.sector_length == 0)
    {
        return (uint32_t)(fpos / 2352);
    }
    uint64_t base = t.file_offset;
    uint64_t delta = (fpos >= base) ? (fpos - base) : 0;
    return t.data_start + (uint32_t)(delta / t.sector_length);
}
