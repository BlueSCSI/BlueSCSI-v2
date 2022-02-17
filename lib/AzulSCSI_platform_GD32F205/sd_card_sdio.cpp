// Driver and interface for accessing SD card in SDIO mode
// Used on AzulSCSI v1.1.

#include "AzulSCSI_platform.h"

#ifdef SD_USE_SDIO

#include "AzulSCSI_log.h"
#include "gd32f20x_sdio.h"
#include "gd32f20x_dma.h"
#include "gd32_sdio_sdcard.h"
#include <SdFat.h>

static sd_error_enum g_sdio_error = SD_OK;
static int g_sdio_error_line = 0;
static sd_card_info_struct g_sdio_card_info;
static uint32_t g_sdio_card_status;
static uint32_t g_sdio_clk_kHz;

#define checkReturnOk(call) ((g_sdio_error = (call)) == SD_OK ? true : logSDError(__LINE__))
static bool logSDError(int line)
{
    g_sdio_error_line = line;
    azlog("SDIO SD card error on line ", line, ", error code ", (int)g_sdio_error);
    return false;
}

bool SdioCard::begin(SdioConfig sdioConfig)
{
    return checkReturnOk(sd_init())
        && checkReturnOk(sd_card_information_get(&g_sdio_card_info))
        && checkReturnOk(sd_card_select_deselect(g_sdio_card_info.card_rca))
        && checkReturnOk(sd_cardstatus_get(&g_sdio_card_status))
        && checkReturnOk(sd_bus_mode_config(SDIO_BUSMODE_4BIT))
        && checkReturnOk(sd_transfer_mode_config(SD_DMA_MODE));
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
    memcpy(cid, &g_sdio_card_info.card_cid, sizeof(cid_t));
    return true;
}

bool SdioCard::readCSD(csd_t* csd)
{
    memcpy(csd, &g_sdio_card_info.card_csd, sizeof(csd_t));
    return true;
}

bool SdioCard::readOCR(uint32_t* ocr)
{
    return checkReturnOk(sd_card_read_ocr(ocr));
}

bool SdioCard::readData(uint8_t* dst)
{
    azlog("SdioCard::readData() called but not implemented!");
    return false;
}

bool SdioCard::readSector(uint32_t sector, uint8_t* dst)
{
    return checkReturnOk(sd_block_read((uint32_t*)dst, sector, 512));
}

bool SdioCard::readSectors(uint32_t sector, uint8_t* dst, size_t n)
{
    return checkReturnOk(sd_multiblocks_read((uint32_t*)dst, sector, 512, n));
}

bool SdioCard::readStart(uint32_t sector)
{
    azlog("SdioCard::readStart() called but not implemented!");
    return false;
}

bool SdioCard::readStop()
{
    azlog("SdioCard::readStop() called but not implemented!");
    return false;
}

uint32_t SdioCard::sectorCount()
{
    return sdCardCapacity((csd_t*)&g_sdio_card_info.card_csd);
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
            azlog("SdioCard::stopTransmission() timeout");
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
    return stopTransmission(true);
}

uint8_t SdioCard::type() const
{
    if (g_sdio_card_info.card_type == SDIO_HIGH_CAPACITY_SD_CARD)
        return SD_CARD_TYPE_SDHC;
    else if (g_sdio_card_info.card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0)
        return SD_CARD_TYPE_SD2;
    else
        return SD_CARD_TYPE_SD1;
}

bool SdioCard::writeData(const uint8_t* src)
{
    azlog("SdioCard::writeData() called but not implemented!");
    return false;
}

bool SdioCard::writeSector(uint32_t sector, const uint8_t* src)
{
    return checkReturnOk(sd_block_write((uint32_t*)src, sector, 512));
}

bool SdioCard::writeSectors(uint32_t sector, const uint8_t* src, size_t n)
{
    return checkReturnOk(sd_multiblocks_write((uint32_t*)src, sector, 512, n));
}

bool SdioCard::writeStart(uint32_t sector)
{
    azlog("SdioCard::writeStart() called but not implemented!");
    return false;
}

bool SdioCard::writeStop()
{
    azlog("SdioCard::writeStop() called but not implemented!");
    return false;
}

bool SdioCard::erase(uint32_t firstSector, uint32_t lastSector)
{
    return checkReturnOk(sd_erase(firstSector, lastSector));
}


SdioConfig g_sd_sdio_config(DMA_SDIO);

void azplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
{
}

// These functions are not used for SDIO mode but are needed to avoid build error.
void sdCsInit(SdCsPin_t pin) {}
void sdCsWrite(SdCsPin_t pin, bool level) {}

#endif