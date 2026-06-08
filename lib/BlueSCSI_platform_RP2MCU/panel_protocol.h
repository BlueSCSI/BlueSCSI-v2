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
 * Panel Protocol Handler
 *
 * Translates panel protocol commands into BlueSCSI operations.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the protocol handler.
 */
void panel_protocol_init(void);

/**
 * Handle a read command (bit 7 = 1).
 * These are synchronous and return data immediately.
 *
 * @param cmd Command code
 * @param arg Command argument
 * @param response Buffer for response data
 * @param max_size Maximum response size
 * @return Size of response data
 */
size_t panel_protocol_handle_read(uint8_t cmd, uint16_t arg,
                                  uint8_t* response, size_t max_size);

/**
 * Handle a write command (bit 7 = 0).
 * May be synchronous or start an async operation.
 *
 * @param cmd Command code
 * @param arg Command argument
 * @param payload Payload data (may be NULL)
 * @param payload_size Size of payload
 * @param payload_crc16 CRC16 of payload (for validation)
 */
void panel_protocol_handle_write(uint8_t cmd, uint16_t arg,
                                 const uint8_t* payload, size_t payload_size,
                                 uint16_t payload_crc16);

/**
 * Poll async operations.
 * Should be called periodically to process background operations.
 */
void panel_protocol_poll(void);

/**
 * Refresh the per-device status snapshot read by the IRQ-context read handlers.
 * MUST be called only from the main loop (it reads img->file.isOpen(), which
 * races switchNextImage()). Call regularly from panel_spi_poll().
 */
void panel_protocol_refresh_device_snapshot(void);

/**
 * Drain IRQ-context telemetry into the log.
 * Some events (e.g. unknown read cmd) are recorded by the DMA IRQ handler
 * and emitted by the main loop here, since logmsg is not IRQ-safe.
 */
void panel_protocol_drain_irq_log(void);

#ifdef __cplusplus
}
#endif
