// SD card access using SDIO for RP2040 platform.
// This module contains the low-level SDIO bus implementation using
// the PIO peripheral. The high-level commands are in sd_card_sdio.cpp.

#pragma once
#include <stdint.h>

enum sdio_status_t {
    SDIO_OK = 0,
    SDIO_ERR_RESPONSE_TIMEOUT = 1, // Timed out waiting for response from card
    SDIO_ERR_CRC = 2,              // Response CRC is wrong
};

// Execute a command that has 48-bit reply (response types R1, R3, R6 and R7)
// If response is NULL, does not wait for reply.
sdio_status_t rp2040_sdio_command_R1(uint8_t command, uint32_t arg, uint32_t *response);

// Execute a command that has 136-bit reply (response type R2)
sdio_status_t rp2040_sdio_command_R2(uint8_t command, uint32_t arg, uint32_t response[4]);

// Start transferring data from SD card to memory buffer
sdio_status_t rp2040_sdio_rx_start(uint8_t *buffer, uint32_t num_bytes);

// Check if reception is complete
bool rp2040_sdio_rx_poll();

// Start transferring data from memory to SD card
sdio_status_t rp2040_sdio_tx_start(const uint8_t *buffer, uint32_t num_bytes);

// Check if transmission is complete
bool rp2040_sdio_tx_poll();

// (Re)initialize the SDIO interface
void rp2040_sdio_init();
