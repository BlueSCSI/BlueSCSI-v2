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

// Driver and interface for accessing SD card in SPI mode

#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include <hardware/spi.h>
#include <SdFat.h>

#ifndef SD_USE_SDIO

class RP2040SPIDriver : public SdSpiBaseClass
{
public:
    void begin(SdSpiConfig config) {
    }

    void activate() {
        _spi_init(SD_SPI, m_sckfreq);
        spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }

    void deactivate() {
    }

    void wait_idle() {
        while (!(spi_get_hw(SD_SPI)->sr & SPI_SSPSR_TFE_BITS));
        while (spi_get_hw(SD_SPI)->sr & SPI_SSPSR_BSY_BITS);
    }

    // Single byte receive
    uint8_t receive() {
        uint8_t tx = 0xFF;
        uint8_t rx;
        spi_write_read_blocking(SD_SPI, &tx, &rx, 1);
        return rx;
    }

    // Single byte send
    void send(uint8_t data) {
        spi_write_blocking(SD_SPI, &data, 1);
        wait_idle();
    }

    // Multiple byte receive
    uint8_t receive(uint8_t* buf, size_t count)
    {
        spi_read_blocking(SD_SPI, 0xFF, buf, count);
        return 0;
    }

    // Multiple byte send
    void send(const uint8_t* buf, size_t count) {
        spi_write_blocking(SD_SPI, buf, count);
    }

    void setSckSpeed(uint32_t maxSck) {
        m_sckfreq = maxSck;
    }

private:
    uint32_t m_sckfreq;
};

void sdCsInit(SdCsPin_t pin)
{
}

void sdCsWrite(SdCsPin_t pin, bool level)
{
    if (level)
        sio_hw->gpio_set = (1 << SD_SPI_CS);
    else
        sio_hw->gpio_clr = (1 << SD_SPI_CS);
}

RP2040SPIDriver g_sd_spi_port;
SdSpiConfig g_sd_spi_config(0, DEDICATED_SPI, SD_SCK_MHZ(25), &g_sd_spi_port);

void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
{
}

#endif