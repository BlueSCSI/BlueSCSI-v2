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

// AS/400 SCSI disk command extensions - bit manipulation helpers
// These are pure functions used by the skip read/write implementation
// in BlueSCSI_disk.cpp.

#include "BlueSCSI_disk_as400.h"

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)

#include <stdint.h>
#include <stdlib.h>

int as400_skip_total_true_bits(const uint8_t *mask, size_t masklen)
{
    int total = 0;
    for (size_t i = 0; i < masklen; i++)
    {
        uint8_t val = mask[i];
        while (val)
        {
            val &= (val - 1);
            total++;
        }
    }
    return total;
}

int as400_skip_contiguous_bits(const uint8_t *data, size_t byte_len, size_t bit_start)
{
    size_t total_bits = byte_len * 8;
    if (bit_start >= total_bits) return 0;

    size_t byte_index = bit_start / 8;
    int bit_offset = 7 - (bit_start % 8); // MSB-first
    int target_bit = (data[byte_index] >> bit_offset) & 1;

    size_t count = 0;
    for (size_t i = bit_start; i < total_bits; i++)
    {
        byte_index = i / 8;
        bit_offset = 7 - (i % 8);
        int current_bit = (data[byte_index] >> bit_offset) & 1;

        if (current_bit != target_bit) break;
        count++;
    }

    return target_bit ? (int)count : -(int)count;
}

#endif // BLUESCSI_ULTRA || BLUESCSI_ULTRA_WIDE
