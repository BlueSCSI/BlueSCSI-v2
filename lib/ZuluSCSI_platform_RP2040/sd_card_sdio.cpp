// Driver for accessing SD card in SDIO mode on RP2040.

#include "ZuluSCSI_platform.h"

#ifdef SD_USE_SDIO

#include "ZuluSCSI_log.h"
#include "rp2040_sdio.h"
#include <hardware/gpio.h>
#include <SdFat.h>
#include <SdCard/SdCardInfo.h>


bool SdioCard::begin(SdioConfig sdioConfig)
{
    uint32_t reply;
    
    rp2040_sdio_init();
    delay(1);
    rp2040_sdio_command_R1(CMD0, 0, NULL); // GO_IDLE_STATE
    rp2040_sdio_command_R1(CMD8, 0x1AA, &reply); // SEND_IF_COND
    azdbg("Reply ", reply);
    rp2040_sdio_command_R1(CMD0, 0, NULL); // GO_IDLE_STATE
    rp2040_sdio_command_R1(CMD8, 0x1AA, &reply); // SEND_IF_COND
    azdbg("Reply ", reply);

    delay(100);
    return false;
}

uint8_t SdioCard::errorCode() const
{
    return SD_CARD_ERROR_NONE;
}

uint32_t SdioCard::errorData() const
{
    return 0;
}

uint32_t SdioCard::errorLine() const
{
    return 0;
}

bool SdioCard::isBusy() 
{
    return (sio_hw->gpio_in & (1 << SDIO_D0)) == 0;
}

uint32_t SdioCard::kHzSdClk()
{
    return 0;
}

bool SdioCard::readCID(cid_t* cid)
{
    return true;
}

bool SdioCard::readCSD(csd_t* csd)
{
    return true;
}

bool SdioCard::readOCR(uint32_t* ocr)
{
    return true;
}

bool SdioCard::readData(uint8_t* dst)
{
    azlog("SdioCard::readData() called but not implemented!");
    return false;
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
    // csd_t csd;
    // sd_csd_get((uint8_t*)&csd);
    // return sdCardCapacity(&csd);
    return 0;
}

uint32_t SdioCard::status()
{
    // uint32_t status = 0;
    // if (!checkReturnOk(sd_cardstatus_get(&status)))
    //     return 0;
    // else
    //     return status;
    return 0;
}

bool SdioCard::stopTransmission(bool blocking)
{
    return false;
    // if (!checkReturnOk(sd_transfer_stop()))
    //     return false;

    // if (!blocking)
    // {
    //     return true;
    // }
    // else
    // {
    //     uint32_t end = millis() + 100;
    //     while (millis() < end && isBusy())
    //     {
    //     }
    //     if (isBusy())
    //     {
    //         azlog("SdioCard::stopTransmission() timeout");
    //         return false;
    //     }
    //     else
    //     {
    //         return true;
    //     }
    // }
}

bool SdioCard::syncDevice()
{
    // if (sd_transfer_state_get() != SD_NO_TRANSFER)
    // {
    //     return stopTransmission(true);
    // }
    return true;
}

uint8_t SdioCard::type() const
{
    // if (g_sdio_card_type == SDIO_HIGH_CAPACITY_SD_CARD)
    //     return SD_CARD_TYPE_SDHC;
    // else if (g_sdio_card_type == SDIO_STD_CAPACITY_SD_CARD_V2_0)
    //     return SD_CARD_TYPE_SD2;
    // else
    //     return SD_CARD_TYPE_SD1;
}

bool SdioCard::writeData(const uint8_t* src)
{
    azlog("SdioCard::writeData() called but not implemented!");
    return false;
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
    // return checkReturnOk(sd_erase(firstSector * 512, lastSector * 512));
}

/* Writing and reading, with progress callback */

// static sd_callback_t m_stream_callback;
// static const uint8_t *m_stream_buffer;
// static uint32_t m_stream_count;
// static uint32_t m_stream_count_start;

// void azplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
// {
//     m_stream_callback = func;
//     m_stream_buffer = buffer;
//     m_stream_count = 0;
//     m_stream_count_start = 0;
// }

// static void sdio_callback(uint32_t complete)
// {
//     if (m_stream_callback)
//     {
//         m_stream_callback(m_stream_count_start + complete);
//     }
// }

// static sdio_callback_t get_stream_callback(const uint8_t *buf, uint32_t count)
// {
//     m_stream_count_start = m_stream_count;

//     if (m_stream_callback)
//     {
//         if (buf == m_stream_buffer + m_stream_count)
//         {
//             m_stream_count += count;
//             return &sdio_callback;
//         }
//         else
//         {
//             azdbg("Stream buffer mismatch: ", (uint32_t)buf, " vs. ", (uint32_t)(m_stream_buffer + m_stream_count));
//             return NULL;
//         }
//     }
    
//     return NULL;
// }


bool SdioCard::writeSector(uint32_t sector, const uint8_t* src)
{
    // return checkReturnOk(sd_block_write((uint32_t*)src, (uint64_t)sector * 512, 512,
    //     get_stream_callback(src, 512)));
}

bool SdioCard::writeSectors(uint32_t sector, const uint8_t* src, size_t n)
{
    // return checkReturnOk(sd_multiblocks_write((uint32_t*)src, (uint64_t)sector * 512, 512, n,
    //     get_stream_callback(src, n * 512)));
}

bool SdioCard::readSector(uint32_t sector, uint8_t* dst)
{
    // return checkReturnOk(sd_block_read((uint32_t*)dst, (uint64_t)sector * 512, 512,
    //     get_stream_callback(dst, 512)));
}

bool SdioCard::readSectors(uint32_t sector, uint8_t* dst, size_t n)
{
    // return checkReturnOk(sd_multiblocks_read((uint32_t*)dst, (uint64_t)sector * 512, 512, n,
    //     get_stream_callback(dst, n * 512)));
}

// SDIO configuration for main program
SdioConfig g_sd_sdio_config(DMA_SDIO);

#endif