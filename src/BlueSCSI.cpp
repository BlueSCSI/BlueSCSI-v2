/*
 *  BlueSCSI v2
 *  Copyright (c) 2023 Eric Helgeson, Androda, and contributors.
 *
 *  This project is based on ZuluSCSI, BlueSCSI v1, and SCSI2SD:
 * 
 *  ZuluSCSI
 *  Copyright (c) 2022 Rabbit Hole Computing
 * 
 * This project is based on BlueSCSI:
 *
 *  BlueSCSI
 *  Copyright (c) 2021  Eric Helgeson, Androda
 *  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see https://github.com/erichelgeson/bluescsi.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  
 *     Copyright (c) 2019 komatsu   
 *  
 *     Permission to use, copy, modify, and/or distribute this software  
 *     for any purpose with or without fee is hereby granted, provided  
 *     that the above copyright notice and this permission notice appear  
 *     in all copies.  
 *  
 *     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL  
 *     WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED  
 *     WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE  
 *     AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR  
 *     CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS  
 *     OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,  
 *     NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN  
 *     CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  
 */

#include <SdFat.h>
#include <minIni.h>
#include <minIni_cache.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "BlueSCSI_config.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_initiator.h"
#include "ROMDrive.h"

SdFs SD;
FsFile g_logfile;
static bool g_romdrive_active;
static bool g_sdcard_present;

/************************************/
/* Status reporting by blinking led */
/************************************/

#define BLINK_STATUS_OK 1
#define BLINK_ERROR_NO_IMAGES  3
#define BLINK_ERROR_NO_SD_CARD 5

void blinkStatus(int count)
{
  for (int i = 0; i < count; i++)
  {
    LED_ON();
    delay(250);
    LED_OFF();
    delay(250);
  }
}

extern "C" void s2s_ledOn()
{
  LED_ON();
}

extern "C" void s2s_ledOff()
{
  LED_OFF();
}

/**************/
/* Log saving */
/**************/

void save_logfile(bool always = false)
{
  static uint32_t prev_log_pos = 0;
  static uint32_t prev_log_len = 0;
  static uint32_t prev_log_save = 0;
  uint32_t loglen = log_get_buffer_len();

  if (loglen != prev_log_len && g_sdcard_present)
  {
    // When debug is off, save log at most every LOG_SAVE_INTERVAL_MS
    // When debug is on, save after every SCSI command.
    if (always || g_log_debug || (LOG_SAVE_INTERVAL_MS > 0 && (uint32_t)(millis() - prev_log_save) > LOG_SAVE_INTERVAL_MS))
    {
      g_logfile.write(log_get_buffer(&prev_log_pos));
      g_logfile.flush();

      prev_log_len = loglen;
      prev_log_save = millis();
    }
  }
}

void init_logfile()
{
  static bool first_open_after_boot = true;

  bool truncate = first_open_after_boot;
  int flags = O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND);
  g_logfile = SD.open(LOGFILE, flags);
  if (!g_logfile.isOpen())
  {
    log("Failed to open log file: ", SD.sdErrorCode());
  }
  save_logfile(true);

  first_open_after_boot = false;
}

const char * fatTypeToChar(int fatType)
{
  switch (fatType)
  {
    case FAT_TYPE_EXFAT:
      return "exFAT";
    case FAT_TYPE_FAT32:
      return "FAT32";
    case FAT_TYPE_FAT16:
      return "FAT16";
    case FAT_TYPE_FAT12:
      return "FAT12";
    default:
      return "Unknown";
  }
}

void print_sd_info()
{
  log(" ");
  log("=== SD Card Info ===");
  uint64_t size = (uint64_t)SD.vol()->clusterCount() * SD.vol()->bytesPerCluster();
  log("SD card detected, ", fatTypeToChar((int)SD.vol()->fatType()),
          " volume size: ", (int)(size / 1024 / 1024), " MB");

  cid_t sd_cid;

  if(SD.card()->readCID(&sd_cid))
  {
    char sdname[6] = {sd_cid.pnm[0], sd_cid.pnm[1], sd_cid.pnm[2], sd_cid.pnm[3], sd_cid.pnm[4], 0};
    log("SD Name: ", sdname, ", MID: ", (uint8_t)sd_cid.mid, ", OID: ", (uint8_t)sd_cid.oid[0], " ", (uint8_t)sd_cid.oid[1]);

    debuglog("SD Date: ", (int)sd_cid.mdtMonth(), "/", sd_cid.mdtYear());
    debuglog("SD Serial: ", sd_cid.psn());
  }
}

/*********************************/
/* Harddisk image file handling  */
/*********************************/
const char * typeToChar(int deviceType)
{
  switch (deviceType)
  {
  case S2S_CFG_OPTICAL:
    return "Optical";
  case S2S_CFG_FIXED:
    return "Fixed";
  case S2S_CFG_FLOPPY_14MB:
    return "Floppy1.4MB";
  case S2S_CFG_MO:
    return "MO";
  case S2S_CFG_NETWORK:
    return "Network";
  case S2S_CFG_SEQUENTIAL:
    return "Tape";
  case S2S_CFG_REMOVEABLE:
    return "Removable";
  case S2S_CFG_ZIP100:
    return "ZIP100";
  default:
    return "Unknown";
  }
}

const char * quirksToChar(int quirks)
{
  switch (quirks)
  {
  case S2S_CFG_QUIRKS_APPLE:
    return "Apple";
  case S2S_CFG_QUIRKS_OMTI:
    return "OMTI";
  case S2S_CFG_QUIRKS_VMS:
    return "VMS";
  case S2S_CFG_QUIRKS_XEBEC:
    return "XEBEC";
  case S2S_CFG_QUIRKS_X68000:
    return "X68000";
  case S2S_CFG_QUIRKS_NONE:
    return "None";
  default:
    return "Unknown";
  }
}

// Iterate over the root path in the SD card looking for candidate image files.
bool findHDDImages()
{
  char imgdir[MAX_FILE_PATH];
  ini_gets("SCSI", "Dir", "/", imgdir, sizeof(imgdir), CONFIGFILE);
  int dirindex = 0;

  log(" ");
  log("=== Finding images in ", imgdir, " ===");

  SdFile root;
  root.open(imgdir);
  if (!root.isOpen())
  {
    log("Could not open directory: ", imgdir);
  }

  SdFile file;
  bool imageReady;
  bool foundImage = false;
  int usedDefaultId = 0;
  while (1)
  {
    if (!file.openNext(&root, O_READ))
    {
      // Check for additional directories with ini keys Dir1..Dir9
      while (dirindex < 10)
      {
        dirindex++;
        char key[5] = "Dir0";
        key[3] += dirindex;
        if (ini_gets("SCSI", key, "", imgdir, sizeof(imgdir), CONFIGFILE) != 0)
        {
          break;
        }
      }

      if (imgdir[0] != '\0')
      {
        log("== Finding HDD images in additional Dir", (int)dirindex, " = \"", imgdir, "\" ==");
        root.open(imgdir);
        if (!root.isOpen())
        {
          log("-- Could not open directory: ", imgdir);
        }
        continue;
      }
      else
      {
        break;
      }
    }

    char name[MAX_FILE_PATH+1];
    if(!file.isDir()) 
    {
      file.getName(name, MAX_FILE_PATH+1);
      file.close();
      bool is_hd = (tolower(name[0]) == 'h' && tolower(name[1]) == 'd');
      bool is_cd = (tolower(name[0]) == 'c' && tolower(name[1]) == 'd');
      bool is_fd = (tolower(name[0]) == 'f' && tolower(name[1]) == 'd');
      bool is_mo = (tolower(name[0]) == 'm' && tolower(name[1]) == 'o');
      bool is_ne = (tolower(name[0]) == 'n' && tolower(name[1]) == 'e');
      bool is_re = (tolower(name[0]) == 'r' && tolower(name[1]) == 'e');
      bool is_tp = (tolower(name[0]) == 't' && tolower(name[1]) == 'p');
      bool is_zp = (tolower(name[0]) == 'z' && tolower(name[1]) == 'p');

      if(strcasecmp(name, "CLEAR_ROM") == 0)
      {
        romDriveClear();
        continue;
      }

      if (is_hd || is_cd || is_fd || is_mo || is_ne || is_re || is_tp || is_zp)
      {
        // Check if the image should be loaded to microcontroller flash ROM drive
        bool is_romdrive = false;
        const char *extension = strrchr(name, '.');
        if (extension && strcasecmp(extension, ".rom") == 0)
        {
          is_romdrive = true;
        }

        // skip file if the name indicates it is not a valid image container
        if (!is_romdrive && !scsiDiskFilenameValid(name)) continue;

        // Defaults for Hard Disks
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        if (is_cd)
        {
          // Use 2048 as the default sector size for CD-ROMs
          blk = 2048;
        }

        // Parse SCSI device ID
        int file_name_length = strlen(name);
        if(file_name_length > 2) { // HD[N]
          int tmp_id = name[HDIMG_ID_POS] - '0';

          if(tmp_id > -1 && tmp_id < 8)
          {
            id = tmp_id; // If valid id, set it, else use default
          }
          else
          {
            id = usedDefaultId++;
          }
        }

        // Parse SCSI LUN number
        if(file_name_length > 3) // HD0[N]
        {
          int tmp_lun = name[HDIMG_LUN_POS] - '0';

          if(tmp_lun > -1 && tmp_lun < NUM_SCSILUN)
          {
            lun = tmp_lun; // If valid id, set it, else use default
          }
        }

        blk = getBlockSize(name, id, blk);

        // Add the directory name to get the full file path
        char fullname[MAX_FILE_PATH * 2 + 2] = {0};
        strncpy(fullname, imgdir, MAX_FILE_PATH);
        if (fullname[strlen(fullname) - 1] != '/') strcat(fullname, "/");
        strcat(fullname, name);

        // Check whether this SCSI ID has been configured yet
        if (s2s_getConfigById(id))
        {
          log("-- Ignoring ", fullname, ", SCSI ID ", id, " is already in use!");
          continue;
        }

        if (is_ne && !platform_network_supported())
        {
          log("-- Ignoring ", fullname, ", networking is not supported on this hardware");
          continue;
        }

        // Type mapping based on filename.
        // If type is FIXED, the type can still be overridden in .ini file.
        S2S_CFG_TYPE type = S2S_CFG_FIXED;
        if (is_cd) type = S2S_CFG_OPTICAL;
        if (is_fd) type = S2S_CFG_FLOPPY_14MB;
        if (is_mo) type = S2S_CFG_MO;
        if (is_ne) type = S2S_CFG_NETWORK;
        if (is_re) type = S2S_CFG_REMOVEABLE;
        if (is_tp) type = S2S_CFG_SEQUENTIAL;
        if (is_zp) type = S2S_CFG_ZIP100;

        // Open the image file
        if (id < NUM_SCSIID && is_romdrive)
        {
          log("== Loading ROM drive from ", fullname, " for ID: ", id);
          imageReady = scsiDiskProgramRomDrive(fullname, id, blk, type);
          
          if (imageReady)
          {
            foundImage = true;
          }
        }
        else if(id < NUM_SCSIID && lun < NUM_SCSILUN)
        {
          log("== Opening ", fullname, " for ID: ", id, " LUN: ", lun);

          imageReady = scsiDiskOpenHDDImage(fullname, id, lun, blk, type);
          if(imageReady)
          {
            foundImage = true;
            log("---- Image ready");
          }
          else
          {
            log("---- Failed to load image");
          }
        } 
        else 
        {
          log("-- Invalid lun or id for image ", fullname);
        }
      }
    }
  }

  if(usedDefaultId > 0)
  {
    log("-- ", usedDefaultId, " images did not specify a SCSI ID and were assigned one if possible.");
  }
  root.close();

  g_romdrive_active = scsiDiskActivateRomDrive();

  // Print SCSI drive map
  log(" ");
  log("=== Configured SCSI Devices ===");
  for (int i = 0; i < NUM_SCSIID; i++)
  {
    const S2S_TargetCfg* cfg = s2s_getConfigByIndex(i);
    if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
    {
      int capacity_kB = ((uint64_t)cfg->scsiSectors * cfg->bytesPerSector) / 1024;

      if (cfg->deviceType == S2S_CFG_NETWORK)
      {
        log("* ID: ", (int)(cfg->scsiId & S2S_CFG_TARGET_ID_BITS),
              ", Type: ", typeToChar((int)cfg->deviceType),
              ", Quirks: ", quirksToChar((int)cfg->quirks));
      }
      else
      {
        log("* ID: ", (int)(cfg->scsiId & S2S_CFG_TARGET_ID_BITS),
              ", BlockSize: ", (int)cfg->bytesPerSector,
              ", Type: ", typeToChar((int)cfg->deviceType),
              ", Quirks: ", quirksToChar((int)cfg->quirks),
              ", Size: ", capacity_kB, "kB");
      }
    }
  }

  return foundImage;
}

/************************/
/* Config file loading  */
/************************/

void readSCSIDeviceConfig()
{
  log(" ");
  log("=== Global Config ===");
  s2s_configInit(&scsiDev.boardCfg);

  for (int i = 0; i < NUM_SCSIID; i++)
  {
    scsiDiskLoadConfig(i);
  }
}

/*********************************/
/* Main SCSI handling loop       */
/*********************************/

static bool mountSDCard()
{
  // Prepare for mounting new SD card by closing all old files.
  // When switching between FAT and exFAT cards the pointers
  // are invalidated and accessing old files results in crash.
  invalidate_ini_cache();
  g_logfile.close();
  scsiDiskCloseSDCardImages();

  // Check for the common case, FAT filesystem as first partition
  if (SD.begin(SD_CONFIG))
  {
    reload_ini_cache(CONFIGFILE);
    return true;
  }

  // Do we have any kind of card?
  if (!SD.card() || SD.sdErrorCode() != 0)
    return false;

  // Try to mount the whole card as FAT (without partition table)
  if (static_cast<FsVolume*>(&SD)->begin(SD.card(), true, 0))
    return true;

  // Failed to mount FAT filesystem, but card can still be accessed as raw image
  return true;
}

static void reinitSCSI()
{
  if (ini_getbool("SCSI", "Debug", 0, CONFIGFILE))
  {
    g_log_debug = true;
  }
  if (ini_getbool("SCSI", "TestMode", 0, CONFIGFILE))
  {
    g_test_mode = true;
  }

#ifdef PLATFORM_HAS_INITIATOR_MODE
  if (platform_is_initiator_mode_enabled())
  {
    // Initialize scsiDev to zero values even though it is not used
    scsiInit();

    // Initializer initiator mode state machine
    scsiInitiatorInit();

    blinkStatus(BLINK_STATUS_OK);

    return;
  }
#endif
  scsiDiskResetImages();
  readSCSIDeviceConfig();
  findHDDImages();

  // Error if there are 0 image files
  if (scsiDiskCheckAnyImagesConfigured())
  {
    // Ok, there is an image, turn LED on for the time it takes to perform init
    LED_ON();
    delay(100);
  }
  else
  {
#if RAW_FALLBACK_ENABLE
    log("No images found, enabling RAW fallback partition");
    scsiDiskOpenHDDImage("RAW:0:0xFFFFFFFF", RAW_FALLBACK_SCSI_ID, 0,
                         RAW_FALLBACK_BLOCKSIZE);
#else
    log("No valid image files found!");
#endif
    blinkStatus(BLINK_ERROR_NO_IMAGES);
  }

  scsiPhyReset();
  scsiDiskInit();
  scsiInit();
#ifdef BLUESCSI_NETWORK
  if (scsiDiskCheckAnyNetworkDevicesConfigured())
  {
    platform_network_init(scsiDev.boardCfg.wifiMACAddress);
    if (scsiDev.boardCfg.wifiSSID[0] != '\0')
    {
      platform_network_wifi_join(scsiDev.boardCfg.wifiSSID, scsiDev.boardCfg.wifiPassword);
    }
  }
#endif
}

void check_and_apply_sdio_delay() {
  long add_sdio_delay = ini_getl("SDIO", "AddClockDelay", 0, CONFIGFILE);
  if (add_sdio_delay) {
    if (add_sdio_delay < 0) {
      log("---- WARNING: Negative numbers are not valid for AddClockDelay. Setting value to 0");
      return;
    }
    if (add_sdio_delay > 2) {
      add_sdio_delay = 2;
      log("---- WARNING: Max value 2 exceeded for AddClockDelay. Setting value to 2.");
    }
    log("INFO: Injecting ", (uint16_t)add_sdio_delay, " additional wait state(s) on SDIO");
    add_extra_sdio_delay((uint16_t) add_sdio_delay);
  }
}

void check_and_apply_sdio_drive_strength() {
  long sdio_drive_strength = ini_getl("SDIO", "GPIODriveStrength", 0, CONFIGFILE);
  if (sdio_drive_strength) {
    if (sdio_drive_strength < 1 || sdio_drive_strength > 4) {
      log("---- WARNING: GPIODriveStrength setting invalid (Expected 1 - 4), defaulting to setting 2.");
      return;
    }
    log("INFO: Setting SDIO GPIO drive strength level ", (uint16_t)sdio_drive_strength, ".");
    set_sdio_drive_strength(sdio_drive_strength);
  }
}

extern "C" void bluescsi_setup(void)
{
  pio_clear_instruction_memory(pio0);
  pio_clear_instruction_memory(pio1);
  platform_init();

  g_sdcard_present = mountSDCard();

  if(!g_sdcard_present)
  {
    log("SD card init failed, sdErrorCode: ", (int)SD.sdErrorCode(),
           " sdErrorData: ", (int)SD.sdErrorData());

    if (romDriveCheckPresent())
    {
      reinitSCSI();
      if (g_romdrive_active)
      {
        log("Enabled ROM drive without SD card");
        return;
      }
    }

    do
    {
      blinkStatus(BLINK_ERROR_NO_SD_CARD);
      delay(1000);
      platform_reset_watchdog();
      g_sdcard_present = mountSDCard();
    } while (!g_sdcard_present);
    log("SD card init succeeded after retry");
  }

  if (g_sdcard_present)
  {
    platform_late_init();
    if (ini_getbool("SCSI", "InitiatorMode", false, CONFIGFILE))
    {
      platform_enable_initiator_mode();
      if (! ini_getbool("SCSI", "InitiatorParity", true, CONFIGFILE))
      {
        log("Initiator Mode Skipping Parity Check.");
        setInitiatorModeParityCheck(false);
      }
    }
    if (SD.clusterCount() == 0)
    {
      log("SD card without filesystem!");
    }
    check_and_apply_sdio_delay();
    check_and_apply_sdio_drive_strength();

    print_sd_info();
  
    reinitSCSI();
  }

  log(" ");
  log("Initialization complete!");

  if (g_sdcard_present)
  {
    init_logfile();
    if (ini_getbool("SCSI", "DisableStatusLED", false, CONFIGFILE))
    {
      platform_disable_led();
    }
  }

  // Counterpart for the LED_ON in reinitSCSI().
  LED_OFF();
}

void bluescsi_test_loop(void) {
  while(g_test_mode) {
    // Initiator Mode Pin Test First (BSY not asserted)
    SCSI_ENABLE_INITIATOR();
    SCSI_OUT(SEL, 1);
    delay(500);
    SCSI_OUT(SEL, 0);
    SCSI_OUT(ACK, 1);
    delay(500);
    SCSI_OUT(ACK, 0);
    delay(500);
    // ATN not tested here, there's no ATN output
    SCSI_RELEASE_INITIATOR();

    delay(1000);
    // Switch to target mode with BSY_OUT
    SCSI_OUT(BSY, 1);
    SCSI_ENABLE_CONTROL_OUT();
    delay(500);
    SCSI_OUT(IO, 1);
    delay(500);
    SCSI_OUT(IO, 0);
    SCSI_OUT(REQ, 1);
    delay(500);
    SCSI_OUT(REQ, 0);
    SCSI_OUT(CD, 1);
    delay(500);
    SCSI_OUT(CD, 0);
    SCSI_OUT(SEL, 1);
    delay(500);
    SCSI_OUT(SEL, 0);
    SCSI_OUT(MSG, 1);
    delay(500);
    SCSI_OUT(MSG, 0);
    SCSI_OUT(ACK, 1);
    delay(500);
    SCSI_OUT(ACK, 0);
    // SCSI_OUT(ATN, 1);
    // delay(500);
    // SCSI_OUT(ATN, 0);
    delay(500);
    SCSI_OUT(BSY, 0);
    SCSI_RELEASE_OUTPUTS();
  }
}

extern "C" void bluescsi_main_loop(void)
{
  if (unlikely(g_test_mode)) {
    bluescsi_test_loop();
  }

  static uint32_t sd_card_check_time = 0;
  static uint32_t last_request_time = 0;

  platform_reset_watchdog();
  platform_poll();
  diskEjectButtonUpdate(true);
#ifdef BLUESCSI_NETWORK
  platform_network_poll();
#endif
  
#ifdef PLATFORM_HAS_INITIATOR_MODE
  if (unlikely(platform_is_initiator_mode_enabled()))
  {
    scsiInitiatorMainLoop();
    save_logfile();
  }
  else
#endif
  {
    scsiPoll();
    scsiDiskPoll();
    scsiLogPhaseChange(scsiDev.phase);

    // Save log periodically during status phase if there are new messages.
    // In debug mode, also save every 2 seconds if no SCSI requests come in.
    // SD card writing takes a while, during which the code can't handle new
    // SCSI requests, so normally we only want to save during a phase where
    // the host is waiting for us. But for debugging issues where no requests
    // come through or a request hangs, it's useful to force saving of log.
    if (scsiDev.phase == STATUS || (g_log_debug && (uint32_t)(millis() - last_request_time) > 2000))
    {
      save_logfile();
      last_request_time = millis();
    }
  }

  if (g_sdcard_present)
  {
    // Check SD card status for hotplug
    if (scsiDev.phase == BUS_FREE &&
        (uint32_t)(millis() - sd_card_check_time) > 5000)
    {
      sd_card_check_time = millis();
      uint32_t ocr;
      if (!SD.card()->readOCR(&ocr))
      {
        if (!SD.card()->readOCR(&ocr))
        {
          g_sdcard_present = false;
          log("SD card removed, trying to reinit");
        }
      }
    }
  }

  if (!g_sdcard_present)
  {
    // Try to remount SD card
    do 
    {
      g_sdcard_present = mountSDCard();

      if (g_sdcard_present)
      {
        log("SD card reinit succeeded");
        check_and_apply_sdio_delay();
        check_and_apply_sdio_drive_strength();
        print_sd_info();

        reinitSCSI();
        init_logfile();
        LED_OFF();
      }
      else if (!g_romdrive_active)
      {
        blinkStatus(BLINK_ERROR_NO_SD_CARD);
        delay(1000);
        platform_reset_watchdog();
        platform_poll();
      }
    } while (!g_sdcard_present && !g_romdrive_active);
  }
}
