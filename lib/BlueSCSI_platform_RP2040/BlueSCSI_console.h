/**
 * Copyright (C) 2024 Eric Helgeson
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

#ifndef BLUESCSI_CONSOLE_H
#define BLUESCSI_CONSOLE_H

#include <cctype>
#include "BlueSCSI_console.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI.h"
#include "BlueSCSI_initiator.h"
#include "stdint.h"
#include <assert.h>
#ifndef __MBED__
# include <Adafruit_TinyUSB.h>
# include <class/cdc/cdc_device.h>
#endif

#define PICO_REBOOT_MAGIC 0x5eeded
#define MAX_CBD_LEN 10
#define NULL_CHAR_LEN 1
#define DISABLE_CHAR '#'
#define MAX_SERIAL_INPUT_CHARS ((MAX_CBD_LEN * 2) + NULL_CHAR_LEN)
#define RESPONSE_BUFFER_LEN 4096
void handleUsbInputTargetMode(int32_t data);
void handleUsbInputInitiatorMode();

void initiatorConsoleLoop();
void serialPoll();
int hexToBytes(const char *hex_string, uint8_t *bytes_array, size_t max_length);
#endif //BLUESCSI_CONSOLE_H
