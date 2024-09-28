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
// num_blocks is the count of data blocks to transfer
// block_size is the number of *bytes* to transfer in each block (must be evenly divisible by 4)
// A CRC is expected after every data block
sdio_status_t rp2040_sdio_rx_start(uint8_t *buffer, uint32_t num_blocks, uint32_t block_size);

// Check if reception is complete
// Returns SDIO_BUSY while transferring, SDIO_OK when done and error on failure.
sdio_status_t rp2040_sdio_rx_poll(uint32_t *bytes_complete = nullptr);

// Start transferring data from memory to SD card
sdio_status_t rp2040_sdio_tx_start(const uint8_t *buffer, uint32_t num_blocks);

// Check if transmission is complete
sdio_status_t rp2040_sdio_tx_poll(uint32_t *bytes_complete = nullptr);

// Force everything to idle state
sdio_status_t rp2040_sdio_stop();

// Performs one full CLK line cycle
void cycleSdClock();

// Receives the SD Status register.  Does not return until the register has been received.
sdio_status_t receive_status_register(uint8_t* sds);

// (Re)initialize the SDIO interface
void rp2040_sdio_init(int clock_divider = 1);

// Adds extra clock delay as specified in additional_delay
void rp2040_sdio_delay_increment(uint16_t additional_delay);
