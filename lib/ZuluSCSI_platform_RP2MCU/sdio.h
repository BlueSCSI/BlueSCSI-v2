/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// SD card access using SDIO for RP2040 platform.
// This module contains the low-level SDIO bus implementation using
// the PIO peripheral. The high-level commands are in sd_card_sdio.cpp.

#pragma once
#include <stdint.h>

enum sdio_status_t {
    SDIO_OK = 0,
    SDIO_BUSY = 1,
    SDIO_ERR_RESPONSE_TIMEOUT = 2, // Timed out waiting for response from card
    SDIO_ERR_RESPONSE_CRC = 3,     // Response CRC is wrong
    SDIO_ERR_RESPONSE_CODE = 4,    // Response command code does not match what was sent
    SDIO_ERR_DATA_TIMEOUT = 5,     // Timed out waiting for data block
    SDIO_ERR_DATA_CRC = 6,         // CRC for data packet is wrong
    SDIO_ERR_WRITE_CRC = 7,        // Card reports bad CRC for write
    SDIO_ERR_WRITE_FAIL = 8,       // Card reports write failure
};

#define SDIO_BLOCK_SIZE 512
#define SDIO_WORDS_PER_BLOCK 128

// Execute a command that has 48-bit reply (response types R1, R6, R7)
// If response is NULL, does not wait for reply.
sdio_status_t rp2040_sdio_command_R1(uint8_t command, uint32_t arg, uint32_t *response);

// Execute a command that has 136-bit reply (response type R2)
// Response buffer should have space for 16 bytes (the 128 bit payload)
sdio_status_t rp2040_sdio_command_R2(uint8_t command, uint32_t arg, uint8_t *response);

// Execute a command that has 48-bit reply but without CRC (response R3)
sdio_status_t rp2040_sdio_command_R3(uint8_t command, uint32_t arg, uint32_t *response);

// Start transferring data from SD card to memory buffer
// Transfer block size is always 512 bytes except for certain special commands
sdio_status_t rp2040_sdio_rx_start(uint8_t *buffer, uint32_t num_blocks, uint32_t num_size = SDIO_BLOCK_SIZE);

// Check if reception is complete
// Returns SDIO_BUSY while transferring, SDIO_OK when done and error on failure.
sdio_status_t rp2040_sdio_rx_poll(uint32_t *bytes_complete = nullptr);

// Start transferring data from memory to SD card
sdio_status_t rp2040_sdio_tx_start(const uint8_t *buffer, uint32_t num_blocks);

// Check if transmission is complete
sdio_status_t rp2040_sdio_tx_poll(uint32_t *bytes_complete = nullptr);

// Force everything to idle state
sdio_status_t rp2040_sdio_stop();

// Receives the SD Status register.  Does not return until the register has been received.
sdio_status_t receive_status_register(uint8_t* sds);

// (Re)initialize the SDIO interface
void rp2040_sdio_init(int clock_divider = 1);
