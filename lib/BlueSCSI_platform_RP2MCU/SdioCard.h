/**
 * SdioCard class declaration for BlueSCSI RP2040/RP2350 SDIO driver.
 *
 * This header provides the SdioConfig and SdioCard class declarations
 * that are implemented in sd_card_sdio.cpp. It allows SdFat to use
 * BlueSCSI's custom SDIO driver.
 *
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * Copyright (c) 2024 Tech by Androda, LLC
 *
 * This file is licensed under the GPL version 3 or any later version.
 */
#pragma once

#include <SdCard/SdCardInterface.h>

// SDIO option flags - basic
#ifndef FIFO_SDIO
#define FIFO_SDIO 0
#endif
#ifndef DMA_SDIO
#define DMA_SDIO 1
#endif

// Extended SDIO options for BlueSCSI Ultra (these may already be defined
// in BlueSCSI_platform_gpio_ultra.h, so use ifndef guards)
#ifndef SDIO_HS
#define SDIO_HS   0b0010
#endif
#ifndef SDIO_US
#define SDIO_US   0b0100
#endif
#ifndef SDIO_1_8
#define SDIO_1_8  0b1000
#endif
#ifndef SDIO_M_D
#define SDIO_M_D  0b10000
#endif
#ifndef SDIO_LOG
#define SDIO_LOG  0b100000
#endif
#ifndef SDIO_FIN
#define SDIO_FIN  0b10000000
#endif

/**
 * \class SdioConfig
 * \brief SDIO card configuration for BlueSCSI.
 */
class SdioConfig {
 public:
  SdioConfig() {}
  /**
   * SdioConfig constructor.
   * \param[in] opt SDIO options.
   */
  explicit SdioConfig(uint8_t opt) : m_options(opt) {}
  /** \return SDIO card options. */
  uint8_t options() { return m_options; }
  /** \return true if DMA_SDIO. */
  bool useDma() { return m_options & DMA_SDIO; }

 private:
  uint8_t m_options = FIFO_SDIO;
};

/**
 * \class SdioCard
 * \brief Raw SDIO access to SD and SDHC flash memory cards.
 *
 * Implementation is in sd_card_sdio.cpp
 */
class SdioCard : public SdCardInterface {
 public:
  /** Initialize the SD card.
   * \param[in] sdioConfig SDIO card configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdioConfig sdioConfig);

  /** CMD6 Switch mode: Check Function Set Function.
   * \param[in] arg CMD6 argument.
   * \param[out] status return status data.
   * \return true for success or false for failure.
   */
  bool cardCMD6(uint32_t arg, uint8_t* status) final;

  /** Disable an SDIO card. Not implemented. */
  void end() final {}

  /** Erase a range of sectors.
   * \param[in] firstSector The address of the first sector in the range.
   * \param[in] lastSector The address of the last sector in the range.
   * \return true for success or false for failure.
   */
  bool erase(Sector_t firstSector, Sector_t lastSector) final;

  /** \return code for the last error. */
  uint8_t errorCode() const final;

  /** \return error data for last error. */
  uint32_t errorData() const final;

  /** \return error line for last error. */
  uint32_t errorLine() const;

  /** Check for busy with CMD13.
   * \return true if busy else false.
   */
  bool isBusy() final;

  /** \return the SD clock frequency in kHz. */
  uint32_t kHzSdClk();

  /** Read a 512 byte sector from an SD card.
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSector(Sector_t sector, uint8_t* dst) final;

  /** Read multiple 512 byte sectors from an SD card.
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSectors(Sector_t sector, uint8_t* dst, size_t ns) final;

  /** Read a card's CID register.
   * \param[out] cid pointer to area for returned data.
   * \return true for success or false for failure.
   */
  bool readCID(cid_t* cid) final;

  /** Read a card's CSD register.
   * \param[out] csd pointer to area for returned data.
   * \return true for success or false for failure.
   */
  bool readCSD(csd_t* csd) final;

  /** Read one data sector in a multiple sector read sequence.
   * \param[out] dst Pointer to the location for the data to be read.
   * \return true for success or false for failure.
   */
  bool readData(uint8_t* dst);

  /** Read OCR register.
   * \param[out] ocr Value of OCR register.
   * \return true for success or false for failure.
   */
  bool readOCR(uint32_t* ocr) final;

  /** Read SCR register.
   * \param[out] scr Value of SCR register.
   * \return true for success or false for failure.
   */
  bool readSCR(scr_t* scr) final;

  /** Return the 64 byte SD Status register.
   * \param[out] sds location for 64 status bytes.
   * \return true for success or false for failure.
   */
  bool readSDS(sds_t* sds) final;

  /** Start a read multiple sectors sequence.
   * \param[in] sector Address of first sector in sequence.
   * \return true for success or false for failure.
   */
  bool readStart(Sector_t sector);

  /** End a read multiple sectors sequence.
   * \return true for success or false for failure.
   */
  bool readStop();

  /** \return SDIO card status. */
  uint32_t status() final;

  /** Determine the size of an SD flash memory card.
   * \return The number of 512 byte data sectors in the card
   *         or zero if an error occurs.
   */
  Sector_t sectorCount() final;

  /** Send CMD12 to stop read or write.
   * \param[in] blocking If true, wait for command complete.
   * \return true for success or false for failure.
   */
  bool stopTransmission(bool blocking);

  /** \return success if sync successful. */
  bool syncDevice() final;

  /** Return the card type: SD V1, SD V2 or SDHC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC.
   */
  uint8_t type() const final;

  /** Writes a 512 byte sector to an SD card.
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSector(Sector_t sector, const uint8_t* src) final;

  /** Write multiple 512 byte sectors to an SD card.
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSectors(Sector_t sector, const uint8_t* src, size_t ns) final;

  /** Write one data sector in a multiple sector write sequence.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeData(const uint8_t* src);

  /** Start a write multiple sectors sequence.
   * \param[in] sector Address of first sector in sequence.
   * \return true for success or false for failure.
   */
  bool writeStart(Sector_t sector);

  /** End a write multiple sectors sequence.
   * \return true for success or false for failure.
   */
  bool writeStop();
};
