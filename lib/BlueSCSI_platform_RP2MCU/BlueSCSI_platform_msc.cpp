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

#include <device/usbd.h>
#include <hardware/gpio.h>

#include "BlueSCSI_platform.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_msc.h"
#include "BlueSCSI_msc_initiator.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_settings.h"
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

// external images configuration
extern image_config_t g_DiskImages[S2S_MAX_TARGETS];

static bool g_msc_lock; // To block re-entrant calls
static bool g_msc_usb_mutex_held;

/* globals */
static struct {
  uint8_t lun_unitReady[S2S_MAX_TARGETS];
  image_config_t * lun_config[S2S_MAX_TARGETS];
  uint8_t lun_count = 0;
  uint8_t unitReady = 0;
  uint8_t SDMode = 1;
  uint8_t lun_count_prev_response = 0;
} g_MSC;

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

/* return true if USB presence detected / eligible to enter CR mode */
bool platform_sense_msc() {
#if defined(BLUESCSI_PICO) || defined(BLUESCSI_PICO_2) || defined(BLUESCSI_V2)
  // check if we're USB powered, if not, exit immediately
  // pin on the wireless module, see https://github.com/earlephilhower/arduino-pico/discussions/835
  // Update: from the above discussion the offset 32 has been changed to 64 to access CYW43 GPIO pins
  // since the addition of the RP2350 chips, now stored in the DIGITAL_PIN_CYW43_OFFSET define
  if (platform_check_picow() && !digitalRead(DIGITAL_PIN_CYW43_OFFSET + 2)) {
    return false;
  }

  if (!platform_check_picow() && !digitalRead(24)) {
    return false;
  }
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

/* perform periodic tasks, return true if we should remain in card reader mode */
bool platform_run_msc() {
  return g_MSC.unitReady;
}

/* load the setting if we present images or not */
void platform_set_msc_image_mode(bool image_mode) {
  g_MSC.SDMode = !image_mode;
}

/* return true if the image type makes sense as a mass-storage device */
bool msc_image_eligble(uint8_t t) {
  switch (t) {
    case S2S_CFG_FIXED:
    case S2S_CFG_REMOVABLE:
    case S2S_CFG_MO: /* not actually sure about this and zip */
    case S2S_CFG_ZIP100:
      return true;

    case S2S_CFG_OPTICAL: /* will not contain a MBR */
    case S2S_CFG_FLOPPY_14MB: /* will not contain a MBR */ 
    case S2S_CFG_SEQUENTIAL: /* tape drive */
    case S2S_CFG_NETWORK: /* always empty */
    case S2S_CFG_NOT_SET:
    default:
      return false;
  } 
}

/* perform MSC class preinit tasks */
void platform_enter_msc() {
  dbgmsg("USB MSC buffer size: ", CFG_TUD_MSC_EP_BUFSIZE);
  g_MSC.lun_count = 0;
    
  if (!g_MSC.SDMode) {
    logmsg("Presenting configured images as USB storage devices");
    for (int i = 0; i < S2S_MAX_TARGETS; i++) {
      if (msc_image_eligble(g_DiskImages[i].deviceType) && g_DiskImages[i].file.isOpen()) {
          logmsg("USB LUN ", (int)g_MSC.lun_count," => ",g_DiskImages[i].current_image);

          // anything but linux probably won't deal gracefully with nonstandard or odd sector sizes, present a warning
          if (g_DiskImages[i].bytesPerSector != 512 && g_DiskImages[i].bytesPerSector != 4096) {
              logmsg("Warning: USB LUN ",(int)g_MSC.lun_count," uses a sector size of ",g_DiskImages[i].bytesPerSector,". Not all OS can deal with this!");
          }

          g_MSC.lun_config[g_MSC.lun_count] = &g_DiskImages[i];
          g_MSC.lun_unitReady[g_MSC.lun_count] = 1;       
          g_MSC.lun_count ++;  
      }
    }

    if (g_MSC.lun_count == 0) {
      logmsg("No images to present, falling back to SD card!");
      g_MSC.SDMode = 1;
    } else 
      logmsg("Total USB LUN ", (int)g_MSC.lun_count);
  }

  if (g_MSC.SDMode) {
    logmsg("Presenting SD card as USB storage device");
    g_MSC.lun_count = 1;
    g_MSC.lun_unitReady[0] = 1;
  }

  // MSC is ready for read/write
  g_MSC.unitReady = g_MSC.lun_count;

  if (g_MSC.lun_count_prev_response != 0 &&
      g_MSC.lun_count != g_MSC.lun_count_prev_response)
  {
    // Host has already queried us for the number of LUNs, but
    // our response has now changed. We need to re-enumerate to
    // update it.
    g_MSC.lun_count_prev_response = 0;
    tud_disconnect();
    delay(250);
    tud_connect();
  }
}

/* perform any cleanup tasks for the MSC-specific functionality */
void platform_exit_msc() {
   g_MSC.unitReady = 0;
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

  const char vid[] = "BlueSCSI";
  const char pid[] = PLATFORM_PID; 
  const char rev[] = PLATFORM_REVISION;

  memcpy(vendor_id, vid, tu_min32(strlen(vid), 8));
  memcpy(product_id, pid, tu_min32(strlen(pid), 16));
  memcpy(product_rev, rev, tu_min32(strlen(rev), 4));
}

// max LUN supported
// we only have the one SD card
extern "C" uint8_t tud_msc_get_maxlun_cb(void)
{
  MSCScopedLock lock;
  uint8_t result;

  if (g_msc_initiator)
  {
    result = init_msc_get_maxlun_cb();
  }
  else if (g_MSC.lun_count != 0)
  {
    result = g_MSC.lun_count; // number of LUNs supported
  }
  else
  {
    // Returning 0 makes TU_VERIFY(maxlun); fail in tinyusb/src/class/msc/msc_device.c:378
    // This stalls the endpoint and causes an unnecessary enumeration delay on Windows.
    result = 1;
  }

  g_MSC.lun_count_prev_response = result;
  return result;
}

// return writable status
// on platform supporting write protect switch, could do that here.
// otherwise this is not actually needed
extern "C" bool tud_msc_is_writable_cb (uint8_t lun)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_is_writable_cb(lun);
  if (g_MSC.SDMode) return g_MSC.unitReady;
  
  (void) lun;
  return g_MSC.unitReady && g_MSC.lun_unitReady[lun] && g_MSC.lun_config[lun]->file.isWritable();
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
      g_MSC.lun_unitReady[lun] = false;

      if (g_MSC.unitReady) // no more active LUNs -> global not ready flag
        g_MSC.unitReady --;
    }
  }

  return true;
}

// return true if we are ready to service reads/writes
extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_test_unit_ready_cb(lun);

  return g_MSC.unitReady && g_MSC.lun_unitReady[lun];
}

// return size in blocks and block size
extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size)
{
  MSCScopedLock lock;
  if (g_msc_initiator) return init_msc_capacity_cb(lun, block_count, block_size);

  if (g_MSC.SDMode) {
    *block_count = g_MSC.unitReady ? (SD.card()->sectorCount()) : 0;
    *block_size = SD_SECTOR_SIZE;
  } else { // present the bytesPerSector of file, though it remains to be seen if host will like this
    *block_count = (g_MSC.unitReady && g_MSC.lun_unitReady[lun]) ? (g_MSC.lun_config[lun]->file.size() / g_MSC.lun_config[lun]->bytesPerSector) : 0;
    *block_size = g_MSC.lun_config[lun]->bytesPerSector;
  }
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

  bool rc = 0;

  if (g_MSC.SDMode) {
    rc = SD.card()->readSectors(lba, (uint8_t*) buffer, bufsize/SD_SECTOR_SIZE);
  } else {
    if (g_MSC.lun_unitReady[lun]) {
      g_MSC.lun_config[lun]->file.seek(lba * g_MSC.lun_config[lun]->bytesPerSector);
      rc = g_MSC.lun_config[lun]->file.read(buffer, bufsize);
    } else {
      logmsg("Attempted read to non-ready LUN ",lun);
    }
  }

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
  if (g_msc_initiator) return init_msc_write10_cb(lun, lba, offset, buffer, bufsize);

  bool rc = 0;

  if (g_MSC.SDMode) {
    rc = SD.card()->writeSectors(lba, buffer, bufsize/SD_SECTOR_SIZE); 
  } else {
    if (g_MSC.lun_unitReady[lun]) {
      g_MSC.lun_config[lun]->file.seek(lba * g_MSC.lun_config[lun]->bytesPerSector);
      rc = g_MSC.lun_config[lun]->file.write(buffer, bufsize);
    } else {
      logmsg("Attempted write to non-ready LUN ",lun);
    }
  }

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
