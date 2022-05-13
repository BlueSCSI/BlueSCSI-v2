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

        if (m_stream_callback && buf == m_stream_buffer + m_stream_count)
        {
            m_stream_count += count;
            m_stream_callback(m_stream_count);
        }

        return 0;
    }

    // Multiple byte send
    void send(const uint8_t* buf, size_t count) {
        spi_write_blocking(SD_SPI, buf, count);

        if (m_stream_callback && buf == m_stream_buffer + m_stream_count)
        {
            m_stream_count += count;
            m_stream_callback(m_stream_count);
        }
    }

    void setSckSpeed(uint32_t maxSck) {
        m_sckfreq = maxSck;
    }

    void set_sd_callback(sd_callback_t func, const uint8_t *buffer)
    {
        m_stream_buffer = buffer;
        m_stream_count = 0;
        m_stream_callback = func;
    }

private:
    uint32_t m_sckfreq;
    const uint8_t *m_stream_buffer;
    uint32_t m_stream_count;
    sd_callback_t m_stream_callback;
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

void azplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
{
    g_sd_spi_port.set_sd_callback(func, buffer);
}

#endif