/**
 * BlueSCSI™ - Copyright (c) 2026 Eric Helgeson <eric@bluescsi.com>
 *
 * BlueSCSI™ firmware is licensed under the GPL version 3 or any later version.
 *
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
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

// Decoding of the SD data response token the card drives on DAT0 after each
// written block. Kept free of SDK dependencies so it can be unit tested.

#pragma once

#include <stdint.h>

enum sdio_write_response_t {
    SDIO_WR_ACCEPTED,
    SDIO_WR_CRC_ERROR,
    SDIO_WR_WRITE_ERROR,
    SDIO_WR_UNKNOWN
};

// The token is five bits, 0 s s s 1:
//   0b00101 accepted, 0b01011 CRC error, 0b01101 write error.
// The Ultra PIO shifts it into an 8 bit register, so an aligned token lands in
// bits 7:3 - 0x28, 0x58, 0x68.
static inline sdio_write_response_t sdio_classify_write_response_ultra(uint32_t card_response)
{
    switch (card_response & 0xF8)
    {
        case 0x28: return SDIO_WR_ACCEPTED;
        case 0x58: return SDIO_WR_CRC_ERROR;
        case 0x30:
        case 0x68: return SDIO_WR_WRITE_ERROR;
        default:   return SDIO_WR_UNKNOWN;
    }
}
