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
//
// The crc_wait loop in sdio_tx_w_clock polls DAT0 for the start bit while it
// clocks. Cards whose output delay puts that edge past the sample point make
// the loop run one more time, and the token is shifted a bit left - 0x50,
// 0xB0, 0xD0. Those values cannot occur aligned, since an aligned token always
// begins with a zero start bit and ends with a one, so decoding both forms is
// unambiguous. Seen on Kensington (MID 0x41) and Phison SD128 (MID 0x27).
static inline sdio_write_response_t sdio_classify_write_response_ultra(uint32_t card_response)
{
    switch (card_response & 0xF8)
    {
        case 0x28:
        case 0x50: return SDIO_WR_ACCEPTED;
        case 0x58:
        case 0xB0: return SDIO_WR_CRC_ERROR;
        case 0x30:
        case 0x68:
        case 0xD0: return SDIO_WR_WRITE_ERROR;
        default:   return SDIO_WR_UNKNOWN;
    }
}
