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
 * Panel transport abstraction.
 *
 * The protocol handler (panel_protocol.cpp) is transport-agnostic: it stages
 * async results and reads/writes payload buffers through the panel_transport_*
 * names below. Exactly one physical transport is compiled in per build and
 * provides the implementation:
 *   ENABLE_PANEL_SPI -> panel_spi.cpp  (BlueSCSI Ultra / Ultra Wide, SPI slave)
 *   ENABLE_PANEL_I2C -> panel_i2c.cpp  (BlueSCSI v2, I2C slave)
 */

#pragma once

#if defined(ENABLE_PANEL_I2C)
#include "panel_i2c.h"
#define panel_transport_get_tx_buffer    panel_i2c_get_tx_buffer
#define panel_transport_set_async_result panel_i2c_set_async_result
#define panel_transport_set_async_error  panel_i2c_set_async_error
#elif defined(ENABLE_PANEL_SPI)
#include "panel_spi.h"
#define panel_transport_get_tx_buffer    panel_spi_get_tx_buffer
#define panel_transport_set_async_result panel_spi_set_async_result
#define panel_transport_set_async_error  panel_spi_set_async_error
#endif
