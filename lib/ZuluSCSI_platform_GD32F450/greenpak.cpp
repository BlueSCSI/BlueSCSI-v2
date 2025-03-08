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

// I2C communication with GreenPAK.
// This uses bitbanging for I2C so that internal GPIO pull-up can be used
// and to avoid the bugs that are present in STM32F2 I2C peripheral,
// it is uncertain if the same bugs apply to GD32F2.

#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include "greenpak.h"
#include "greenpak_fw.h"
#include <gd32f4xx_gpio.h>

#ifndef GREENPAK_I2C_PORT

bool greenpak_write(uint16_t regaddr, const uint8_t *data, int length) { return false; }
bool greenpak_read(uint16_t regaddr, uint8_t *data, int length) { return false; }
bool greenpak_load_firmware() { return false; }
bool greenpak_is_ready() { return false; }

#else

bool g_greenpak_is_ready;

// SCL is driven as push-pull, SDA is driven as IPU / OUT_OD
#define I2C_SCL_HI() GPIO_BOP(GREENPAK_I2C_PORT) = GREENPAK_I2C_SCL
#define I2C_SCL_LO() GPIO_BC(GREENPAK_I2C_PORT) = GREENPAK_I2C_SCL
#define I2C_SDA_HI() gpio_mode_set(GREENPAK_I2C_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GREENPAK_I2C_SDA)
#define I2C_SDA_LO() gpio_mode_set(GREENPAK_I2C_PORT, GPIO_MODE_OUTPUT, GPIO_OSPEED_2MHZ, GREENPAK_I2C_SDA), gpio_output_options_set(GREENPAK_I2C_PORT,GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GREENPAK_I2C_SDA), GPIO_BC(GREENPAK_I2C_PORT) = GREENPAK_I2C_SDA

#define I2C_SDA_READ() (GPIO_ISTAT(GREENPAK_I2C_PORT) & GREENPAK_I2C_SDA)
#define I2C_DELAY() delay_ns(10000);

static void greenpak_gpio_init()
{
    gpio_bit_set(GREENPAK_I2C_PORT, GREENPAK_I2C_SCL | GREENPAK_I2C_SDA);

    gpio_mode_set(GREENPAK_I2C_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GREENPAK_I2C_SDA);
    
    gpio_mode_set(GREENPAK_I2C_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GREENPAK_I2C_SCL);
    gpio_output_options_set(GREENPAK_I2C_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GREENPAK_I2C_SCL);

    // Data bits used for communication
    uint32_t greenpak_io = GREENPAK_PLD_IO1 | GREENPAK_PLD_IO2 | GREENPAK_PLD_IO3;
    gpio_bit_reset(SCSI_OUT_PORT, greenpak_io);
    gpio_mode_set(GREENPAK_PLD_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, greenpak_io);
    gpio_output_options_set(GREENPAK_PLD_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, greenpak_io);
    
}

static void i2c_writebit(bool bit)
{
    if (bit)
        I2C_SDA_HI();
    else
        I2C_SDA_LO();

    I2C_DELAY();
    I2C_SCL_HI();
    I2C_DELAY();
    I2C_SCL_LO();
}

static bool i2c_readbit()
{
    I2C_SDA_HI(); // Pull-up
    I2C_DELAY();
    I2C_SCL_HI();
    I2C_DELAY();
    bool result = I2C_SDA_READ();
    I2C_SCL_LO();
    return result;
}

// Write byte to I2C bus, return ACK bit status
static bool i2c_writebyte(uint8_t byte)
{
    for (int i = 0; i < 8; i++)
    {
        i2c_writebit(byte & (0x80 >> i));
    }
    return !i2c_readbit();
}

// Read byte from I2C bus
static uint8_t i2c_readbyte(bool ack)
{
    uint8_t result = 0;
    for (int i = 0; i < 8; i++)
    {
        result |= i2c_readbit() << (7 - i);
    }

    i2c_writebyte(!ack);
    return result;
}

static bool i2c_start(uint8_t device_addr)
{
    // Initial signal state
    I2C_SCL_HI();
    I2C_SDA_HI();
    I2C_DELAY();

    // Start condition
    I2C_SDA_LO();
    I2C_DELAY();
    
    I2C_SCL_LO();
    I2C_DELAY();

    // Device address
    return i2c_writebyte(device_addr);
}

static void i2c_stop()
{
    I2C_SDA_LO();
    I2C_DELAY();
    I2C_SCL_HI();
    I2C_DELAY();
    I2C_SDA_HI();
    I2C_DELAY();
}

bool greenpak_write(uint16_t regaddr, const uint8_t *data, int length)
{
    bool status = true;
    uint16_t blockaddr = regaddr >> 8;
    status &= i2c_start(GREENPAK_I2C_ADDR | (blockaddr << 1));
    status &= i2c_writebyte(regaddr & 0xFF);
    
    for (int i = 0; i < length; i++)
    {
        status &= i2c_writebyte(data[i]);
    }

    i2c_stop();
    return status;
}

bool greenpak_read(uint16_t regaddr, uint8_t *data, int length)
{
    bool status = true;
    uint16_t blockaddr = (regaddr >> 8) & 7;
    status &= i2c_start(GREENPAK_I2C_ADDR | (blockaddr << 1));
    status &= i2c_writebyte(regaddr & 0xFF);

    status &= i2c_start(GREENPAK_I2C_ADDR | (blockaddr << 1) | 1);
    
    for (int i = 0; i < length; i++)
    {
        data[i] = i2c_readbyte(i < length - 1);
    }

    i2c_stop();
    return status;
}

bool greenpak_load_firmware()
{
    uint8_t dummy;
    greenpak_gpio_init();

    if (!greenpak_read(0, &dummy, 1))
    {
        logmsg("Optional GreenPAK not detected");
        return false;
    }
    else
    {
        logmsg("Optional GreenPAK detected, loading firmware");
    }

    if (!greenpak_write(0, g_greenpak_fw, sizeof(g_greenpak_fw)))
    {
        logmsg("GreenPAK firmware loading failed");
        return false;
    }
    else
    {
        logmsg("GreenPAK firmware successfully loaded");
        LED_ON();
        delay(10);
        LED_OFF();
        delay(100);
        g_greenpak_is_ready = true;
        return true;
    }
}

bool greenpak_is_ready()
{
    return g_greenpak_is_ready;
}

#endif