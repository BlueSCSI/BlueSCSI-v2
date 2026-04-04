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

/**
 * TinyUSB configuration for BlueSCSI
 *
 * Composite CDC (serial) + MSC (mass storage) device.
 * This file must be in the include path before any TinyUSB headers.
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// Board/MCU specific — let the SDK define CFG_TUSB_MCU and CFG_TUSB_OS

// RHPort mode — required by tusb_init() with no arguments
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

// USB device configuration
#define CFG_TUD_ENABLED       1
#define CFG_TUD_ENDPOINT0_SIZE 64

// USB device class configuration
#define CFG_TUD_CDC           1
#define CFG_TUD_MSC           1
#define CFG_TUD_HID           0
#define CFG_TUD_MIDI          0
#define CFG_TUD_VENDOR        0

// CDC FIFO size
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256
#define CFG_TUD_CDC_EP_BUFSIZE 64

// MSC buffer size
#define CFG_TUD_MSC_EP_BUFSIZE 512

#ifdef __cplusplus
}
#endif

#endif // TUSB_CONFIG_H
