/**
 * BlueSCSI - Copyright (c) 2026 Eric Helgeson
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
 *
 * ----
 *
 * SHA-256 compatibility shim for the panel firmware-relay path.
 *
 * The RP2350 (Ultra/Ultra Wide) has a hardware SHA-256 block exposed by the
 * pico-sdk's pico_sha256 library. The RP2040 (v2) has neither the hardware nor
 * pico/sha256.h, so it gets a drop-in software implementation exposing the same
 * pico_sha256_* API subset panel_protocol.cpp uses. Callers include this header
 * instead of <pico/sha256.h> and are otherwise unchanged.
 */

#pragma once

#if defined(BLUESCSI_MCU_RP23XX)

// RP2350: use the hardware SHA-256 accelerator via the pico-sdk.
#include <pico/sha256.h>

#else

// RP2040: software SHA-256 with a pico_sha256-compatible front end.
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pico/error.h>   // PICO_OK

#ifdef __cplusplus
extern "C" {
#endif

enum sha256_endianness {
    SHA256_LITTLE_ENDIAN = 0,
    SHA256_BIG_ENDIAN = 1,
};

typedef struct {
    uint8_t bytes[32];
} sha256_result_t;

typedef struct pico_sha256_state {
    uint32_t state[8];
    uint64_t bitlen;
    uint32_t datalen;
    uint8_t  data[64];
} pico_sha256_state_t;

// Endianness/use_dma are accepted for API compatibility; the software path
// always produces the standard big-endian digest. Always returns PICO_OK.
int  pico_sha256_try_start(pico_sha256_state_t *state, enum sha256_endianness endianness, bool use_dma);
void pico_sha256_update(pico_sha256_state_t *state, const uint8_t *data, size_t data_size_bytes);
void pico_sha256_finish(pico_sha256_state_t *state, sha256_result_t *out);
void pico_sha256_cleanup(pico_sha256_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // BLUESCSI_MCU_RP23XX
