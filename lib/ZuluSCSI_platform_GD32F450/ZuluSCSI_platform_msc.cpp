/**
 * Copyright (c) 2023-2024 zigzagjoe
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

#ifdef PLATFORM_MASS_STORAGE

#include <SdFat.h>
#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_msc.h"
#include "ZuluSCSI_config.h"
#include "ZuluSCSI_settings.h"
#include "usb_serial.h"

/* gd32 USB code is all C linked */
extern "C" {
  #include <drv_usb_hw.h>
  #include <usbd_core.h>
  #include <drv_usbd_int.h>

  #include "usb_conf.h"
  #include "usbd_msc_core.h"
  #include "usbd_msc_mem.h"
  #include "usbd_msc_bbb.h"
}


/* local function prototypes ('static') */
static int8_t storageInit(uint8_t Lun);
static int8_t storageIsReady(uint8_t Lun);
static int8_t storageIsWriteProtected(uint8_t Lun);
static int8_t storageGetMaxLun(void);
static int8_t storageRead(uint8_t Lun,
                           uint8_t *buf,
                           uint32_t BlkAddr,
                           uint16_t BlkLen);
static int8_t storageWrite(uint8_t Lun,
                            uint8_t *buf,
                            uint32_t BlkAddr,
                            uint16_t BlkLen);

usbd_mem_cb USBD_SD_fops = {
    .mem_init      = storageInit,
    .mem_ready     = storageIsReady,
    .mem_protected = storageIsWriteProtected,
    .mem_read      = storageRead,
    .mem_write     = storageWrite,
    .mem_maxlun    = storageGetMaxLun,

    .mem_inquiry_data = {(uint8_t *)storageInquiryData},
    .mem_block_size   = {SD_SECTOR_SIZE},
    .mem_block_len    = {0}
};

usbd_mem_cb *usbd_mem_fops = &USBD_SD_fops;

// shared with usb serial
extern usb_core_driver cdc_acm;

// external global SD variable
extern SdFs SD;

// private globals
static bool unitReady = false;

/* returns true if card reader mode should be entered. sd card is available. */
bool platform_sense_msc() {

  // kill usb serial.
  usbd_disconnect (&cdc_acm);
  // set the MSC storage size
  usbd_mem_fops->mem_block_len[0] = SD.card()->sectorCount();
  unitReady = true;
  
  // init the MSC class, uses ISR and other global routines from usb_serial.cpp
  usbd_init(&cdc_acm, USB_CORE_ENUM_HS, &msc_desc, &msc_class);

  logmsg("Waiting for USB enumeration to expose SD card as a mass storage device");

  // wait to be begin to be enumerated
  uint32_t start = millis();
  uint16_t usb_timeout =  g_scsi_settings.getSystem()->usbMassStorageWaitPeriod;
  while ((uint32_t)(millis() - start) < usb_timeout)
  {
    if (cdc_acm.dev.cur_status >= USBD_ADDRESSED)
    {
      dbgmsg("USB enumeration took ", (int)((uint32_t)(millis() - start)), "ms");
      return true;
    }
  }

  logmsg("Waiting for USB enumeration timed out after ", usb_timeout, "ms.");
  logmsg("-- Try increasing 'USBMassStorageWaitPeriod' in the ", CONFIGFILE);
  //if not, disconnect MSC class...
  usbd_disconnect (&cdc_acm);

  // and bring serial back for later.
  usb_serial_init();

  return false;
}

/* perform MSC-specific init tasks */
void platform_enter_msc() {
  dbgmsg("USB MSC buffer size: ", (uint32_t) MSC_MEDIA_PACKET_SIZE);

  // give the host a moment to finish enumerate and "load" media
  uint32_t start = millis();
  uint16_t usb_timeout =  g_scsi_settings.getSystem()->usbMassStorageWaitPeriod;
  while ((USBD_CONFIGURED != cdc_acm.dev.cur_status) && ((uint32_t)(millis() - start) < usb_timeout ) ) 
    delay(100);
}

/* return true while remaining in msc mode, and perform periodic tasks */
bool platform_run_msc() {
  usbd_msc_handler *msc = (usbd_msc_handler *)cdc_acm.dev.class_data[USBD_MSC_INTERFACE];

  // stupid windows doesn't send start_stop_unit events if it is ejected via safely remove devices. 
  // it just stops talking to the device so we don't know we've been ejected....
  // other OSes always send the start_stop_unit, windows does too when ejected from explorer.
  // so we watch for the OS suspending device and assume we're done in USB mode if so.
  // this will also trigger if the host were to suspend usb device due to going to sleep
  // however, I hope no sane OS would sleep mid transfer or with a dirty filesystem. 
  // Note: Mac OS X apparently not sane.
  uint8_t is_suspended = cdc_acm.dev.cur_status == (uint8_t)USBD_SUSPENDED;

  return (! msc->scsi_disk_pop) && !is_suspended;
}

void platform_exit_msc() {
  unitReady = false;

  // disconnect msc....
  usbd_disconnect (&cdc_acm);

  // catch our breath....
  delay(200);
  
  // ... and bring usb serial up
  usb_serial_init();
}

/*!
    \brief      initialize the storage medium
    \param[in]  Lun: logical unit number
    \param[out] none
    \retval     status
*/
static int8_t storageInit(uint8_t Lun)
{
    return 0;
}

/*!
    \brief      check whether the medium is ready
    \param[in]  Lun: logical unit number
    \param[out] none
    \retval     status
*/
static int8_t storageIsReady(uint8_t Lun)
{
    return ! unitReady; // 0 = success / unit is ready
}

/*!
    \brief      check whether the medium is write-protected
    \param[in]  Lun: logical unit number
    \param[out] none
    \retval     status
*/
static int8_t storageIsWriteProtected(uint8_t Lun)
{
    return ! unitReady; // 0 = read/write
}

/*!
    \brief      read data from the medium
    \param[in]  Lun: logical unit number
    \param[in]  buf: pointer to the buffer to save data
    \param[in]  BlkAddr: address of 1st block to be read
    \param[in]  BlkLen: number of blocks to be read
    \param[out] none
    \retval     status
*/
static int8_t storageRead(uint8_t Lun,
                           uint8_t *buf,
                           uint32_t BlkAddr,
                           uint16_t BlkLen)
{
    // divide by sector size to convert address to LBA
    bool rc = SD.card()->readSectors(BlkAddr/SD_SECTOR_SIZE, buf, BlkLen);

    // only blink fast on reads; writes will override this
    if (MSC_LEDMode == LED_SOLIDON)
      MSC_LEDMode = LED_BLINK_FAST;
      
    return !rc;
}

/*!
    \brief      write data to the medium
    \param[in]  Lun: logical unit number
    \param[in]  buf: pointer to the buffer to write
    \param[in]  BlkAddr: address of 1st block to be written
    \param[in]  BlkLen: number of blocks to be write
    \param[out] none
    \retval     status
*/
static int8_t storageWrite(uint8_t Lun,
                            uint8_t *buf,
                            uint32_t BlkAddr,
                            uint16_t BlkLen)
{
    // divide by sector size to convert address to LBA
    bool rc = SD.card()->writeSectors(BlkAddr/SD_SECTOR_SIZE, buf, BlkLen);

    // always slow blink
    MSC_LEDMode = LED_BLINK_SLOW;

    return !rc;
}

/*!
    \brief      get number of supported logical unit
    \param[in]  none
    \param[out] none
    \retval     number of logical unit
*/
static int8_t storageGetMaxLun(void)
{
    return 0; // number of LUNs supported - 1
}

#endif // PLATFORM_MASS_STORAGE