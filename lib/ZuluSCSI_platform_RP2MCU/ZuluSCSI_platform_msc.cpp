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

/* platform specific MSC routines */
#ifdef PLATFORM_MASS_STORAGE

#include <SdFat.h>
#include <device/usbd.h>
#include <hardware/gpio.h>
#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_msc.h"
#include "ZuluSCSI_msc_initiator.h"
#include "ZuluSCSI_config.h"
#include "ZuluSCSI_settings.h"
#include <class/msc/msc.h>
#include <class/msc/msc_device.h>

#include <pico/mutex.h>
extern mutex_t __usb_mutex;

#if CFG_TUD_MSC_EP_BUFSIZE < SD_SECTOR_SIZE
  #error "CFG_TUD_MSC_EP_BUFSIZE is too small! It needs to be at least 512 (SD_SECTOR_SIZE)"
#endif

#define DIGITAL_PIN_CYW43_OFFSET 64

// external global SD variable
extern SdFs SD;
static bool unitReady = false;

static bool g_msc_lock; // To block re-entrant calls
static bool g_msc_usb_mutex_held;

void platform_msc_lock_set(bool block)
{
  if (block)
  {
    if (g_msc_lock)
    {
      logmsg("Re-entrant MSC lock!");
      assert(false);
    }

    g_msc_usb_mutex_held = mutex_try_enter(&__usb_mutex, NULL); // Blocks USB IRQ if not already blocked
    g_msc_lock = true; // Blocks platform USB polling
  }
  else
  {
    if (!g_msc_lock)
    {
      logmsg("MSC lock released when not held!");
      assert(false);
    }

    g_msc_lock = false;

    if (g_msc_usb_mutex_held)
    {
      g_msc_usb_mutex_held = false;
      mutex_exit(&__usb_mutex);
    }
  }
}

bool platform_msc_lock_get()
{
  return g_msc_lock;
}

struct MSCScopedLock {
public:
  MSCScopedLock() {  platform_msc_lock_set(true); }
  ~MSCScopedLock() { platform_msc_lock_set(false); }
};

/* return true if USB presence detected / eligble to enter CR mode */
bool platform_sense_msc() {

#if defined(ZULUSCSI_PICO) || defined(ZULUSCSI_PICO_2)
  // check if we're USB powered, if not, exit immediately
  // pin on the wireless module, see https://github.com/earlephilhower/arduino-pico/discussions/835
  // Update: from the above discussion the offset 32 has been changed to 64 to access CYW43 GPIO pins
  // since the addition of the RP2350 chips, now stored in the DIGITAL_PIN_CYW43_OFFSET define
  if (rp2040.isPicoW() && !digitalRead(DIGITAL_PIN_CYW43_OFFSET + 2))
    return false;

  if (!rp2040.isPicoW() && !digitalRead(24))
    return false;
#endif

  logmsg("Waiting for USB enumeration to enter Card Reader mode.");

  // wait for up to a second to be enumerated
  uint32_t start = millis();
  bool timed_out = false;
  uint16_t usb_timeout =  g_scsi_settings.getSystem()->usbMassStorageWaitPeriod;
  while (!tud_connected())
  {
    if ((uint32_t)(millis() - start) > usb_timeout)
    {
      logmsg("Waiting for USB enumeration timed out after ", usb_timeout, "ms.");
      logmsg("-- Try increasing 'USBMassStorageWaitPeriod' in the ", CONFIGFILE);
      timed_out = true;
      break;
    } 
    delay(100);
  }
  if (!timed_out)
    dbgmsg("USB enumeration took ", (int)((uint32_t)(millis() - start)), "ms");
  // tud_connected returns True if just got out of Bus Reset and received the very first data from host
  // https://github.com/hathach/tinyusb/blob/master/src/device/usbd.h#L63
  return tud_connected();
}

/* return true if we should remain in card reader mode and perform periodic tasks */
bool platform_run_msc() {
  return unitReady;
}

/* perform MSC class preinit tasks */
void platform_enter_msc() {
  dbgmsg("USB MSC buffer size: ", CFG_TUD_MSC_EP_BUFSIZE);
  // MSC is ready for read/write
  // we don't need any prep, but the var is requried as the MSC callbacks are always active
  unitReady = true;
}

/* perform any cleanup tasks for the MSC-specific functionality */
void platform_exit_msc() {
  unitReady = false;
}

/* TinyUSB mass storage callbacks follow */

// usb framework checks this func exists for mass storage config. no code needed.
void __USBInstallMassStorage() { }

// Invoked when received SCSI_CMD_INQUIRY
// fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
extern "C" void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {

  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_inquiry_cb(lun, vendor_id, product_id, product_rev);

  const char vid[] = "ZuluSCSI";
  const char pid[] = PLATFORM_PID; 
  const char rev[] = "1.0";

  memcpy(vendor_id, vid, tu_min32(strlen(vid), 8));
  memcpy(product_id, pid, tu_min32(strlen(pid), 16));
  memcpy(product_rev, rev, tu_min32(strlen(rev), 4));
}

// max LUN supported
// we only have the one SD card
extern "C" uint8_t tud_msc_get_maxlun_cb(void)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_get_maxlun_cb();

  return 1; // number of LUNs supported
}

// return writable status
// on platform supporting write protect switch, could do that here.
// otherwise this is not actually needed
extern "C" bool tud_msc_is_writable_cb (uint8_t lun)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_is_writable_cb(lun);

  (void) lun;
  return unitReady;
}

// see https://www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf pg 221
extern "C" bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_start_stop_cb(lun, power_condition, start, load_eject);

  if (load_eject)  {
    if (start) {
      // load disk storage
      // do nothing as we started "loaded"
    } else {
      unitReady = false;
    }
  }

  return true;
}

// return true if we are ready to service reads/writes
extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_test_unit_ready_cb(lun);

  return unitReady;
}

// return size in blocks and block size
extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_capacity_cb(lun, block_count, block_size);

  *block_count = unitReady ? (SD.card()->sectorCount()) : 0;
  *block_size = SD_SECTOR_SIZE;
}

// Callback invoked when received an SCSI command not in built-in list (below) which have their own callbacks
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE, READ10 and WRITE10
extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer,
                        uint16_t bufsize)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_scsi_cb(lun, scsi_cmd, buffer, bufsize);

  const void *response = NULL;
  uint16_t resplen = 0;

  switch (scsi_cmd[0]) {
  case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
    // Host is about to read/write etc ... better not to disconnect disk
    resplen = 0;
    break;

  default:
    // Set Sense = Invalid Command Operation
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    // negative means error -> tinyusb could stall and/or response with failed status
    resplen = -1;
    break;
  }

  // return len must not larger than bufsize
  if (resplen > bufsize) {
    resplen = bufsize;
  }

  // copy response to stack's buffer if any
  if (response && resplen) {
    memcpy(buffer, response, resplen);
  }

  return resplen;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes (must be multiple of block size)
extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, 
                            void* buffer, uint32_t bufsize)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_read10_cb(lun, lba, offset, buffer, bufsize);

  bool rc = SD.card()->readSectors(lba, (uint8_t*) buffer, bufsize/SD_SECTOR_SIZE);

  // only blink fast on reads; writes will override this
  if (MSC_LEDMode == LED_SOLIDON)
    MSC_LEDMode = LED_BLINK_FAST;
  
  return rc ? bufsize : -1;
}

// Callback invoked when receive WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes (must be multiple of block size)
extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_read10_cb(lun, lba, offset, buffer, bufsize);

  bool rc = SD.card()->writeSectors(lba, buffer, bufsize/SD_SECTOR_SIZE);

  // always slow blink
  MSC_LEDMode = LED_BLINK_SLOW;

  return rc ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache to storage
extern "C" void tud_msc_write10_complete_cb(uint8_t lun)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_write10_complete_cb(lun);
}

#endif
