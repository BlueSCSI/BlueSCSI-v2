/** 
 * This file is originally part of ZuluSCSI adopted for BlueSCSI
 *
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * Copyright (c) 2024 Tech by Androda, LLC
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

// Driver for accessing SD card in SDIO mode on RP2040 and RP23XX.

#include "BlueSCSI_platform.h"

#if defined(SD_USE_SDIO) && !defined(SD_USE_RP2350_SDIO)

#include "BlueSCSI_log.h"
#include "sdio.h"
#include "timings_RP2MCU.h"
#include <hardware/gpio.h>
#include <SdFat.h>
#include <SdCard/SdCardInfo.h>

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE) 
#define ULTRA_SDIO
#endif

static uint32_t g_sdio_ocr; // Operating condition register from card
static uint32_t g_sdio_rca; // Relative card address
cid_t g_sdio_cid;
static csd_t g_sdio_csd;
static sds_t __attribute__((aligned(4))) g_sdio_sds;
static int g_sdio_error_line;
static sdio_status_t g_sdio_error;
static uint32_t g_sdio_dma_buf[128];
static uint32_t g_sdio_sector_count;
static float current_sdio_clock_divisor;
static uint8_t current_sdio_speed_mode;
bool g_record_sdio_errors = true; // Normally, record errors.  During autoconfig, turn off.

#ifdef ULTRA_SDIO
// struct cmd6_response_t is MIT licensed from Greiman
// Fields are all right to left indexed
struct cmd6_response_t {
    // Decimal value of this is the max current consumption in mA
    // The bits here should be swapped.
    // 0xFF00 >> 8
    // 0x00FF << 8
    // 511:496
    uint16_t max_current;
    // 495:480
    uint16_t function_group_6;
    // 479:464
    uint16_t function_group_5;
    // 1 << 8 == Power limit 0
    // 1 << 9 == Power limit 1
    // 1 << 10 == Power limit 2
    // 1 << 11 == Power limit 3
    // 1 << 0 == Power limit 9
    // 1 << 1 == Power limit 10
    // 455:448, 463:456
    uint16_t function_group_4;
    // 1 << 8 == Driver strength 0
    // 1 << 9 == Driver strength 1
    // 1 << 10 == Driver strength 2
    // 1 << 11 == Driver strength 3
    // 1 << 0 == Driver strength 9
    // 1 << 1 == Driver strength 10
    // 439:432, 447:440
    uint16_t function_group_3;
    // 1 << 8 == Command system 0
    // 1 << 9 == Command system 1
    // 1 << 10 == Command system 2
    // 1 << 11 == Command system 3
    // 1 << 0 == Command system 9
    // 1 << 1 == Command system 10
    // 423:416, 423:416
    uint16_t function_group_2;
    // 1 << 8 == Access mode 0
    // 1 << 9 == Access mode 1
    // 1 << 10 == Access mode 2
    // 1 << 11 == Access mode 3
    // 1 << 0 == Access Mode 9
    // 1 << 1 == Access Mode 10
    // 407:400, 415:408
    uint16_t function_group_1;
    // Function Group 6: Left 4 bits (0xF0 mask) indicate which functions can be switched (mode 0), or which were switched (mode 1)
    // Function Group 5: Right 4 bits (0x0F mask) indicate which functions can be switched (mode 0), or which were switched (mode 1)
    // 399:392
    uint8_t function_sel_6_5;
    // Function Group 4: Left 4 bits (0xF0 mask) indicate which functions can be switched (mode 0), or which were switched (mode 1)
    // Function Group 3: Right 4 bits (0x0F mask) indicate which functions can be switched (mode 0), or which were switched (mode 1)
    // 391:384
    uint8_t function_sel_4_3;
    // Function Group 2: Left 4 bits (0xF0 mask) indicate which functions can be switched (mode 0), or which were switched (mode 1)
    // Function Group 1: Right 4 bits (0x0F mask) indicate which functions can be switched (mode 0), or which were switched (mode 1)
    // 383:376
    uint8_t function_sel_2_1;
    // If 0x00: Only bits 511:376 are valid
    // If 0x01: Bits 511:272 are valid
    // 0x02 to 0xFF are reserved
    // 375:368
    uint8_t data_version;
    // If bit #i is set (right to left), then this function is currently busy
    // 367:352
    uint16_t function_group_6_busy_bits;
    // If bit #i is set (right to left), then this function is currently busy
    // 351:336
    uint16_t function_group_5_busy_bits;
    // If bit #i is set (right to left), then this function is currently busy
    // 335:320
    uint16_t function_group_4_busy_bits;
    // If bit #i is set (right to left), then this function is currently busy
    // 319:304
    uint16_t function_group_3_busy_bits;
    // If bit #i is set (right to left), then this function is currently busy
    // 303:288
    uint16_t function_group_2_busy_bits;
    // If bit #i is set (right to left), then this function is currently busy
    // 287:272
    uint16_t function_group_1_busy_bits;
    // Supposed to all be zero
    // 271:0
    uint8_t reserved[34];
};
#endif

#define checkReturnOk(call) ((g_sdio_error = (call)) == SDIO_OK ? true : logSDError(__LINE__))
static bool logSDError(int line)
{
    g_sdio_error_line = line;
    dbgmsg("SDIO SD card error on line ", line, ", error code ", (int)g_sdio_error);
    return false;
}

// Callback used by SCSI code for simultaneous processing
static sd_callback_t m_stream_callback;
static const uint8_t *m_stream_buffer;
static uint32_t m_stream_count;
static uint32_t m_stream_count_start;

void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
{
    m_stream_callback = func;
    m_stream_buffer = buffer;
    m_stream_count = 0;
    m_stream_count_start = 0;
}

static sd_callback_t get_stream_callback(const uint8_t *buf, uint32_t count, const char *accesstype, uint32_t sector)
{
    m_stream_count_start = m_stream_count;

    if (m_stream_callback)
    {
        if (buf == m_stream_buffer + m_stream_count)
        {
            m_stream_count += count;
            return m_stream_callback;
        }
        else
        {
            dbgmsg("SD card ", accesstype, "(", (int)sector,
                  ") slow transfer, buffer", (uint32_t)buf, " vs. ", (uint32_t)(m_stream_buffer + m_stream_count));
            return NULL;
        }
    }
    
    return NULL;
}

#ifdef ULTRA_SDIO
bool three_block_check(SdioCard* sd_card, uint8_t* data_buffer) {
    // There's no need to clog up the logs with errors when doing SD card autoconfig
    bool previous_log_debug = g_log_debug;
    g_log_debug = false;
    bool operation_success = false;
    
    // Perform some block read commands, to determine if the card is stable
    // 0, 0x1000, 0x10000
    operation_success = sd_card->readSectors(0x0, data_buffer, 8);
    if (operation_success) {
        operation_success = sd_card->readSectors(0x100, data_buffer, 8);
        if (operation_success) {
            operation_success = sd_card->readSectors(0x10000, data_buffer, 8);
            // Card appears stable with this feature-set
            return true;
        }
    }
    // Restore debug setting
    g_log_debug = previous_log_debug;
    return false;
}


// SD Card Communication Autoconfig 
uint8_t sd_comms_autoconfig(void* sdio_card_ptr, uint8_t* data_buffer) {
    SdioCard* sdio_card = (SdioCard*) sdio_card_ptr;
    bool operation_success = false;
    g_record_sdio_errors = false;
    // g_log_debug = true;
    uint8_t result = 0b0;

    // The base 3.3v settings, which really should work in most cases
    if (sdio_card->begin(SdioConfig(DMA_SDIO))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_STANDARD_MODE;
        }
    }
    // High speed mode
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    //sdio_config.setOptions(DMA_SDIO | SDIO_HS);
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_HS))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_HIGH_SPEED;
        }
    }
    // Ultra high speed mode
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    //sdio_config.setOptions(DMA_SDIO | SDIO_US);
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_US))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_ULTRA_SPEED;
        }
    }
    // 1.8v
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_1_8))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_LOW_VOLTAGE;
        }
    }
    // 1.8v and high speed mode
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_1_8 | SDIO_HS))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_LV_HIGH_SPEED;
        }
    }
    // 1.8v and ultra speed mode
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_1_8 | SDIO_US))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_LV_ULTRA_SPEED;
        }
    }
    // 1.8 and high speed, power mode d
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_1_8 | SDIO_HS | SDIO_M_D | (g_log_debug ? SDIO_FIN : 0)))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_LV_HS_MODE_D;
        }
    }
    // 1.8 and ultra high speed, power mode d
    operation_success = platform_power_cycle_sd_card();
    if (!operation_success) {
        logmsg("Failed to power cycle the SD Card");
    }
    if (sdio_card->begin(SdioConfig(DMA_SDIO | SDIO_1_8 | SDIO_US | SDIO_M_D | (g_log_debug ? SDIO_FIN : 0)))) {
        operation_success = three_block_check(sdio_card, data_buffer);
        if (operation_success) {
            result |= SDIO_AC_LV_US_MODE_D;
        }
    }
    // Reset the SD card one final time to allow for final configuration
    operation_success = platform_power_cycle_sd_card();

    g_record_sdio_errors = true;

    return result;
}
#endif

bool SdioCard::begin(SdioConfig sdioConfig)
{
    uint32_t reply;
    sdio_status_t status;
    // Notably 0b1 is always set

#ifdef ULTRA_SDIO
    bool high_speed =       sdioConfig.options() & SDIO_HS;
    bool ultra_high_speed = sdioConfig.options() & SDIO_US;
    bool use_1_8v =         sdioConfig.options() & SDIO_1_8;
    bool power_mode_d =     sdioConfig.options() & SDIO_M_D;
    bool log_cmd6_info =    sdioConfig.options() & SDIO_LOG;
    bool final_mode =       sdioConfig.options() & SDIO_FIN;
    // Combined flag for high speed wave and sending cmd6
    // Because using high speed wave without switching
    // the card into high speed mode makes no sense
    bool send_cmd6 = high_speed || ultra_high_speed;

    cmd6_response_t cmd6Data_inquiry;
    cmd6_response_t cmd6Data_switchMode;
    memset(&cmd6Data_inquiry, 0, sizeof(cmd6Data_inquiry));
    uint8_t* cmd6_pointer_inquiry = (uint8_t*) &cmd6Data_inquiry;

    memset(&cmd6Data_switchMode, 0, sizeof(cmd6Data_switchMode));
    uint8_t* cmd6_pointer_set = (uint8_t*) &cmd6Data_switchMode;
#endif
    bool success;
    bool step_success;
    uint8_t loop_counter = 0;

    // First time through, 3.3v mode is already in place and the card is on
    do {
        loop_counter++;  // Lowest value here is 1, not 0
        success = false;
        step_success = true;
#ifdef ULTRA_SDIO
        if (loop_counter > 1) {
            // This is a retry
            // Set power mode 3.3v
            platform_switch_SD_3_3v();
            // Power cycle the SD card
            for (int j = 0; j < 3; j++) {
                if(platform_power_cycle_sd_card()) {
                    break;
                }
                logmsg("Failed to power cycle the SD Card");
                continue;
            }
            
        }
#endif
        if (loop_counter > 2) {
            break;  // Too many retries
        }
#ifdef ULTRA_SDIO
        //        pin           function       pup    pdown  out   state  fast
        gpio_conf(SDIO_DAT_DIR, GPIO_FUNC_SIO, false, false, true, true, true);
        gpio_conf(SDIO_CMD_DIR, GPIO_FUNC_SIO, false, false, true, true, true);
        gpio_conf(SDIO_CLK,     GPIO_FUNC_SIO, false, false, true, false, true);
        gpio_conf(SDIO_CMD,     GPIO_FUNC_SIO, false, false, true, false, true);
        gpio_conf(SDIO_D0,      GPIO_FUNC_SIO, false, false, true, true, true);
        gpio_conf(SDIO_D1,      GPIO_FUNC_SIO, false, false, true, true, true);
        gpio_conf(SDIO_D2,      GPIO_FUNC_SIO, false, false, true, true, true);
        gpio_conf(SDIO_D3,      GPIO_FUNC_SIO, false, false, true, true, true);
        busy_wait_us_32(1000);
        // logmsg("GPIOI:",sio_hw->gpio_in, " GPIIH:",sio_hw->gpio_hi_in);
        // logmsg("GPIOL:",sio_hw->gpio_out, " GPIOH:",sio_hw->gpio_hi_out);
        // logmsg("GPIOE:",sio_hw->gpio_oe, " GPOEH:",sio_hw->gpio_hi_oe);
#endif
        
        // Initialize at < 1MHz clock speed, should be 100kHz to 400kHz
        rp2040_sdio_init(g_bluescsi_timings->sdio.clk_div_1mhz, 0);  // Always standard waveform to start
        current_sdio_clock_divisor = g_bluescsi_timings->sdio.clk_div_1mhz;
        current_sdio_speed_mode = 0;

        // Establish initial connection with the card
        for (int retries = 0; retries < 5; retries++)
        {
            // After a hard fault crash, delayMicroseconds hangs
            // using busy_wait_us_32 instead
            // delayMicroseconds(1000);
            busy_wait_us_32(1000);
            reply = 0;
            rp2040_sdio_command_R1(CMD0, 0, NULL); // GO_IDLE_STATE
            status = rp2040_sdio_command_R1(CMD8, 0x1AA, &reply); // SEND_IF_COND  // 2AA if starting in 1.8v
            if (status == SDIO_OK && reply == 0x1AA)
            {
                break;
            }
        }

        if (reply != 0x1AA || status != SDIO_OK)
        {
            // dbgmsg("SDIO not responding to CMD8 SEND_IF_COND, status ", (int)status, " reply ", reply);
            continue;
        }

        // Send ACMD41 to begin card initialization and wait for it to complete
        uint32_t start = platform_millis();
        do {
            if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, 0, &reply)) || // APP_CMD
#ifdef ULTRA_SDIO
                !checkReturnOk(rp2040_sdio_command_R3(ACMD41, 0xD1040000, &g_sdio_ocr)))
#else
                !checkReturnOk(rp2040_sdio_command_R3(ACMD41, 0xD0040000, &g_sdio_ocr)))
#endif
                // !checkReturnOk(rp2040_sdio_command_R1(ACMD41, 0xC0100000, &g_sdio_ocr)))
            {
                step_success = false;
                break;
            }

            if ((uint32_t)(platform_millis() - start) > 1000)
            {
                logmsg("SDIO card initialization timeout");
                step_success = false;
                break;
            }
        } while (!(g_sdio_ocr & (1 << 31)));
        if (!step_success) {
            continue;
        }
#ifdef ULTRA_SDIO
        bool cmd_success = false;
        if (use_1_8v && (g_sdio_ocr & (1 << 24))) {
            // Signal Voltage Switch here
            // CMD11
            cmd_success = checkReturnOk(rp2040_sdio_command_R1(CMD11, 0, &reply));

            if (cmd_success) {
                // Stop supplying SDIO clock
                // Configure all SDIO GPIO as input and SIO (except CLK)
                //        pin           function       pup    pdown  out    state  fast
                gpio_conf(SDIO_DAT_DIR, GPIO_FUNC_SIO, false, false, true,  false, true);
                gpio_conf(SDIO_CMD_DIR, GPIO_FUNC_SIO, false, false, true,  false, true);
                gpio_conf(SDIO_CLK,     GPIO_FUNC_SIO, true,  false, true,  false, false);
                gpio_conf(SDIO_CMD,     GPIO_FUNC_SIO, true,  false, false, false, false);
                gpio_conf(SDIO_D0,      GPIO_FUNC_SIO, true,  false, false, false, false);
                gpio_conf(SDIO_D1,      GPIO_FUNC_SIO, true,  false, false, false, false);
                gpio_conf(SDIO_D2,      GPIO_FUNC_SIO, true,  false, false, false, false);
                gpio_conf(SDIO_D3,      GPIO_FUNC_SIO, true,  false, false, false, false);

                // Check expected signal levels
                uint32_t gpio_state;
                for (int i = 0; i < 5; i++) {
#ifdef BLUESCSI_ULTRA_WIDE
                    gpio_state = sio_hw->gpio_in & 0x7C00000;
#else
                    gpio_state = sio_hw->gpio_in & 0x3E000;
#endif
                    if (gpio_state) {
                        platform_delay_ms(1);
                        cmd_success = false;
                    } else {
                        cmd_success = true;
                        break;
                    }
                }
                if (!cmd_success && g_record_sdio_errors) {
                    logmsg("1.8v Switch: GPIO State Check Fail");
                    continue;
                }

                cmd_success = platform_switch_SD_1_8v();

                if (cmd_success) {  // Switched power to 1.8v
                    // It takes a minimum of 5ms for the card to switch to 1.8v
                    // Wait at least that long, and a little longer is fine
                    platform_delay_ms(6);

                    bool cmd_detected;
                    bool dat_detected;
                    bool transition_complete = false;
                    absolute_time_t wait_until = delayed_by_ms(get_absolute_time(), 3);
                    uint32_t gpio_state_2;
                    do {
                        cmd_detected = false;
                        dat_detected = false;
                        gpio_put(SDIO_CLK, true);
                        // delayMicroseconds(10);
#ifdef BLUESCSI_ULTRA_WIDE
                        gpio_state = sio_hw->gpio_in & 0x400000;
                        gpio_state_2 = sio_hw->gpio_in & 0x7800000;
#else
                        gpio_state = sio_hw->gpio_in & 0x2000;
                        gpio_state_2 = sio_hw->gpio_in & 0x3C000;
#endif
                        // CMD should immediately be pulled high by the card
                        if (gpio_state) {
                            cmd_detected = true;
                        }
                        // Also detect whether the DAT lines are pulled high
                        if (gpio_state_2) {
                            dat_detected = true;
                        }
#ifdef BLUESCSI_ULTRA_WIDE
                        if ((dat_detected && cmd_detected) && (sio_hw->gpio_in & 0x7C00000)) {
#else
                        if ((dat_detected && cmd_detected) && (sio_hw->gpio_in & 0x3E000)) {
#endif
                            transition_complete = true;
                            break;
                        }
                        gpio_put(SDIO_CLK, false);
                        // delayMicroseconds(10);
                    } while (get_absolute_time() < wait_until);
                    if (! (dat_detected && transition_complete)) {
                        if (g_record_sdio_errors) {
                            logmsg("1.8v transition FAIL, ", cmd_detected, " , ", dat_detected, " , ", transition_complete);
                        }
                        continue;
                    }

                    // Re-configure PIO for SDIO and move on
                    rp2040_sdio_init(g_bluescsi_timings->sdio.clk_div_1mhz, 0);  // Not yet switched to high speed
                    current_sdio_clock_divisor = g_bluescsi_timings->sdio.clk_div_1mhz;
                    current_sdio_speed_mode = 0;
                } else {
                    if (g_record_sdio_errors) {
                        logmsg("8574 1.8v write failed");
                    }
                    continue;
                }
            } else {
                // Halt and catch fire?
                // Have to restart the voltage switch attempt
                if (g_record_sdio_errors) {
                    logmsg("CMD11 Failed");
                }
                continue;
            }
        
        } else if (use_1_8v) {
            if (final_mode) {
                logmsg("Final SDIO Mode Set Failure, OCR:", (uint32_t)g_sdio_ocr);
            }
            // 1.8v requested by config but unable or not supported
            return false;
        }
#endif

        // Get CID
        if (!checkReturnOk(rp2040_sdio_command_R2(CMD2, 0, (uint8_t*)&g_sdio_cid)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to read CID");
            }
            continue;
        }

        // Get relative card address
        if (!checkReturnOk(rp2040_sdio_command_R1(CMD3, 0, &g_sdio_rca)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to get RCA");
            }
            continue;
        }

        // Get CSD
        if (!checkReturnOk(rp2040_sdio_command_R2(CMD9, g_sdio_rca, (uint8_t*)&g_sdio_csd)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to read CSD");
            }
            continue;
        }

        g_sdio_sector_count = sectorCount();

        // Select card
        if (!checkReturnOk(rp2040_sdio_command_R1(CMD7, g_sdio_rca, &reply)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to select card");
            }
            continue;
        }

        // Set 4-bit bus mode
        if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) ||
            !checkReturnOk(rp2040_sdio_command_R1(ACMD6, 2, &reply)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to set bus width");
            }
            continue;
        }
#ifdef ULTRA_SDIO
        if (!checkReturnOk(rp2040_sdio_command_R1(CMD6, 0x00FFFFFF, &reply)) ||
            !checkReturnOk(receive_status_register(cmd6_pointer_inquiry)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to get SD CMD6 Inq Response");
            }
            continue;
        }

        uint32_t cmd6_set_arg = 0x80FFF0F1;
        if (ultra_high_speed) {
            cmd6_set_arg = 0x80FF30F2;
        }
        if (power_mode_d) {
            cmd6_set_arg |= 0x300;
        } else {
            cmd6_set_arg |= 0x200;
        }

        if (send_cmd6) {
            if (!checkReturnOk(rp2040_sdio_command_R1(CMD6, cmd6_set_arg, &reply)) ||
                !checkReturnOk(receive_status_register(cmd6_pointer_set)))
            {
                if (g_record_sdio_errors) {
                    logmsg("SDIO failed to get SD CMD6 Set Response");
                }
                continue;
            }
            current_sdio_speed_mode = ultra_high_speed ? 2 : 1;
            rp2040_sdio_init(g_bluescsi_timings->sdio.clk_div_1mhz, current_sdio_speed_mode);
            current_sdio_clock_divisor = g_bluescsi_timings->sdio.clk_div_1mhz;
        }
#endif

        // Read SD Status field
        memset(&g_sdio_sds, 0, sizeof(sds_t));
        uint8_t* stat_pointer = (uint8_t*) &g_sdio_sds;
        if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) ||
            !checkReturnOk(rp2040_sdio_command_R1(ACMD13, 0, &reply)) ||
            !checkReturnOk(receive_status_register(stat_pointer)))
        {
            if (g_record_sdio_errors) {
                logmsg("SDIO failed to get SD Status, ", (uint8_t)sdioConfig.options());
            }
            continue;
        }

#ifdef SDIO_DEBUG
        if (ultra_high_speed) {
            // Debug indicate
            sio_hw->gpio_hi_set = 0b01000000;
            asm volatile ("nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \nnop \n nop \n nop \n nop \n nop \n nop");
            sio_hw->gpio_hi_clr = 0b01000000;
        }
#endif
        // Increase to standard clock rate
        rp2040_sdio_init(1, current_sdio_speed_mode);
        current_sdio_clock_divisor = 1;

        success = true;
    } while (success == false);
#ifdef ULTRA_SDIO
    if (log_cmd6_info) {
        // CMD6 inquiry result
        logmsg("----- CMD6 Inquiry Result, config: ", sdioConfig.options());
        // for (int i = 0; i < 64; i++) {
        //     logmsg("CMD6r: ", i, " ", (511-(i*8)),":", ((511-(i*8)) - 7), ": ", cmd6_pointer_inquiry[i]);
        // }
        logmsg("CMD6 Response Version:", cmd6Data_inquiry.data_version);
        logmsg("TotalPwr (mA):", ((cmd6Data_inquiry.max_current & 0xFF00) >> 8) | ((cmd6Data_inquiry.max_current & 0x00FF) << 8));
        // logmsg("Access Modes Selected:", (uint8_t)(cmd6Data_inquiry.function_sel_2_1 & 0x0F));
        logmsg("Access Modes Available:", (uint32_t)(((cmd6Data_inquiry.function_group_1 & 0xFF00) >> 8) | ((cmd6Data_inquiry.function_group_1 & 0x00FF) << 8)));
        // logmsg("Command System Selected:", ((cmd6Data_inquiry.function_sel_2_1 & 0xF0) >> 4));
        logmsg("Command System Available:", (uint32_t)(((cmd6Data_inquiry.function_group_2 & 0xFF00) >> 8) | ((cmd6Data_inquiry.function_group_2 & 0x00FF) << 8)));
        // logmsg("Driver Strength Selected:", (cmd6Data_inquiry.function_sel_4_3 & 0x0F));
        logmsg("Driver Strength Available:", (uint32_t)(((cmd6Data_inquiry.function_group_3 & 0xFF00) >> 8) | ((cmd6Data_inquiry.function_group_3 & 0x00FF) << 8)));
        // logmsg("Power Limit Selected:", ((cmd6Data_inquiry.function_sel_4_3 & 0xF0) >> 4));
        logmsg("Power Limit Available:", (uint32_t)(((cmd6Data_inquiry.function_group_4 & 0xFF00) >> 8) | ((cmd6Data_inquiry.function_group_4 & 0x00FF) << 8)));

        // CMD6 Set Result
        logmsg("----- CMD6 Set Result");
        // for (int i = 0; i < 64; i++) {
        //     logmsg("CMD6r: ", i, " ", (511-(i*8)),":", ((511-(i*8)) - 7), ": ", cmd6_pointer_set[i]);
        // }
        logmsg("CMD6 Response Version:", cmd6Data_switchMode.data_version);
        logmsg("TotalPwr (mA):", ((cmd6Data_switchMode.max_current & 0xFF00) >> 8) | ((cmd6Data_switchMode.max_current & 0x00FF) << 8));
        logmsg("Access Modes Selected:", (uint8_t)(cmd6Data_switchMode.function_sel_2_1 & 0x0F));
        logmsg("Access Modes Available:", (uint32_t)(((cmd6Data_switchMode.function_group_1 & 0xFF00) >> 8) | ((cmd6Data_switchMode.function_group_1 & 0x00FF) << 8)));
        // logmsg("Command System Selected:", ((cmd6Data_switchMode.function_sel_2_1 & 0xF0) >> 4));
        // logmsg("Command System Available:", (cmd6Data_switchMode.function_group_2));
        logmsg("Driver Strength Selected:", (uint32_t)(cmd6Data_switchMode.function_sel_4_3 & 0x0F));
        logmsg("Driver Strength Available:", (uint32_t)(((cmd6Data_switchMode.function_group_3 & 0xFF00) >> 8) | ((cmd6Data_switchMode.function_group_3 & 0x00FF) << 8)));
        logmsg("Power Limit Selected:", (uint32_t)((cmd6Data_switchMode.function_sel_4_3 & 0xF0) >> 4));
        logmsg("Power Limit Available:", (uint32_t)(((cmd6Data_switchMode.function_group_4 & 0xFF00) >> 8) | ((cmd6Data_switchMode.function_group_4 & 0x00FF) << 8)));
    }
#endif
    return success;
}

uint8_t SdioCard::errorCode() const
{
    return g_sdio_error;
}

uint32_t SdioCard::errorData() const    
{
    return 0;
}

uint32_t SdioCard::errorLine() const
{
    return g_sdio_error_line;
}

bool SdioCard::isBusy() 
{
#if SDIO_D0 > 31
    return 0 == (sio_hw->gpio_hi_in & (1 << (SDIO_D0 - 32)));
#else
    return 0 == (sio_hw->gpio_in & (1 << SDIO_D0));
#endif
}

uint32_t SdioCard::kHzSdClk()
{
    return 0;
}

bool SdioCard::readCID(cid_t* cid)
{
    *cid = g_sdio_cid;
    return true;
}

bool SdioCard::readCSD(csd_t* csd)
{
    *csd = g_sdio_csd;
    return true;
}

bool SdioCard::readSDS(sds_t* sds)
{
    *sds = g_sdio_sds;
    return true;
}

bool SdioCard::readOCR(uint32_t* ocr)
{
    // SDIO mode does not have CMD58, but main program uses this to
    // poll for card presence. Return status register instead.
    return checkReturnOk(rp2040_sdio_command_R1(CMD13, g_sdio_rca, ocr));
}

bool SdioCard::readData(uint8_t* dst)
{
    logmsg("readData() not implemented");
    return false;
}

bool SdioCard::readStart(uint32_t sector)
{
    logmsg("readStart() not implemented");
    return false;
}

bool SdioCard::readStop()
{
    logmsg("readStop() not implemented");
    return false;
}

uint32_t SdioCard::sectorCount()
{
    return g_sdio_csd.capacity();
}

uint32_t SdioCard::status()
{
    uint32_t reply;
    if (checkReturnOk(rp2040_sdio_command_R1(CMD13, g_sdio_rca, &reply)))
        return reply;
    else
        return 0;
}

bool SdioCard::stopTransmission(bool blocking)
{
    uint32_t reply;

#ifdef ULTRA_SDIO
    // Set data pins output and F
    gpio_set_function(SDIO_DAT_DIR, GPIO_FUNC_SIO);
    sio_hw->gpio_set = (1 << SDIO_DAT_DIR | 1 << SDIO_D0 | 1 << SDIO_D1 | 1 << SDIO_D2 | 1 << SDIO_D3);
    sio_hw->gpio_oe_set = (1 << SDIO_DAT_DIR | 1 << SDIO_D0 | 1 << SDIO_D1 | 1 << SDIO_D2 | 1 << SDIO_D3);
#endif
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD12, 0, &reply)))
    {
        return false;
    }

#ifdef SDIO_DEBUG
    // Debug indicate
    sio_hw->gpio_hi_set = 0b01000000;
    asm volatile ("nop \n nop");
    sio_hw->gpio_hi_clr = 0b01000000;
#endif

    if (!blocking)
    {
        return true;
    }
    else
    {
#ifdef ULTRA_SDIO
        // Set data pins to input mode
        sio_hw->gpio_oe_clr = (1 << SDIO_D0 | 1 << SDIO_D1 | 1 << SDIO_D2 | 1 << SDIO_D3);
        // Set the data direction to input
        sio_hw->gpio_clr = (1 << SDIO_DAT_DIR);
        sio_hw->gpio_oe_set = (1 << SDIO_DAT_DIR);
        gpio_set_function(SDIO_DAT_DIR, GPIO_FUNC_SIO);
        gpio_set_function(SDIO_CLK, GPIO_FUNC_SIO);
#endif

        uint32_t start = platform_millis();
        while ((uint32_t)(platform_millis() - start) < 5000 && isBusy())
        {
            cycleSdClock();
            if (m_stream_callback)
            {
                m_stream_callback(m_stream_count);
            }
        }
        if (isBusy())  // 0 == SDIO_D0 pin?
        {
            // DAT pins need to be in input mode after CMD SM completes
            if (g_record_sdio_errors) {
                logmsg("SdioCard::stopTransmission() timeout");
#ifdef ULTRA_SDIO
                logmsg("M: ", (int)current_sdio_speed_mode, ", DIV: ", current_sdio_clock_divisor);
                logmsg("GPIO: ", (uint32_t)sio_hw->gpio_in, ", OE: ", (uint32_t)sio_hw->gpio_oe, ", S: ", (uint32_t)sio_hw->gpio_out);
#endif
            }
#ifdef ULTRA_SDIO
            gpio_set_function(SDIO_DAT_DIR, GPIO_FUNC_PIO1);
            gpio_set_function(SDIO_CLK, GPIO_FUNC_PIO1);
#endif
            return false;
        }
        else
        {
#ifdef ULTRA_SDIO
            gpio_set_function(SDIO_DAT_DIR, GPIO_FUNC_PIO1);
            gpio_set_function(SDIO_CLK, GPIO_FUNC_PIO1);
#endif
            return true;
        }
    }
}

bool SdioCard::syncDevice()
{
    return true;
}

uint8_t SdioCard::type() const
{
    if (g_sdio_ocr & (1 << 30))
        return SD_CARD_TYPE_SDHC;
    else
        return SD_CARD_TYPE_SD2;
}

bool SdioCard::writeData(const uint8_t* src)
{
    logmsg("writeData() not implemented");
    return false;
}

bool SdioCard::writeStart(uint32_t sector)
{
    logmsg("writeStart() not implemented");
    return false;
}

bool SdioCard::writeStop()
{
    logmsg("writeStop() not implemented");
    return false;
}

bool SdioCard::erase(uint32_t firstSector, uint32_t lastSector)
{
    logmsg("erase() not implemented");
    return false;
}

bool SdioCard::cardCMD6(uint32_t arg, uint8_t* status) {
    logmsg("cardCMD6() not implemented");
    return false;
}

bool SdioCard::readSCR(scr_t* scr) {
    logmsg("readSCR() not implemented");
    return false;
}

/* Writing and reading, with progress callback */

bool SdioCard::writeSector(uint32_t sector, const uint8_t* src)
{
    if (((uint32_t)src & 3) != 0)
    {
        // Buffer is not aligned, need to memcpy() the data to a temporary buffer.
        memcpy(g_sdio_dma_buf, src, sizeof(g_sdio_dma_buf));
        src = (uint8_t*)g_sdio_dma_buf;
    }

    // If possible, report transfer status to application through callback.
    sd_callback_t callback = get_stream_callback(src, 512, "writeSector", sector);

    // Cards up to 2GB use byte addressing, SDHC cards use sector addressing
    uint32_t address = (type() == SD_CARD_TYPE_SDHC) ? sector : (sector * 512);

    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_command_R1(CMD24, address, &reply)) || // WRITE_BLOCK
        !checkReturnOk(rp2040_sdio_tx_start(src, 1))) // Start transmission
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_tx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        logmsg("SdioCard::writeSector(", sector, ") failed: ", (int)g_sdio_error);
    }

    return g_sdio_error == SDIO_OK;
}

bool SdioCard::writeSectors(uint32_t sector, const uint8_t* src, size_t n)
{
    if (((uint32_t)src & 3) != 0)
    {
        // Unaligned write, execute sector-by-sector
        for (size_t i = 0; i < n; i++)
        {
            if (!writeSector(sector + i, src + 512 * i))
            {
                return false;
            }
        }
        return true;
    }

    sd_callback_t callback = get_stream_callback(src, n * 512, "writeSectors", sector);

    // Cards up to 2GB use byte addressing, SDHC cards use sector addressing
    uint32_t address = (type() == SD_CARD_TYPE_SDHC) ? sector : (sector * 512);

    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) || // APP_CMD
        !checkReturnOk(rp2040_sdio_command_R1(ACMD23, n, &reply)) || // SET_WR_CLK_ERASE_COUNT
        !checkReturnOk(rp2040_sdio_command_R1(CMD25, address, &reply)) || // WRITE_MULTIPLE_BLOCK
        !checkReturnOk(rp2040_sdio_tx_start(src, n))) // Start transmission
    {
        return false;
    }

#ifdef SDIO_DEBUG
        // Debug indicate
        sio_hw->gpio_hi_set = 0b10100000;
        asm volatile ("nop \n nop");
        sio_hw->gpio_hi_clr = 0b10100000;
#endif
    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_tx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        logmsg("SdioCard::writeSectors(", sector, ",...,", (int)n, ") failed: ", (int)g_sdio_error);
        stopTransmission(true);
        return false;
    }
    else
    {
        // TODO: Instead of CMD12 stopTransmission command, according to SD spec we should send stopTran token.
        // stopTransmission seems to work in practice.
        return stopTransmission(true);
    }
}

bool SdioCard::readSector(uint32_t sector, uint8_t* dst)
{
    uint8_t *real_dst = dst;
    if (((uint32_t)dst & 3) != 0)
    {
        // Buffer is not aligned, need to memcpy() the data from a temporary buffer.
        dst = (uint8_t*)g_sdio_dma_buf;
    }

    sd_callback_t callback = get_stream_callback(dst, 512, "readSector", sector);

    // Cards up to 2GB use byte addressing, SDHC cards use sector addressing
    uint32_t address = (type() == SD_CARD_TYPE_SDHC) ? sector : (sector * 512);

    uint32_t reply;
    if (
        !checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_command_R1(CMD17, address, &reply)) || // READ_SINGLE_BLOCK
        !checkReturnOk(rp2040_sdio_rx_start(dst, 1)) // Prepare for reception
        )
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_rx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_record_sdio_errors && g_sdio_error != SDIO_OK)
    {
        logmsg("readSec(", sector, ") failed: ", (int)g_sdio_error);
    }

    if (dst != real_dst)
    {
        memcpy(real_dst, g_sdio_dma_buf, sizeof(g_sdio_dma_buf));
    }

    return g_sdio_error == SDIO_OK;
}

bool SdioCard::readSectors(uint32_t sector, uint8_t* dst, size_t n)
{
    if (((uint32_t)dst & 3) != 0 || sector + n >= g_sdio_sector_count)
    {
        // Unaligned read or end-of-drive read, execute sector-by-sector
        for (size_t i = 0; i < n; i++)
        {
            if (!readSector(sector + i, dst + 512 * i))
            {
                return false;
            }
        }
        return true;
    }

    sd_callback_t callback = get_stream_callback(dst, n * 512, "readSectors", sector);

    // Cards up to 2GB use byte addressing, SDHC cards use sector addressing
    uint32_t address = (type() == SD_CARD_TYPE_SDHC) ? sector : (sector * 512);

    uint32_t reply;
    if (
        !checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_command_R1(CMD18, address, &reply)) || // READ_MULTIPLE_BLOCK
        !checkReturnOk(rp2040_sdio_rx_start(dst, n)) // Prepare for reception
        )
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_rx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        if (g_record_sdio_errors) {
#ifdef SDIO_DEBUG
            // Debug indicate
            sio_hw->gpio_hi_set = 0b11100000;
            asm volatile ("nop \n nop");
            sio_hw->gpio_hi_clr = 0b11100000;
#endif
            logmsg("SdioCard::readSectors(", sector, ",...,", (int)n, ") failed: ", (int)g_sdio_error);
        }
        stopTransmission(true);
        return false;
    }
    else
    {
        return stopTransmission(true);
    }
}

#endif

#ifdef ULTRA_SDIO
#undef ULTRA_SDIO
#endif