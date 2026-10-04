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
 * Panel SPI Slave Driver
 *
 * Handles SPI communication with ESP32-C3 front panel.
 * BlueSCSI acts as SPI slave, ESP32 is master.
 *
 * Protocol:
 *   Phase 1: 5-byte header exchange
 *   Phase 2: Variable payload exchange (0-4096 bytes)
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the panel SPI slave interface.
 * Sets up GPIO, SPI peripheral in slave mode, and DMA channels.
 *
 * @return true on success, false on failure
 */
bool panel_spi_init(void);

/**
 * Deinitialize the panel SPI interface.
 * Releases DMA channels and resets GPIO.
 */
void panel_spi_deinit(void);

/**
 * Poll the panel SPI interface.
 * Should be called from platform_poll() regularly.
 * Handles completed DMA transfers and dispatches commands.
 */
void panel_spi_poll(void);

/**
 * Check if panel SPI is initialized and operational.
 *
 * @return true if initialized
 */
bool panel_spi_is_initialized(void);

/**
 * Set the async operation result.
 * Called by protocol handlers when async operation completes.
 *
 * @param data Response data buffer
 * @param size Size of response data
 */
void panel_spi_set_async_result(const uint8_t* data, size_t size);

/**
 * Signal that an async operation has completed with error.
 */
void panel_spi_set_async_error(void);

/**
 * Get the RX payload buffer for reading incoming data.
 *
 * @return Pointer to RX payload buffer
 */
uint8_t* panel_spi_get_rx_buffer(void);

/**
 * Get the TX payload buffer for writing outgoing data.
 *
 * @return Pointer to TX payload buffer
 */
uint8_t* panel_spi_get_tx_buffer(void);

#ifdef __cplusplus
}
#endif
