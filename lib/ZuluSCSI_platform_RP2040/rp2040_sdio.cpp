// Implementation of SDIO communication for RP2040
//
// The RP2040 official work-in-progress code at
// https://github.com/raspberrypi/pico-extras/tree/master/src/rp2_common/pico_sd_card
// may be useful reference, but this is independent implementation.
//
// For official SDIO specifications, refer to:
// https://www.sdcard.org/downloads/pls/
// "SDIO Physical Layer Simplified Specification Version 8.00"

#include "rp2040_sdio.h"
#include "rp2040_sdio.pio.h"
#include <hardware/pio.h>
#include <hardware/gpio.h>
#include <ZuluSCSI_platform.h>
#include <ZuluSCSI_log.h>

#define SDIO_PIO pio1
#define SDIO_CMD_SM 0
#define SDIO_DATA_SM 1

static struct {
    uint32_t pio_cmd_clk_offset;
} g_sdio;

// Table lookup for calculating CRC-7 checksum that is used in SDIO command packets.
// Usage:
//    uint8_t crc = 0;
//    crc = crc7_table[crc ^ byte];
//    .. repeat for every byte ..
static const uint8_t crc7_table[256] = {
	0x00, 0x12, 0x24, 0x36, 0x48, 0x5a, 0x6c, 0x7e,	0x90, 0x82, 0xb4, 0xa6, 0xd8, 0xca, 0xfc, 0xee,
	0x32, 0x20, 0x16, 0x04, 0x7a, 0x68, 0x5e, 0x4c,	0xa2, 0xb0, 0x86, 0x94, 0xea, 0xf8, 0xce, 0xdc,
	0x64, 0x76, 0x40, 0x52, 0x2c, 0x3e, 0x08, 0x1a,	0xf4, 0xe6, 0xd0, 0xc2, 0xbc, 0xae, 0x98, 0x8a,
	0x56, 0x44, 0x72, 0x60, 0x1e, 0x0c, 0x3a, 0x28,	0xc6, 0xd4, 0xe2, 0xf0, 0x8e, 0x9c, 0xaa, 0xb8,
	0xc8, 0xda, 0xec, 0xfe, 0x80, 0x92, 0xa4, 0xb6,	0x58, 0x4a, 0x7c, 0x6e, 0x10, 0x02, 0x34, 0x26,
	0xfa, 0xe8, 0xde, 0xcc, 0xb2, 0xa0, 0x96, 0x84,	0x6a, 0x78, 0x4e, 0x5c, 0x22, 0x30, 0x06, 0x14,
	0xac, 0xbe, 0x88, 0x9a, 0xe4, 0xf6, 0xc0, 0xd2,	0x3c, 0x2e, 0x18, 0x0a, 0x74, 0x66, 0x50, 0x42,
	0x9e, 0x8c, 0xba, 0xa8, 0xd6, 0xc4, 0xf2, 0xe0,	0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54, 0x62, 0x70,
	0x82, 0x90, 0xa6, 0xb4, 0xca, 0xd8, 0xee, 0xfc,	0x12, 0x00, 0x36, 0x24, 0x5a, 0x48, 0x7e, 0x6c,
	0xb0, 0xa2, 0x94, 0x86, 0xf8, 0xea, 0xdc, 0xce,	0x20, 0x32, 0x04, 0x16, 0x68, 0x7a, 0x4c, 0x5e,
	0xe6, 0xf4, 0xc2, 0xd0, 0xae, 0xbc, 0x8a, 0x98,	0x76, 0x64, 0x52, 0x40, 0x3e, 0x2c, 0x1a, 0x08,
	0xd4, 0xc6, 0xf0, 0xe2, 0x9c, 0x8e, 0xb8, 0xaa,	0x44, 0x56, 0x60, 0x72, 0x0c, 0x1e, 0x28, 0x3a,
	0x4a, 0x58, 0x6e, 0x7c, 0x02, 0x10, 0x26, 0x34,	0xda, 0xc8, 0xfe, 0xec, 0x92, 0x80, 0xb6, 0xa4,
	0x78, 0x6a, 0x5c, 0x4e, 0x30, 0x22, 0x14, 0x06,	0xe8, 0xfa, 0xcc, 0xde, 0xa0, 0xb2, 0x84, 0x96,
	0x2e, 0x3c, 0x0a, 0x18, 0x66, 0x74, 0x42, 0x50,	0xbe, 0xac, 0x9a, 0x88, 0xf6, 0xe4, 0xd2, 0xc0,
	0x1c, 0x0e, 0x38, 0x2a, 0x54, 0x46, 0x70, 0x62,	0x8c, 0x9e, 0xa8, 0xba, 0xc4, 0xd6, 0xe0, 0xf2
};

sdio_status_t rp2040_sdio_command_R1(uint8_t command, uint32_t arg, uint32_t *response)
{
    azdbg("Command: ", command, " arg ", arg);

    // Format the arguments in the way expected by the PIO code.
    uint32_t word0 =
        (47 << 24) | // Number of bits in command minus one
        ( 1 << 22) | // Transfer direction from host to card
        (command << 16) | // Command byte
        (((arg >> 24) & 0xFF) << 8) | // MSB byte of argument
        (((arg >> 16) & 0xFF) << 0);
    
    uint32_t word1 =
        (((arg >> 8) & 0xFF) << 24) |
        (((arg >> 0) & 0xFF) << 16) | // LSB byte of argument
        ( 1 << 8); // End bit

    // Set number of bits in response minus one, or leave at 0 if no response expected
    if (response)
    {
        word1 |= (47 << 0);
    }

    // Calculate checksum in the order that the bytes will be transmitted (big-endian)
    uint8_t crc = 0;
    crc = crc7_table[crc ^ ((word0 >> 16) & 0xFF)];
    crc = crc7_table[crc ^ ((word0 >>  8) & 0xFF)];
    crc = crc7_table[crc ^ ((word0 >>  0) & 0xFF)];
    crc = crc7_table[crc ^ ((word1 >> 24) & 0xFF)];
    crc = crc7_table[crc ^ ((word1 >> 16) & 0xFF)];
    word1 |= crc << 8;
    
    // Transmit command
    pio_sm_clear_fifos(SDIO_PIO, SDIO_CMD_SM);
    pio_sm_put(SDIO_PIO, SDIO_CMD_SM, word0);
    pio_sm_put(SDIO_PIO, SDIO_CMD_SM, word1);

    // Wait for response
    uint32_t start = millis();
    uint32_t wait_words = response ? 2 : 1;
    while (pio_sm_get_rx_fifo_level(SDIO_PIO, SDIO_CMD_SM) < wait_words)
    {
        if ((uint32_t)(millis() - start) > 2)
        {
            azdbg("Timeout waiting for response in rp2040_sdio_command_R1(), ",
                  "PIO PC: ", (int)pio_sm_get_pc(SDIO_PIO, SDIO_CMD_SM) - (int)g_sdio.pio_cmd_clk_offset,
                  " RXF: ", (int)pio_sm_get_rx_fifo_level(SDIO_PIO, SDIO_CMD_SM),
                  " TXF: ", (int)pio_sm_get_tx_fifo_level(SDIO_PIO, SDIO_CMD_SM));

            // Reset the state machine program
            pio_sm_clear_fifos(SDIO_PIO, SDIO_CMD_SM);
            pio_sm_exec(SDIO_PIO, SDIO_CMD_SM, pio_encode_jmp(g_sdio.pio_cmd_clk_offset));
            return SDIO_ERR_RESPONSE_TIMEOUT;
        }
    }

    delay(1);
    azdbg("PIO PC: ", (int)pio_sm_get_pc(SDIO_PIO, SDIO_CMD_SM) - (int)g_sdio.pio_cmd_clk_offset,
                  " RXF: ", (int)pio_sm_get_rx_fifo_level(SDIO_PIO, SDIO_CMD_SM),
                  " TXF: ", (int)pio_sm_get_tx_fifo_level(SDIO_PIO, SDIO_CMD_SM));

    if (response)
    {
        // Read out response packet
        uint32_t resp0 = pio_sm_get(SDIO_PIO, SDIO_CMD_SM);
        uint32_t resp1 = pio_sm_get(SDIO_PIO, SDIO_CMD_SM);
        azdbg(resp0, " ", resp1);

        // Calculate response checksum
        crc = 0;
        crc = crc7_table[crc ^ ((resp0 >> 24) & 0xFF)];
        crc = crc7_table[crc ^ ((resp0 >> 16) & 0xFF)];
        crc = crc7_table[crc ^ ((resp0 >>  8) & 0xFF)];
        crc = crc7_table[crc ^ ((resp0 >>  0) & 0xFF)];
        crc = crc7_table[crc ^ ((resp1 >>  8) & 0xFF)];

        uint8_t actual_crc = ((resp1 >> 0) & 0xFE);
        if (crc != actual_crc)
        {
            azdbg("CRC error in rp2040_sdio_command_R1(): calculated ", crc, " packet has ", actual_crc);
            return SDIO_ERR_CRC;
        }

        *response = ((resp0 & 0xFFFFFF) << 8) | ((resp1 >> 8) & 0xFF);
    }
    else
    {
        // Read out dummy marker
        pio_sm_get(SDIO_PIO, SDIO_CMD_SM);
    }

    return SDIO_OK;
}

void rp2040_sdio_init()
{
    azdbg("rp2040_sdio_init()");

    // Mark resources as being in use, unless it has been done already.
    static bool resources_claimed = false;
    if (!resources_claimed)
    {
        pio_sm_claim(SDIO_PIO, SDIO_CMD_SM);
        pio_sm_claim(SDIO_PIO, SDIO_DATA_SM);
        resources_claimed = true;
    }

    // Load PIO programs
    pio_clear_instruction_memory(SDIO_PIO);

    // Command & clock state machine
    g_sdio.pio_cmd_clk_offset = pio_add_program(SDIO_PIO, &sdio_cmd_clk_program);
    pio_sm_config cfg = sdio_cmd_clk_program_get_default_config(g_sdio.pio_cmd_clk_offset);
    sm_config_set_out_pins(&cfg, SDIO_CMD, 1);
    sm_config_set_in_pins(&cfg, SDIO_CMD);
    sm_config_set_set_pins(&cfg, SDIO_CMD, 1);
    sm_config_set_jmp_pin(&cfg, SDIO_CMD);
    sm_config_set_sideset_pins(&cfg, SDIO_CLK);
    sm_config_set_out_shift(&cfg, false, true, 32);
    sm_config_set_in_shift(&cfg, false, true, 32);
    sm_config_set_clkdiv_int_frac(&cfg, 5, 0);
    sm_config_set_mov_status(&cfg, STATUS_TX_LESSTHAN, 2);

    pio_sm_init(SDIO_PIO, SDIO_CMD_SM, g_sdio.pio_cmd_clk_offset, &cfg);
    pio_sm_set_consecutive_pindirs(SDIO_PIO, SDIO_CMD_SM, SDIO_CLK, 1, true);
    pio_sm_set_enabled(SDIO_PIO, SDIO_CMD_SM, true);

    // Redirect GPIOs to PIO
    gpio_set_function(SDIO_CMD, GPIO_FUNC_PIO1);
    gpio_set_function(SDIO_CLK, GPIO_FUNC_PIO1);
}
