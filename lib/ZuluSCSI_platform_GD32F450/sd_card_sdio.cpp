/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
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

// Driver and interface for accessing SD card in SDIO mode
// Used on ZuluSCSI v1.4.

#include "ZuluSCSI_platform.h"

#ifdef SD_USE_SDIO
extern "C"
{
#include "gd32f4xx_sdio.h"
#include "gd32f4xx_dma.h"
#include "gd32_sdio_sdcard.h"
}
#include "ZuluSCSI_log.h"
#include <SdFat.h>

static sd_error_enum g_sdio_error = SD_OK;
static int g_sdio_error_line = 0;
static uint32_t g_sdio_card_status;
static uint32_t g_sdio_clk_kHz;
static sdio_card_type_enum g_sdio_card_type;
static uint16_t g_sdio_card_rca;
static uint32_t g_sdio_sector_count;

#define checkReturnOk(call) ((g_sdio_error = (call)) == SD_OK ? true : logSDError(__LINE__))
static bool logSDError(int line)
{
    g_sdio_error_line = line;
    logmsg("SDIO SD card error on line ", line, ", error code ", (int)g_sdio_error);
    return false;
}

bool SdioCard::begin(SdioConfig sdioConfig)
{
    rcu_periph_clock_enable(RCU_SDIO);
    rcu_periph_clock_enable(RCU_DMA1);
    nvic_irq_enable(SDIO_IRQn, 0, 0);

    g_sdio_error = sd_init();
    if (g_sdio_error != SD_OK)
    {
        // Don't spam the log when main program polls for card insertion.
        dbgmsg("sd_init() failed: ", (int)g_sdio_error);
        return false;
    }

    return checkReturnOk(sd_card_information_get_short(&g_sdio_card_type, &g_sdio_card_rca))
        && checkReturnOk(sd_card_select_deselect(g_sdio_card_rca))
        && checkReturnOk(sd_cardstatus_get(&g_sdio_card_status))
        && checkReturnOk(sd_bus_mode_config(SDIO_BUSMODE_4BIT))
        && checkReturnOk(sd_transfer_mode_config(sdioConfig.useDma() ? SD_DMA_MODE : SD_POLLING_MODE))
        && (g_sdio_sector_count = sectorCount());
}

uint8_t SdioCard::errorCode() const
{
    if (g_sdio_error == SD_OK)
        return SD_CARD_ERROR_NONE;
    else
        return 0x80 + g_sdio_error;
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
    return (GPIO_ISTAT(SD_SDIO_DATA_PORT) & SD_SDIO_D0) == 0;
}

uint32_t SdioCard::kHzSdClk()
{
    return g_sdio_clk_kHz;
}

bool SdioCard::readCID(cid_t* cid)
{
    sd_cid_get((uint8_t*)cid);
    return true;
}

bool SdioCard::readCSD(csd_t* csd)
{
    sd_csd_get((uint8_t*)csd);
    return true;
}

bool SdioCard::readOCR(uint32_t* ocr)
{
    // SDIO mode does not have CMD58, but main program uses this to
    // poll for card presence. Return status register instead.
    return sd_cardstatus_get(ocr) == SD_OK;
}

bool SdioCard::readData(uint8_t* dst)
{
    logmsg("SdioCard::readData() called but not implemented!");
    return false;
}

bool SdioCard::readStart(uint32_t sector)
{
    logmsg("SdioCard::readStart() called but not implemented!");
    return false;
}

bool SdioCard::readStop()
{
    logmsg("SdioCard::readStop() called but not implemented!");
    return false;
}

uint32_t SdioCard::sectorCount()
{
    csd_t csd;
    sd_csd_get((uint8_t*)&csd);
    return csd.capacity();
}

uint32_t SdioCard::status()
{
    uint32_t status = 0;
    if (!checkReturnOk(sd_cardstatus_get(&status)))
        return 0;
    else
        return status;
}

bool SdioCard::stopTransmission(bool blocking)
{
    if (!checkReturnOk(sd_transfer_stop()))
        return false;

    if (!blocking)
    {
        return true;
    }
    else
    {
        uint32_t end = millis() + 100;
        while (millis() < end && isBusy())
        {
        }
        if (isBusy())
        {
            logmsg("SdioCard::stopTransmission() timeout");
            return false;
        }
        else
        {
            return true;
        }
    }
}

bool SdioCard::syncDevice()
{
    if (sd_transfer_state_get() != SD_NO_TRANSFER)
    {
        return stopTransmission(true);
    }
    return true;
}

uint8_t SdioCard::type() const
{
    if (g_sdio_card_type == SDIO_HIGH_CAPACITY_SD_CARD)
        return SD_CARD_TYPE_SDHC;
    else if (g_sdio_card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0)
        return SD_CARD_TYPE_SD2;
    else
        return SD_CARD_TYPE_SD1;
}

bool SdioCard::writeData(const uint8_t* src)
{
    logmsg("SdioCard::writeData() called but not implemented!");
    return false;
}

bool SdioCard::writeStart(uint32_t sector)
{
    logmsg("SdioCard::writeStart() called but not implemented!");
    return false;
}

bool SdioCard::writeStop()
{
    logmsg("SdioCard::writeStop() called but not implemented!");
    return false;
}

bool SdioCard::erase(uint32_t firstSector, uint32_t lastSector)
{
    return checkReturnOk(sd_erase(firstSector * 512, lastSector * 512));
}

bool SdioCard::cardCMD6(uint32_t arg, uint8_t* status) {
    logmsg("SdioCard::cardCMD6() not implemented");
    return false;
}

bool SdioCard::readSCR(scr_t* scr) {
    logmsg("SdioCard::readSCR() not implemented");
    return false;
}

/* Writing and reading, with progress callback */

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

static void sdio_callback(uint32_t complete)
{
    if (m_stream_callback)
    {
        m_stream_callback(m_stream_count_start + complete);
    }
}

static sdio_callback_t get_stream_callback(const uint8_t *buf, uint32_t count, const char *accesstype, uint32_t sector)
{
    m_stream_count_start = m_stream_count;

    if (m_stream_callback)
    {
        if (buf == m_stream_buffer + m_stream_count)
        {
            m_stream_count += count;
            return &sdio_callback;
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


bool SdioCard::writeSector(uint32_t sector, const uint8_t* src)
{
    return checkReturnOk(sd_block_write((uint32_t*)src, (uint64_t)sector * 512, 512,
        get_stream_callback(src, 512, "writeSector", sector)));
}

bool SdioCard::writeSectors(uint32_t sector, const uint8_t* src, size_t n)
{
    return checkReturnOk(sd_multiblocks_write((uint32_t*)src, (uint64_t)sector * 512, 512, n,
        get_stream_callback(src, n * 512, "writeSectors", sector)));
}

bool SdioCard::readSector(uint32_t sector, uint8_t* dst)
{
    return checkReturnOk(sd_block_read((uint32_t*)dst, (uint64_t)sector * 512, 512,
        get_stream_callback(dst, 512, "readSector", sector)));
}

bool SdioCard::readSectors(uint32_t sector, uint8_t* dst, size_t n)
{
    if (sector + n >= g_sdio_sector_count)
    {
        // sd_multiblocks_read() seems to have trouble reading the very last sector
        for (int i = 0; i < n; i++)
        {
            if (!readSector(sector + i, dst + i * 512))
            {
                logmsg("End of drive read failed at ", sector, " + ", i);
                return false;
            }
        }
        return true;
    }

    return checkReturnOk(sd_multiblocks_read((uint32_t*)dst, (uint64_t)sector * 512, 512, n,
        get_stream_callback(dst, n * 512, "readSectors", sector)));
}

// Check if a DMA request for SD card read has completed.
// This is used to optimize the timing of data transfers on SCSI bus.
bool check_sd_read_done()
{
    return (DMA_CHCTL(DMA1, DMA_CH3) & DMA_CHXCTL_CHEN)
        && (DMA_INTF0(DMA1) & DMA_FLAG_ADD(DMA_FLAG_FTF, DMA_CH3));
}

// These functions are not used for SDIO mode but are needed to avoid build error.
void sdCsInit(SdCsPin_t pin) {}
void sdCsWrite(SdCsPin_t pin, bool level) {}

// Interrupt handler for SDIO
extern "C" void SDIO_IRQHandler(void)
{
    sd_interrupts_process();
}

// SDIO configuration for main program
SdioConfig g_sd_sdio_config(DMA_SDIO);

// SDIO configuration in crash
SdioConfig g_sd_sdio_config_crash(0);

#endif