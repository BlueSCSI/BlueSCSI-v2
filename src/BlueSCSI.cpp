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
#include <zip_parser.h>
#include "BlueSCSI_config.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_settings.h"
#include "BlueSCSI_disk.h"
#include "BlueSCSI_initiator.h"
#include "BlueSCSI_msc_initiator.h"
#include "BlueSCSI_msc.h"
#include "BlueSCSI_blink.h"
#include "ROMDrive.h"

SdFs SD;
FsFile g_logfile;
bool g_rawdrive_active;
static bool g_romdrive_active;
bool g_sdcard_present;

#ifndef SD_SPEED_CLASS_WARN_BELOW
#define SD_SPEED_CLASS_WARN_BELOW 10
#endif



/**************/
/* Log saving */
/**************/

void save_logfile(bool always = false)
{
#ifdef BLUESCSI_HARDWARE_CONFIG
  // Disable logging to the SD card when in direct mode
  if (g_hw_config.is_active())
    return;
#endif

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
#ifdef BLUESCSI_HARDWARE_CONFIG
  // Disable logging to the SD card when in direct mode
  if (g_hw_config.is_active())
    return;
#endif

  if (g_rawdrive_active)
    return;

  static bool first_open_after_boot = true;

  bool truncate = first_open_after_boot;
  int flags = O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND);
  g_logfile = SD.open(LOGFILE, flags);
  if (!g_logfile.isOpen())
  {
    logmsg("Failed to open log file: ", SD.sdErrorCode());
  }
  save_logfile(true);

  first_open_after_boot = false;
}

void print_sd_info()
{
  uint64_t size = (uint64_t)SD.vol()->clusterCount() * SD.vol()->bytesPerCluster();
  logmsg("SD card detected, FAT", (int)SD.vol()->fatType(),
          " volume size: ", (int)(size / 1024 / 1024), " MB");

  cid_t sd_cid;

  if(SD.card()->readCID(&sd_cid))
  {
    logmsg("SD MID: ", (uint8_t)sd_cid.mid, ", OID: ", (uint8_t)sd_cid.oid[0], " ", (uint8_t)sd_cid.oid[1]);

    char sdname[6] = {sd_cid.pnm[0], sd_cid.pnm[1], sd_cid.pnm[2], sd_cid.pnm[3], sd_cid.pnm[4], 0};
    logmsg("SD Name: ", sdname);
    logmsg("SD Date: ", (int)sd_cid.mdtMonth(), "/", sd_cid.mdtYear());
    logmsg("SD Serial: ", sd_cid.psn());
  }

  sds_t sds = {0};
  if (SD.card()->readSDS(&sds) && sds.speedClass() < SD_SPEED_CLASS_WARN_BELOW)
  {
    logmsg("-- WARNING: Your SD Card Speed Class is ", (int)sds.speedClass(), ". Class ", (int) SD_SPEED_CLASS_WARN_BELOW," or better is recommended for best performance.");
  }

}

/*********************************/
/* Harddisk image file handling  */
/*********************************/

// When a file is called e.g. "Create_1024M_HD40.txt",
// create image file with specified size.
// Returns true if image file creation succeeded.
//
// Parsing rules:
// - Filename must start with "Create", case-insensitive
// - Separator can be either underscore, dash or space
// - Size must start with a number. Unit of k, kb, m, mb, g, gb is supported,
//   case-insensitive, with 1024 as the base. If no unit, assume MB.
// - If target filename does not have extension (just .txt), use ".bin"
bool createImage(const char *cmd_filename, char imgname[MAX_FILE_PATH + 1])
{
  if (strncasecmp(cmd_filename, CREATEFILE, strlen(CREATEFILE)) != 0)
  {
    return false;
  }

  const char *p = cmd_filename + strlen(CREATEFILE);

  // Skip separator if any
  while (isspace(*p) || *p == '-' || *p == '_')
  {
    p++;
  }

  char *unit = nullptr;
  uint64_t size = strtoul(p, &unit, 10);

  if (size <= 0 || unit <= p)
  {
    logmsg("---- Could not parse size in filename '", cmd_filename, "'");
    return false;
  }

  // Parse k/M/G unit
  char unitchar = tolower(*unit);
  if (unitchar == 'k')
  {
    size *= 1024;
    p = unit + 1;
  }
  else if (unitchar == 'm')
  {
    size *= 1024 * 1024;
    p = unit + 1;
  }
  else if (unitchar == 'g')
  {
    size *= 1024 * 1024 * 1024;
    p = unit + 1;
  }
  else
  {
    size *= 1024 * 1024;
    p = unit;
  }

  // Skip i and B if part of unit
  if (tolower(*p) == 'i') p++;
  if (tolower(*p) == 'b') p++;

  // Skip separator if any
  while (isspace(*p) || *p == '-' || *p == '_')
  {
    p++;
  }

  // Copy target filename to new buffer
  strncpy(imgname, p, MAX_FILE_PATH);
  imgname[MAX_FILE_PATH] = '\0';
  int namelen = strlen(imgname);

  // Strip .txt extension if any
  if (namelen >= 4 && strncasecmp(imgname + namelen - 4, ".txt", 4) == 0)
  {
    namelen -= 4;
    imgname[namelen] = '\0';
  }

  // Add .bin if no extension
  if (!strchr(imgname, '.') && namelen < MAX_FILE_PATH - 4)
  {
    namelen += 4;
    strcat(imgname, ".bin");
  }

  // Check if file exists
  if (namelen <= 5 || SD.exists(imgname))
  {
    logmsg("---- Image file already exists, skipping '", cmd_filename, "'");
    return false;
  }

  // Create file, try to preallocate contiguous sectors
  LED_ON();
  FsFile file = SD.open(imgname, O_WRONLY | O_CREAT);

  if (!file.preAllocate(size))
  {
    logmsg("---- Preallocation didn't find contiguous set of clusters, continuing anyway");
  }

  // Write zeros to fill the file
  uint32_t start = millis();
  memset(scsiDev.data, 0, sizeof(scsiDev.data));
  uint64_t remain = size;
  while (remain > 0)
  {
    if (millis() & 128) { LED_ON(); } else { LED_OFF(); }
    platform_reset_watchdog();

    size_t to_write = sizeof(scsiDev.data);
    if (to_write > remain) to_write = remain;
    if (file.write(scsiDev.data, to_write) != to_write)
    {
      logmsg("---- File writing to '", imgname, "' failed with ", (int)remain, " bytes remaining");
      file.close();
      LED_OFF();
      return false;
    }

    remain -= to_write;
  }

  file.close();
  uint32_t time = millis() - start;
  int kb_per_s = size / time;
  logmsg("---- Image creation successful, write speed ", kb_per_s, " kB/s, removing '", cmd_filename, "'");
  SD.remove(cmd_filename);

  LED_OFF();
  return true;
}

static bool typeIsRemovable(S2S_CFG_TYPE type)
{
  switch (type)
  {
  case S2S_CFG_OPTICAL:
  case S2S_CFG_MO:
  case S2S_CFG_FLOPPY_14MB:
  case S2S_CFG_ZIP100:
  case S2S_CFG_REMOVABLE:
  case S2S_CFG_SEQUENTIAL:
    return true;
  default:
    return false;
  }
}

// Iterate over the root path in the SD card looking for candidate image files.
bool findHDDImages()
{
#ifdef BLUESCSI_HARDWARE_CONFIG
  if (g_hw_config.is_active())
  {
    return false;
  }
#endif // BLUESCSI_HARDWARE_CONFIG
  char imgdir[MAX_FILE_PATH];
  ini_gets("SCSI", "Dir", "/", imgdir, sizeof(imgdir), CONFIGFILE);
  int dirindex = 0;

  logmsg("Finding images in directory ", imgdir, ":");

  FsFile root;
  root.open(imgdir);
  if (!root.isOpen())
  {
    logmsg("Could not open directory: ", imgdir);
  }

  FsFile file;
  bool imageReady;
  bool foundImage = false;
  int usedDefaultId = 0;
  uint8_t removable_count = 0;
  uint8_t eject_btn_set = 0;
  uint8_t last_removable_device = 255;
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
        logmsg("Finding images in additional directory Dir", (int)dirindex, " = \"", imgdir, "\":");
        root.open(imgdir);
        if (!root.isOpen())
        {
          logmsg("-- Could not open directory: ", imgdir);
        }
        continue;
      }
      else
      {
        break;
      }
    }

    char name[MAX_FILE_PATH+1];
    if(!file.isDir() || scsiDiskFolderContainsCueSheet(&file) || scsiDiskFolderIsTapeFolder(&file)) {
      file.getName(name, MAX_FILE_PATH+1);
      file.close();

      // Special filename for clearing any previously programmed ROM drive
      if(strcasecmp(name, "CLEAR_ROM") == 0)
      {
        logmsg("-- Special filename: '", name, "'");
        romDriveClear();
        continue;
      }

      // Special filename for creating new empty image files
      if (strncasecmp(name, CREATEFILE, strlen(CREATEFILE)) == 0)
      {
        logmsg("-- Special filename: '", name, "'");
        char imgname[MAX_FILE_PATH+1];
        if (createImage(name, imgname))
        {
          // Created new image file, use its name instead of the name of the command file
          strncpy(name, imgname, MAX_FILE_PATH);
          name[MAX_FILE_PATH] = '\0';
        }
      }
      bool use_prefix = false;
      bool is_hd = (tolower(name[0]) == 'h' && tolower(name[1]) == 'd');
      bool is_cd = (tolower(name[0]) == 'c' && tolower(name[1]) == 'd');
      bool is_fd = (tolower(name[0]) == 'f' && tolower(name[1]) == 'd');
      bool is_mo = (tolower(name[0]) == 'm' && tolower(name[1]) == 'o');
      bool is_re = (tolower(name[0]) == 'r' && tolower(name[1]) == 'e');
      bool is_tp = (tolower(name[0]) == 't' && tolower(name[1]) == 'p');
      bool is_zp = (tolower(name[0]) == 'z' && tolower(name[1]) == 'p');
#ifdef BLUESCSI_NETWORK
      bool is_ne = (tolower(name[0]) == 'n' && tolower(name[1]) == 'e');
#endif // BLUESCSI_NETWORK

      if (is_hd || is_cd || is_fd || is_mo || is_re || is_tp || is_zp
#ifdef BLUESCSI_NETWORK
        || is_ne
#endif // BLUESCSI_NETWORK
      )
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

        // Parse SCSI device ID
        int file_name_length = strlen(name);
        if(file_name_length > 2) { // HD[N]
          int tmp_id = name[HDIMG_ID_POS] - '0';

          if(tmp_id > -1 && tmp_id < 8)
          {
            id = tmp_id; // If valid id, set it, else use default
            use_prefix = true;
          }
          else
          {
            id = usedDefaultId++;
          }
        }

        // Parse SCSI LUN number
        if(file_name_length > 3) { // HD0[N]
          int tmp_lun = name[HDIMG_LUN_POS] - '0';

          if(tmp_lun > -1 && tmp_lun < NUM_SCSILUN) {
            lun = tmp_lun; // If valid id, set it, else use default
          }
        }

        // Add the directory name to get the full file path
        char fullname[MAX_FILE_PATH * 2 + 2] = {0};
        strncpy(fullname, imgdir, MAX_FILE_PATH);
        if (fullname[strlen(fullname) - 1] != '/') strcat(fullname, "/");
        strcat(fullname, name);

        // Check whether this SCSI ID has been configured yet
        if (s2s_getConfigById(id))
        {
          logmsg("-- Ignoring ", fullname, ", SCSI ID ", id, " is already in use!");
          continue;
        }

        // set the default block size now that we know the device type
        if (g_scsi_settings.getDevice(id)->blockSize == 0)
        {
          g_scsi_settings.getDevice(id)->blockSize = is_cd ?  DEFAULT_BLOCKSIZE_OPTICAL : DEFAULT_BLOCKSIZE;
        }
        int blk = getBlockSize(name, id);

#ifdef BLUESCSI_NETWORK
        if (is_ne && !platform_network_supported())
        {
          logmsg("-- Ignoring ", fullname, ", networking is not supported on this hardware");
          continue;
        }
#endif // BLUESCSI_NETWORK
        // Type mapping based on filename.
        // If type is FIXED, the type can still be overridden in .ini file.
        S2S_CFG_TYPE type = S2S_CFG_FIXED;
        if (is_cd) type = S2S_CFG_OPTICAL;
        if (is_fd) type = S2S_CFG_FLOPPY_14MB;
        if (is_mo) type = S2S_CFG_MO;
#ifdef BLUESCSI_NETWORK
        if (is_ne) type = S2S_CFG_NETWORK;
#endif // BLUESCSI_NETWORK
        if (is_re) type = S2S_CFG_REMOVABLE;
        if (is_tp) type = S2S_CFG_SEQUENTIAL;
        if (is_zp) type = S2S_CFG_ZIP100;

        g_scsi_settings.initDevice(id & 7, type);
        // Open the image file
        if (id < NUM_SCSIID && is_romdrive)
        {
          logmsg("-- Loading ROM drive from ", fullname, " for id:", id);
          imageReady = scsiDiskProgramRomDrive(fullname, id, blk, type);
          if (imageReady)
          {
            foundImage = true;
          }
        }
        else if(id < NUM_SCSIID && lun < NUM_SCSILUN) {
          logmsg("-- Opening ", fullname, " for id:", id, " lun:", lun);

          if (g_scsi_settings.getDevicePreset(id) != DEV_PRESET_NONE)
          {
              logmsg("---- Using device preset: ", g_scsi_settings.getDevicePresetName(id));
          }

          imageReady = scsiDiskOpenHDDImage(id, fullname, lun, blk, type, use_prefix);
          if(imageReady)
          {
            foundImage = true;
          }
          else
          {
            logmsg("---- Failed to load image");
          }
        } else {
          logmsg("-- Invalid lun or id for image ", fullname);
        }
      }
    }
  }

  if(usedDefaultId > 0) {
    logmsg("Some images did not specify a SCSI ID. Last file will be used at ID ", usedDefaultId);
  }
  root.close();

  g_romdrive_active = scsiDiskActivateRomDrive();

  // Print SCSI drive map
  for (int i = 0; i < NUM_SCSIID; i++)
  {
    const S2S_TargetCfg* cfg = s2s_getConfigByIndex(i);

    if (cfg && (cfg->scsiId & S2S_CFG_TARGET_ENABLED))
    {
      int capacity_kB = ((uint64_t)cfg->scsiSectors * cfg->bytesPerSector) / 1024;

      if (cfg->deviceType == S2S_CFG_NETWORK)
      {
        logmsg("SCSI ID: ", (int)(cfg->scsiId & 7),
              ", Type: ", (int)cfg->deviceType,
              ", Quirks: ", (int)cfg->quirks);
      }
      else
      {
        logmsg("SCSI ID: ", (int)(cfg->scsiId & S2S_CFG_TARGET_ID_BITS),
              ", BlockSize: ", (int)cfg->bytesPerSector,
              ", Type: ", (int)cfg->deviceType,
              ", Quirks: ", (int)cfg->quirks,
              ", Size: ", capacity_kB, "kB",
              typeIsRemovable((S2S_CFG_TYPE)cfg->deviceType) ? ", Removable" : ""
              );
       }
    }
  }
  // count the removable drives and drive with eject enabled
  for (uint8_t id = 0; id < S2S_MAX_TARGETS; id++)
  {
    const S2S_TargetCfg* cfg = s2s_getConfigByIndex(id);
    if (cfg  && (cfg->scsiId & S2S_CFG_TARGET_ENABLED ))
    {
       if (typeIsRemovable((S2S_CFG_TYPE)cfg->deviceType))
        {
          removable_count++;
          last_removable_device = id;
          if ( getEjectButton(id) !=0 )
          {
            eject_btn_set++;
          }
        }
    }
  }

  if (removable_count == 1)
  {
    // If there is a removable device
    if (eject_btn_set == 1)
      logmsg("Eject set to device with ID: ", last_removable_device);
    else if (eject_btn_set == 0 && !platform_has_phy_eject_button())
    {
      logmsg("Found 1 removable device, to set an eject button see EjectButton in the '", CONFIGFILE,"', or the http://bluescsi.com/manual");
    }
  }
  else if (removable_count > 1)
  {

    if (removable_count >= eject_btn_set && eject_btn_set > 0)
    {
      if (eject_btn_set == removable_count)
        logmsg("Eject set on all removable devices:");
      else
        logmsg("Eject set on the following SCSI IDs:");

      for (uint8_t id = 0; id < S2S_MAX_TARGETS; id++)
      {
        if( getEjectButton(id) != 0)
        {
          logmsg("-- SCSI ID: ", (int)id, " type: ", (int) s2s_getConfigById(id)->deviceType, " button mask: ", getEjectButton(id));
        }
      }
    }
    else
    {
      logmsg("Multiple removable devices, to set an eject button see EjectButton in the '", CONFIGFILE,"', or the http://bluescsi.com/manual");
    }
  }
  return foundImage;
}

/************************/
/* Config file loading  */
/************************/

void readSCSIDeviceConfig()
{
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
#if defined(HAS_SDIO_CLASS) && HAS_SDIO_CLASS
    int speed = ((SdioCard*)SD.card())->kHzSdClk();
    if (speed > 0)
    {
      logmsg("SD card communication speed: ",
        (int)((speed + 500) / 1000), " MHz, ",
        (int)((speed + 1000) / 2000), " MB/s");
    }
#endif

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
#if defined(BLUESCSI_HARDWARE_CONFIG)
  if (!g_hw_config.is_active() && ini_getbool("SCSI", "Debug", 0, CONFIGFILE))
  {
    g_log_debug = true;
  }
#else
  if (ini_getbool("SCSI", "Debug", 0, CONFIGFILE))
  {
    g_log_debug = true;
  }
#endif
  if (g_log_debug)
  {
    g_scsi_log_mask = ini_getl("SCSI", "DebugLogMask", 0xFF, CONFIGFILE) & 0xFF;
    if (g_scsi_log_mask == 0)
    {
      dbgmsg("DebugLogMask set to 0x00, this will silence all debug messages when a SCSI ID has been selected");
    }
    else if (g_scsi_log_mask != 0xFF)
    {
      dbgmsg("DebugLogMask set to ", (uint8_t) g_scsi_log_mask, " only SCSI ID's matching the bit mask will be logged");
    }

    g_log_ignore_busy_free = ini_getbool("SCSI", "DebugIgnoreBusyFree", 0, CONFIGFILE);
    if (g_log_ignore_busy_free)
    {
      dbgmsg("DebugIgnoreBusyFree enabled, BUS_FREE/BUS_BUSY messages suppressed");
    }
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
#if defined(BLUESCSI_HARDWARE_CONFIG)
  if (g_hw_config.is_active())
  {
    bool success;
    uint8_t scsiId = g_hw_config.scsi_id();
    g_scsi_settings.initDevice(scsiId, g_hw_config.device_type());

    logmsg("Direct/Raw mode enabled, using hardware switches for configuration");
    logmsg("-- SCSI ID set via DIP switch to ", (int) g_hw_config.scsi_id());
    char raw_filename[32];
    uint32_t start =  g_scsi_settings.getDevice(scsiId)->sectorSDBegin;
    uint32_t end = g_scsi_settings.getDevice(scsiId)->sectorSDEnd;

    if (start == end && end == 0)
    {
      strcpy(raw_filename, "RAW:0:0xFFFFFFFF");
    }
    else
    {
      snprintf(raw_filename, sizeof(raw_filename), "RAW:0x%X:0x%X", start, end);
    }

    success = scsiDiskOpenHDDImage(scsiId, raw_filename, 0,
                                   g_hw_config.blocksize(), g_hw_config.device_type());
    if (success)
    {
      if (g_scsi_settings.getDevicePreset(scsiId) != DEV_PRESET_NONE)
      {
        logmsg("---- Using device preset: ", g_scsi_settings.getDevicePresetName(scsiId));
      }
    }
    blinkStatus(BLINK_DIRECT_MODE);
  }
  else
#endif // BLUESCSI_HARDWARE_CONFIG
  {
    readSCSIDeviceConfig();
    findHDDImages();

    // Error if there are 0 image files
    if (!scsiDiskCheckAnyImagesConfigured())
    {
  #ifdef RAW_FALLBACK_ENABLE
      logmsg("No images found, enabling RAW fallback partition");
      g_scsi_settings.initDevice(RAW_FALLBACK_SCSI_ID, S2S_CFG_FIXED);
      scsiDiskOpenHDDImage(RAW_FALLBACK_SCSI_ID, "RAW:0:0xFFFFFFFF", 0,
                          RAW_FALLBACK_BLOCKSIZE);
  #else
      logmsg("No valid image files found!");
  #endif // RAW_FALLBACK_ENABLE
      blinkStatus(BLINK_ERROR_NO_IMAGES);
    }
  }

  scsiPhyReset();
  scsiDiskInit();
  scsiInit();

#ifdef BLUESCSI_NETWORK
  if (scsiDiskCheckAnyNetworkDevicesConfigured() && platform_network_supported())
  {
    platform_network_init(scsiDev.boardCfg.wifiMACAddress);
    if (scsiDev.boardCfg.wifiSSID[0] != '\0')
      platform_network_wifi_join(scsiDev.boardCfg.wifiSSID, scsiDev.boardCfg.wifiPassword);
  }
  else
  {
    platform_network_deinit();
  }
#endif // BLUESCSI_NETWORK

}

// Alert user that update bin file not used
static void check_for_unused_update_files()
{
  FsFile root = SD.open("/");
  FsFile file;
  char filename[MAX_FILE_PATH + 1];
  bool bin_files_found = false;
  while (file.openNext(&root, O_RDONLY))
  {
    if (!file.isDir())
    {
      size_t filename_len = file.getName(filename, sizeof(filename));
      if (strncasecmp(filename, "bluescsi", sizeof("bluescsi" - 1)) == 0 &&
          strncasecmp(filename + filename_len - 4, ".bin", 4) == 0)
      {
        bin_files_found = true;
        logmsg("Firmware update file \"", filename, "\" does not contain the board model string \"", FIRMWARE_NAME_PREFIX, "\"");
      }
    }
  }
  if (bin_files_found)
  {
    logmsg("Please use the ", FIRMWARE_PREFIX ,"*.zip firmware bundle, or the proper .bin or .uf2 file to update the firmware.");
    logmsg("See http://bluescsi.com/manual for more information");
  }
}

// Update firmware by unzipping the firmware package
static void firmware_update()
{
  const char firmware_prefix[] = FIRMWARE_PREFIX;
  FsFile root = SD.open("/");
  FsFile file;
  char name[MAX_FILE_PATH + 1];
  while (1)
  {
    if (!file.openNext(&root, O_RDONLY))
    {
      file.close();
      root.close();
      return;
    }
    if (file.isDir())
      continue;

    file.getName(name, sizeof(name));
    if (strlen(name) + 1 < sizeof(firmware_prefix))
      continue;
    if ( strncasecmp(firmware_prefix, name, sizeof(firmware_prefix) -1) == 0)
    {
      break;
    }
  }

  logmsg("Found firmware package ", name);
  // example fixed length at the end of the filename
  const uint32_t postfix_filename_length = sizeof("_2025-02-21_e4be9ed.bin") - 1;
  const uint32_t target_filename_length = sizeof(FIRMWARE_NAME_PREFIX) - 1 + postfix_filename_length;
  zipparser::Parser parser = zipparser::Parser(FIRMWARE_NAME_PREFIX, sizeof(FIRMWARE_NAME_PREFIX) - 1, target_filename_length);
  uint8_t buf[512];
  int32_t parsed_length;
  int bytes_read = 0;
  while ((bytes_read = file.read(buf, sizeof(buf))) > 0)
  {
    parsed_length = parser.Parse(buf, bytes_read);
    if (parsed_length == sizeof(buf))
       continue;
    if (parsed_length >= 0)
    {
      if (!parser.FoundMatch())
      {
        parser.Reset();
        file.seekSet(file.position() - (sizeof(buf) - parsed_length) + parser.GetCompressedSize());
      }
      else
      {
        // seek to start of data in matching file
        file.seekSet(file.position() - (sizeof(buf) - parsed_length));
        break;
      }
    }
    if (parsed_length < 0)
    {
      logmsg("Filename character length of ", (int)target_filename_length , " with a prefix of ", FIRMWARE_NAME_PREFIX, " not found in ", name);
      file.close();
      root.close();
      return;
    }
  }


  if (parser.FoundMatch())
  {

    logmsg("Unzipping matching firmware with prefix: ", FIRMWARE_NAME_PREFIX);
    FsFile target_firmware;
    char firmware_name[64] = {0};
    memcpy(firmware_name, FIRMWARE_NAME_PREFIX, sizeof(FIRMWARE_NAME_PREFIX) - 1);
    memcpy(firmware_name + sizeof(FIRMWARE_NAME_PREFIX) - 1, ".bin", sizeof(".bin"));
    target_firmware.open(&root, firmware_name, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC);
    uint32_t position = 0;
    while ((bytes_read = file.read(buf, sizeof(buf))) > 0)
    {
      if (bytes_read > parser.GetCompressedSize() - position)
        bytes_read =  parser.GetCompressedSize() - position;
      target_firmware.write(buf, bytes_read);
      position += bytes_read;
      if (position >= parser.GetCompressedSize())
      {
        break;
      }
    }
    // zip file has a central directory at the end of the file,
    // so the compressed data should never hit the end of the file
    // so bytes read should always be greater than 0 for a valid datastream
    if (bytes_read > 0)
    {
      target_firmware.close();
      file.close();
      root.remove(name);
      root.close();
      logmsg("Update extracted from package, rebooting MCU");
      platform_reset_mcu();
    }
    else
    {
      target_firmware.close();
      logmsg("Error reading firmware package file");
      root.remove(firmware_name);
    }
  }
  file.close();
  root.close();
}

// Checks if SD card is still present
static bool poll_sd_card()
{
#ifdef SD_USE_SDIO
  return SD.card()->status() != 0 && SD.card()->errorCode() == 0;
#else
  uint32_t ocr;
  return SD.card()->readOCR(&ocr);
#endif
}

static void init_eject_button()
{
  if (platform_has_phy_eject_button() &&  !g_scsi_settings.isEjectButtonSet())
  {
    for (uint8_t i = 0; i < S2S_MAX_TARGETS; i++)
    {
      S2S_CFG_TYPE dev_type = (S2S_CFG_TYPE)scsiDev.targets[i].cfg->deviceType;
      if (dev_type == S2S_CFG_OPTICAL
          ||dev_type == S2S_CFG_ZIP100
          || dev_type == S2S_CFG_REMOVABLE
          || dev_type == S2S_CFG_FLOPPY_14MB
          || dev_type == S2S_CFG_MO
          || dev_type == S2S_CFG_SEQUENTIAL
      )
      {
          setEjectButton(i, 1);
          logmsg("Setting hardware eject button to the first ejectable device on SCSI ID ", (int)i);
          break;
      }
    }
  }
}

// Place all the setup code that requires the SD card to be initialized here
// Which is pretty much everything after platform_init and and platform_late_init
static void bluescsi_setup_sd_card(bool wait_for_card = true)
{
  g_sdcard_present = mountSDCard();

  if(!g_sdcard_present)
  {
    if (SD.sdErrorCode() == platform_no_sd_card_on_init_error_code())
    {
  #ifdef PLATFORM_HAS_INITIATOR_MODE
      if (platform_is_initiator_mode_enabled())
      {
        logmsg("No SD card detected, imaging to SD card not possible");
      }
      else
  #endif
      {
        logmsg("No SD card detected, please check SD card slot to make sure it is in correctly");
      }
    }
    dbgmsg("SD card init failed, sdErrorCode: ", (int)SD.sdErrorCode(),
           " sdErrorData: ", (int)SD.sdErrorData());

    if (romDriveCheckPresent())
    {
      reinitSCSI();
      if (g_romdrive_active)
      {
        logmsg("Enabled ROM drive without SD card");
        return;
      }
    }

    do
    {
      blinkStatus(BLINK_ERROR_NO_SD_CARD);
      platform_reset_watchdog();
      g_sdcard_present = mountSDCard();
    } while (!g_sdcard_present && wait_for_card);
    blink_cancel();
    LED_OFF();

    if (g_sdcard_present)
    {
      logmsg("SD card init succeeded after retry");
    }
    else
    {
      logmsg("Continuing without SD card");
    }
  }
  check_for_unused_update_files();
  firmware_update();



  if (g_sdcard_present)
  {


    if (SD.clusterCount() == 0)
    {
      logmsg("SD card without filesystem!");
    }

    print_sd_info();

    char presetName[32];
    ini_gets("SCSI", "System", "", presetName, sizeof(presetName), CONFIGFILE);
    scsi_system_settings_t *cfg = g_scsi_settings.initSystem(presetName);

#ifdef RECLOCKING_SUPPORTED
    bluescsi_speed_grade_t speed_grade = (bluescsi_speed_grade_t) g_scsi_settings.getSystem()->speedGrade;
    if (speed_grade != bluescsi_speed_grade_t::SPEED_GRADE_DEFAULT)
    { 
      logmsg("Speed grade set to ", g_scsi_settings.getSpeedGradeString(), " reclocking system");
      if (platform_reclock(speed_grade))
      {
        logmsg("======== Reinitializing BlueSCSI after reclock ========");
        g_sdcard_present = mountSDCard();
      }
    }
    else
    {
#ifndef ENABLE_AUDIO_OUTPUT // if audio is enabled, skip message because reclocking ocurred earlier
      logmsg("Speed grade set to Default, skipping reclocking");
#endif
    }
#endif

    int boot_delay_ms = cfg->initPreDelay;
    if (boot_delay_ms > 0)
    {
      logmsg("Pre SCSI init boot delay in millis: ", boot_delay_ms);
      delay(boot_delay_ms);
    }
    platform_post_sd_card_init();
    reinitSCSI();

    boot_delay_ms = cfg->initPostDelay;
    if (boot_delay_ms > 0)
    {
      logmsg("Post SCSI init boot delay in millis: ", boot_delay_ms);
      delay(boot_delay_ms);
    }

  }

  if (g_sdcard_present)
  {
    init_logfile();
    if (ini_getbool("SCSI", "DisableStatusLED", false, CONFIGFILE))
    {
      platform_disable_led();
    }
  }
#ifdef PLATFORM_HAS_INITIATOR_MODE
  if (!platform_is_initiator_mode_enabled())
#endif
  {
    init_eject_button();
  }

  blinkStatus(BLINK_STATUS_OK);
}

extern "C" void bluescsi_setup(void)
{
  platform_init();
  platform_late_init();

  bool is_initiator = false;
#ifdef PLATFORM_HAS_INITIATOR_MODE
  is_initiator = platform_is_initiator_mode_enabled();
#endif

  bluescsi_setup_sd_card(!is_initiator);

#ifdef PLATFORM_MASS_STORAGE
  static bool check_mass_storage = true;
  if ((check_mass_storage || platform_rebooted_into_mass_storage()) && !is_initiator)
  {
    if (g_scsi_settings.getSystem()->enableUSBMassStorage
       || g_scsi_settings.getSystem()->usbMassStoragePresentImages
    )
    {
      bluescsi_msc_loop();
      logmsg("Re-processing filenames and bluescsi.ini config parameters");
      bluescsi_setup_sd_card();
    }
  }
#endif
  logmsg("Clock set to: ", (int) platform_sys_clock_in_hz(), "Hz");
  logmsg("Initialization complete!");
}

extern "C" void bluescsi_main_loop(void)
{
  static uint32_t sd_card_check_time = 0;
  static uint32_t last_request_time = 0;

  bool is_initiator = false;
#ifdef PLATFORM_HAS_INITIATOR_MODE
  is_initiator = platform_is_initiator_mode_enabled();
#endif

  platform_reset_watchdog();
  platform_poll();
  diskEjectButtonUpdate(true);
  blink_poll();

#ifdef BLUESCSI_NETWORK
  platform_network_poll();
#endif // BLUESCSI_NETWORK

#ifdef PLATFORM_HAS_INITIATOR_MODE
  if (is_initiator)
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
        (uint32_t)(millis() - sd_card_check_time) > SDCARD_POLL_INTERVAL)
    {
      sd_card_check_time = millis();
      if (!poll_sd_card())
      {
        if (!poll_sd_card())
        {
          g_sdcard_present = false;
          logmsg("SD card removed, trying to reinit");
        }
      }
    }
  }

  if (!g_sdcard_present && (uint32_t)(millis() - sd_card_check_time) > SDCARD_POLL_INTERVAL
      && !g_msc_initiator)
  {
    sd_card_check_time = millis();

    // Try to remount SD card
    do
    {
      g_sdcard_present = mountSDCard();

      if (g_sdcard_present)
      {
        blink_cancel();
        LED_OFF();
        logmsg("SD card reinit succeeded");
        print_sd_info();
        reinitSCSI();
        init_logfile();
        blinkStatus(BLINK_STATUS_OK);
      }
      else if (!g_romdrive_active)
      {
        blinkStatus(BLINK_ERROR_NO_SD_CARD);
        platform_reset_watchdog();
        platform_poll();
      }
    } while (!g_sdcard_present && !g_romdrive_active && !is_initiator);
  }
}
