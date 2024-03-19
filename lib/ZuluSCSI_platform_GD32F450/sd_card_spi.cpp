// Driver and interface for accessing SD card in SPI mode
// Used on ZuluSCSI v1.0.

#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include "gd32f4xx_spi.h"
#include "gd32f4xx_dma.h"
#include <SdFat.h>

#ifndef SD_USE_SDIO

class GD32SPIDriver : public SdSpiBaseClass
{
public:
    void begin(SdSpiConfig config) {
        rcu_periph_clock_enable(RCU_SPI0);
        rcu_periph_clock_enable(RCU_DMA0);

        dma_parameter_struct rx_dma_config =
        {
            .periph_addr = (uint32_t)&SPI_DATA(SD_SPI),
            .periph_width = DMA_PERIPHERAL_WIDTH_8BIT,
            .memory_addr = 0, // Set before transfer
            .memory_width = DMA_MEMORY_WIDTH_8BIT,
            .number = 0, // Set before transfer
            .priority = DMA_PRIORITY_ULTRA_HIGH,
            .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
            .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
            .direction = DMA_PERIPHERAL_TO_MEMORY
        };
        dma_init(DMA0, SD_SPI_RX_DMA_CHANNEL, &rx_dma_config);

        dma_parameter_struct tx_dma_config =
        {
            .periph_addr = (uint32_t)&SPI_DATA(SD_SPI),
            .periph_width = DMA_PERIPHERAL_WIDTH_8BIT,
            .memory_addr = 0, // Set before transfer
            .memory_width = DMA_MEMORY_WIDTH_8BIT,
            .number = 0, // Set before transfer
            .priority = DMA_PRIORITY_HIGH,
            .periph_inc = DMA_PERIPH_INCREASE_DISABLE,
            .memory_inc = DMA_MEMORY_INCREASE_ENABLE,
            .direction = DMA_MEMORY_TO_PERIPHERAL
        };
        dma_init(DMA0, SD_SPI_TX_DMA_CHANNEL, &tx_dma_config);
    }
        
    void activate() {
        spi_parameter_struct config = {
            SPI_MASTER,
            SPI_TRANSMODE_FULLDUPLEX,
            SPI_FRAMESIZE_8BIT,
            SPI_NSS_SOFT,
            SPI_ENDIAN_MSB,
            SPI_CK_PL_LOW_PH_1EDGE,
            SPI_PSC_256
        };

        // Select closest available divider based on system frequency
        int divider = (SystemCoreClock + m_sckfreq / 2) / m_sckfreq;
        if (divider <= 2)
            config.prescale = SPI_PSC_2;
        else if (divider <= 4)
            config.prescale = SPI_PSC_4;
        else if (divider <= 8)
            config.prescale = SPI_PSC_8;
        else if (divider <= 16)
            config.prescale = SPI_PSC_16;
        else if (divider <= 32)
            config.prescale = SPI_PSC_32;
        else if (divider <= 64)
            config.prescale = SPI_PSC_64;
        else if (divider <= 128)
            config.prescale = SPI_PSC_128;
        else
            config.prescale = SPI_PSC_256;

        spi_init(SD_SPI, &config);
        spi_enable(SD_SPI);
    }
    
    void deactivate() {
        spi_disable(SD_SPI);
    }

    void wait_idle() {
        while (!(SPI_STAT(SD_SPI) & SPI_STAT_TBE));
        while (SPI_STAT(SD_SPI) & SPI_STAT_TRANS);
    }

    // Single byte receive
    uint8_t receive() {
        // Wait for idle and clear RX buffer
        wait_idle();
        (void)SPI_DATA(SD_SPI);

        // Send dummy byte and wait for receive
        SPI_DATA(SD_SPI) = 0xFF;
        while (!(SPI_STAT(SD_SPI) & SPI_STAT_RBNE));
        return SPI_DATA(SD_SPI);
    }

    // Single byte send
    void send(uint8_t data) {
        SPI_DATA(SD_SPI) = data;
        wait_idle();
    }

    // Multiple byte receive
    uint8_t receive(uint8_t* buf, size_t count)
    {
        // Wait for idle and clear RX buffer
        wait_idle();
        (void)SPI_DATA(SD_SPI);

        // Check if this is part of callback streaming request
        bool stream = false;
        if (m_stream_callback && buf == m_stream_buffer + m_stream_count)
        {
            stream = true;
        }
        else if (m_stream_callback)
        {
            dbgmsg("Stream buffer mismatch: ", (uint32_t)buf, " vs. ", (uint32_t)(m_stream_buffer + m_stream_count));
        }

        // Use DMA to stream dummy TX data and store RX data
        uint8_t tx_data = 0xFF;
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL);
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_TX_DMA_CHANNEL);
        DMA_CHMADDR(DMA0, SD_SPI_RX_DMA_CHANNEL) = (uint32_t)buf;
        DMA_CHMADDR(DMA0, SD_SPI_TX_DMA_CHANNEL) = (uint32_t)&tx_data;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_MNAGA; // No memory increment for TX
        DMA_CHCNT(DMA0, SD_SPI_RX_DMA_CHANNEL) = count;
        DMA_CHCNT(DMA0, SD_SPI_TX_DMA_CHANNEL) = count;
        DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;

        SPI_CTL1(SD_SPI) |= SPI_CTL1_DMAREN | SPI_CTL1_DMATEN;
        
        uint32_t start = millis();
        while (!(DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL)))
        {
            if (millis() - start > 500)
            {
                logmsg("ERROR: SPI DMA receive of ", (int)count, " bytes timeouted");
                return 1;
            }

            if (stream)
            {
                uint32_t complete = m_stream_count + (count - DMA_CHCNT(DMA0, SD_SPI_RX_DMA_CHANNEL));
                m_stream_callback(complete);
            }
        }

        if (DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_ERR, SD_SPI_RX_DMA_CHANNEL))
        {
            logmsg("ERROR: SPI DMA receive set DMA_FLAG_ERR");
        }

        SPI_CTL1(SD_SPI) &= ~(SPI_CTL1_DMAREN | SPI_CTL1_DMATEN);
        DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;

        if (stream)
        {
            m_stream_count += count;
        }

        return 0;
    }

    // Multiple byte send
    void send(const uint8_t* buf, size_t count) {
        // Check if this is part of callback streaming request
        bool stream = false;
        if (m_stream_callback && buf == m_stream_buffer + m_stream_count)
        {
            stream = true;
        }
        else if (m_stream_callback)
        {
            dbgmsg("Stream buffer mismatch: ", (uint32_t)buf, " vs. ", (uint32_t)(m_stream_buffer + m_stream_count));
        }

        // Use DMA to stream TX data
        DMA_INTC(DMA0) = DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_TX_DMA_CHANNEL);
        DMA_CHMADDR(DMA0, SD_SPI_TX_DMA_CHANNEL) = (uint32_t)buf;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) |= DMA_CHXCTL_MNAGA; // Memory increment for TX
        DMA_CHCNT(DMA0, SD_SPI_TX_DMA_CHANNEL) = count;
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) |= DMA_CHXCTL_CHEN;

        SPI_CTL1(SD_SPI) |= SPI_CTL1_DMATEN;
        
        uint32_t start = millis();
        while (!(DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_FTF | DMA_FLAG_ERR, SD_SPI_TX_DMA_CHANNEL)))
        {
            if (millis() - start > 500)
            {
                logmsg("ERROR: SPI DMA transmit of ", (int)count, " bytes timeouted");
                return;
            }

            if (stream)
            {
                uint32_t complete = m_stream_count + (count - DMA_CHCNT(DMA0, SD_SPI_TX_DMA_CHANNEL));
                m_stream_callback(complete);
            }
        }

        if (DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_ERR, SD_SPI_TX_DMA_CHANNEL))
        {
            logmsg("ERROR: SPI DMA transmit set DMA_FLAG_ERR");
        }

        wait_idle();

        SPI_CTL1(SD_SPI) &= ~(SPI_CTL1_DMAREN | SPI_CTL1_DMATEN);
        DMA_CHCTL(DMA0, SD_SPI_TX_DMA_CHANNEL) &= ~DMA_CHXCTL_CHEN;

        if (stream)
        {
            m_stream_count += count;
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
        GPIO_BOP(SD_PORT) = SD_CS_PIN;
    else
        GPIO_BC(SD_PORT) = SD_CS_PIN;
}

GD32SPIDriver g_sd_spi_port;
SdSpiConfig g_sd_spi_config(0, DEDICATED_SPI, SD_SCK_MHZ(30), &g_sd_spi_port);

void platform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
{
    g_sd_spi_port.set_sd_callback(func, buffer);    
}

// Check if a DMA request for SD card read has completed.
// This is used to optimize the timing of data transfers on SCSI bus.
bool check_sd_read_done()
{
    return (DMA_CHCTL(DMA0, SD_SPI_RX_DMA_CHANNEL) & DMA_CHXCTL_CHEN)
        && (DMA_INTF(DMA0) & DMA_FLAG_ADD(DMA_FLAG_FTF, SD_SPI_RX_DMA_CHANNEL));
}

#endif