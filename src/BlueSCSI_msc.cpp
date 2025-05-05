/**
 * Copyright (c) 2023,2024 zigzagjoe
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
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_msc.h"
#include "BlueSCSI_settings.h"

// external global SD variable
extern SdFs SD;

// SCSI system and device settings
extern BlueSCSISettings g_scsi_settings;

// public globals
volatile MSC_LEDState MSC_LEDMode;

// card reader operation loop
// assumption that SD card was enumerated and is working
void bluescsi_msc_loop() {

  // turn LED on to indicate entering card reader mode.
  LED_ON();
  
  logmsg("Entering USB Mass storage mode. Eject the USB disk(s) to exit.");
  
  // force blink status to inactive (may inhibit our blinking otherwise)
  platform_set_blink_status(0);

  // pull in the setting for presenting images as USB devices
  platform_set_msc_image_mode(g_scsi_settings.getSystem()->usbMassStoragePresentImages);

  platform_enter_msc();
  
  uint32_t sd_card_check_time = 0;
  uint16_t syncCounter = 0;
        
  // steady state operation / indication loop
  // led remains steady on
  while(platform_run_msc()) {
    platform_reset_watchdog(); // also sends log to USB serial

    if ((uint32_t)(millis() - sd_card_check_time) > 5000) {
      sd_card_check_time = millis();
      uint32_t ocr;
      if (!SD.card()->readOCR(&ocr)) {
        if (!SD.card()->readOCR(&ocr)) {
          logmsg("SD card presence check failed! Card unexpectedly removed?");
          break;
        }
      }
    }
 
    // blink LED according to access type
    switch (MSC_LEDMode) {
      case LED_BLINK_FAST:
        LED_OFF();
        delay(30);
        break;
      case LED_BLINK_SLOW:
        delay(30);
        LED_OFF();
        delay(100);
        syncCounter = 1;
        break;
      default:
        // sync sd card ~ 500ms after writes stop
        if (syncCounter && (++syncCounter > 8)) {
          syncCounter = 0;
          SD.card()->syncDevice();
        }
    }

    // LED always on in card reader mode
    MSC_LEDMode = LED_SOLIDON;
	  LED_ON(); 
    delay(30);
  }

  // turn the LED off to indicate exiting MSC
  LED_OFF();
  
  logmsg("USB Mass Storage mode exited: resuming standard functionality.");
  platform_exit_msc();
  
  SD.card()->syncDevice();

  // leave the LED off for a moment, before any blinks from the main firmware occur
  delay(1000);
}

#endif