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
 * Panel I2C Slave Driver
 *
 * Handles I2C communication with the ESP32-C3 front panel on BlueSCSI v2.
 * BlueSCSI acts as the I2C slave; the ESP32 is the master. It drives the same
 * transport-agnostic panel_protocol handlers as the SPI slave (panel_spi.cpp),
 * so only the wire mechanics differ.
 *
 * Protocol (matches the SPI slave / panel_protocol_defs.h):
 *   Phase 1: master writes a 5-byte header (+ payload for write commands)
 *   Phase 2: master reads the response in a separate transaction
 *   Async results are drained via POLL_OP_READY then a follow-up read.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the panel I2C slave interface (i2c0 @ 0x50, 400 kHz on the v2
 * GPIO16/17 pins). Call only when the front panel is enabled in the INI;
 * it claims those pins exclusively (no buttons / SPDIF on them).
 *
 * @return true on success, false on failure
 */
bool panel_i2c_init(void);

/** Deinitialize the panel I2C interface and release the pins. */
void panel_i2c_deinit(void);

/**
 * Poll the panel I2C interface from the main loop. Refreshes the device
 * snapshot and dispatches deferred write commands once the SCSI bus is idle.
 */
void panel_i2c_poll(void);

/** @return true if the I2C slave is initialized and operational. */
bool panel_i2c_is_initialized(void);

/** Stage an async operation result (called by protocol handlers). */
void panel_i2c_set_async_result(const uint8_t* data, size_t size);

/** Signal that an async operation completed with error. */
void panel_i2c_set_async_error(void);

/** @return the TX payload buffer protocol handlers stage async results into. */
uint8_t* panel_i2c_get_tx_buffer(void);

#ifdef __cplusplus
}
#endif
