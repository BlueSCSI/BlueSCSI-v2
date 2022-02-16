/*  
 *  AzulSCSI
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
#include <string.h>
#include <ctype.h>
#include "AzulSCSI_config.h"
#include "AzulSCSI_platform.h"
#include "AzulSCSI_log.h"
#include "AzulSCSI_log_trace.h"
#include "AzulSCSI_disk.h"

SdFs SD;
FsFile g_logfile;

/************************************/
/* Status reporting by blinking led */
/************************************/

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
  uint32_t loglen = azlog_get_buffer_len();

  if (loglen != prev_log_len)
  {
    // When debug is off, save log at most every LOG_SAVE_INTERVAL_MS
    // When debug is on, save after every SCSI command.
    if (always || g_azlog_debug || (LOG_SAVE_INTERVAL_MS > 0 && (uint32_t)(millis() - prev_log_save) > LOG_SAVE_INTERVAL_MS))
    {
      g_logfile.write(azlog_get_buffer(&prev_log_pos));
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
  save_logfile(true);

  first_open_after_boot = false;
}

void print_sd_info()
{
  uint64_t size = (uint64_t)SD.vol()->clusterCount() * SD.vol()->bytesPerCluster();
  azlog("SD card detected, FAT", (int)SD.vol()->fatType(),
          " volume size: ", (int)(size / 1024 / 1024), " MB");
  
  cid_t sd_cid;

  if(SD.card()->readCID(&sd_cid))
  {
    azlog("SD MID: ", (uint8_t)sd_cid.mid, ", OID: ", (uint8_t)sd_cid.oid[0], " ", (uint8_t)sd_cid.oid[1]);
    
    char sdname[6] = {sd_cid.pnm[0], sd_cid.pnm[1], sd_cid.pnm[2], sd_cid.pnm[3], sd_cid.pnm[4], 0};
    azlog("SD Name: ", sdname);
    
    char sdyear[5] = {'2', '0', sd_cid.mdt_year_high, sd_cid.mdt_year_low, 0};
    azlog("SD Date: ", (int)sd_cid.mdt_month, "/", sdyear);
    
    azlog("SD Serial: ", sd_cid.psn);
  }
}

/*********************************/
/* Harddisk image file handling  */
/*********************************/

// Iterate over the root path in the SD card looking for candidate image files.
bool findHDDImages()
{
  azlog("Finding HDD images:");

  SdFile root;
  root.open("/");
  SdFile file;
  bool imageReady;
  bool foundImage = false;
  int usedDefaultId = 0;
  while (1) {
    if (!file.openNext(&root, O_READ)) break;
    char name[MAX_FILE_PATH+1];
    if(!file.isDir()) {
      file.getName(name, MAX_FILE_PATH+1);
      file.close();
      if (tolower(name[0]) == 'h' && tolower(name[1]) == 'd') {
        // Defaults for Hard Disks
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        // Positionally read in and coerase the chars to integers.
        // We only require the minimum and read in the next if provided.
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
        if(file_name_length > 3) { // HD0[N]
          int tmp_lun = name[HDIMG_LUN_POS] - '0';

          if(tmp_lun > -1 && tmp_lun < 2) {
            lun = tmp_lun; // If valid id, set it, else use default
          }
        }
        int blk1 = 0, blk2 = 0, blk3 = 0, blk4 = 0;
        if(file_name_length > 8) { // HD00_[111]
          blk1 = name[HDIMG_BLK_POS] - '0';
          blk2 = name[HDIMG_BLK_POS+1] - '0';
          blk3 = name[HDIMG_BLK_POS+2] - '0';
          if(file_name_length > 9) // HD00_NNN[1]
            blk4 = name[HDIMG_BLK_POS+3] - '0';
        }
        if(blk1 == 2 && blk2 == 5 && blk3 == 6) {
          blk = 256;
        } else if(blk1 == 1 && blk2 == 0 && blk3 == 2 && blk4 == 4) {
          blk = 1024;
        } else if(blk1 == 2 && blk2 == 0 && blk3 == 4 && blk4 == 8) {
          blk  = 2048;
        }

        if(id < NUM_SCSIID && lun < NUM_SCSILUN) {
          azlog("-- Trying to open ", name, " for id:", id, " lun:", lun);
          imageReady = scsiDiskOpenHDDImage(id, name, id, lun, blk);
          if(imageReady) { // Marked as a responsive ID
            foundImage = true;
          }
        } else {
          azlog("-- Invalid lun or id for image ", name);
        }
      } else {
        azlog("-- Skipping file ", name);
      }
    }
  }

  if(usedDefaultId > 0) {
    azlog("Some images did not specify a SCSI ID. Last file will be used at ID ", usedDefaultId);
  }
  root.close();

  // Error if there are 0 image files
  if(!foundImage) {
    azlog("ERROR: No valid images found!");
    blinkStatus(BLINK_ERROR_NO_IMAGES);
  }

  // Print SCSI drive map
  for (int i = 0; i < NUM_SCSIID; i++)
  {
    const S2S_TargetCfg* cfg = s2s_getConfigByIndex(i);
    
    if (cfg && cfg->scsiId & S2S_CFG_TARGET_ENABLED)
    {
      int capacity_kB = ((uint64_t)cfg->scsiSectors * cfg->bytesPerSector) / 1024;
      azlog("SCSI ID:", (int)(cfg->scsiId & 7),
            " BlockSize:", (int)cfg->bytesPerSector,
            " Type:", (int)cfg->deviceType,
            " Quirks:", (int)cfg->quirks,
            " ImageSize:", capacity_kB, "kB");
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
  
  if (ini_getbool("SCSI", "Debug", 0, CONFIGFILE))
  {
    g_azlog_debug = true;
  }
}

/*********************************/
/* Main SCSI handling loop       */
/*********************************/

static void reinitSCSI()
{
  readSCSIDeviceConfig();
  bool foundImage = findHDDImages();

  if (foundImage)
  {
    // Ok, there is an image
    blinkStatus(1);
  }

  scsiPhyReset();
  scsiDiskInit();
  scsiInit();
  
}

int main(void)
{
  azplatform_init();

  if(!SD.begin(SD_CONFIG))
  {
    azlog("SD card init failed, sdErrorCode: ", (int)SD.sdErrorCode(),
           " sdErrorData: ", (int)SD.sdErrorData());
    
    do
    {
      blinkStatus(BLINK_ERROR_NO_SD_CARD);
      delay(1000);
    } while (!SD.begin(SD_CONFIG));
    azlog("SD card init succeeded after retry");
  }

  print_sd_info();
  
  reinitSCSI();

  azlog("Initialization complete!");
  azlog("Platform: ", g_azplatform_name);
  azlog("FW Version: ", g_azlog_firmwareversion);

  init_logfile();
  
  uint32_t sd_card_check_time = 0;

  while (1)
  {
    azplatform_reset_watchdog(15000);
    scsiPoll();
    scsiDiskPoll();
    scsiLogPhaseChange(scsiDev.phase);

    // Save log periodically during status phase if there are new messages.
    if (scsiDev.phase == STATUS)
    {
      save_logfile();
    }

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
          azlog("SD card removed, trying to reinit");
          do
          {
            blinkStatus(BLINK_ERROR_NO_SD_CARD);
            delay(1000);
          } while (!SD.begin(SD_CONFIG));
          azlog("SD card reinit succeeded");
          print_sd_info();
          
          reinitSCSI();
          init_logfile();
        }
      }
    }
  }
}
