//    Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
//
//    This file is part of SCSI2SD.
//
//    SCSI2SD is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    SCSI2SD is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.

#ifdef STM32F2xx
#include "stm32f2xx.h"
#endif

#ifdef STM32F4xx
#include "stm32f4xx.h"
#endif


#include "config.h"
#include "disk.h"
#include "fpga.h"
#include "hwversion.h"
#include "led.h"
#include "sd.h"
#include "scsi.h"
#include "scsiPhy.h"
#include "time.h"
#include "sdio.h"
#include "usb_device/usb_device.h"
#include "usb_device/usbd_composite.h"
#include "usb_device/usbd_msc_storage_sd.h"

#include "bsp_driver_sd.h"

const char* Notice = "Copyright (C) 2020 Michael McMaster <michael@codesrc.com>";
uint32_t lastSDPoll;
uint32_t lastSDKeepAlive;

static int isUsbStarted;

// Note that the chip clocking isn't fully configured at this stage.
void mainEarlyInit()
{
#ifdef nULPI_RESET_GPIO_Port
    // Disable the ULPI chip
    HAL_GPIO_WritePin(nULPI_RESET_GPIO_Port, nULPI_RESET_Pin, GPIO_PIN_RESET);
#endif

    // Sets up function pointers only
    s2s_initUsbDeviceStorage();
}

void mainInit()
{
    s2s_timeInit();
    s2s_checkHwVersion();

    s2s_ledInit();
    s2s_fpgaInit();

    scsiPhyInit();

    scsiDiskInit();
    sdInit();
    s2s_configInit(&scsiDev.boardCfg);
    scsiPhyConfig();
    scsiInit();

    #ifdef S2S_USB_HS
        // Enable the ULPI chip
        HAL_GPIO_WritePin(nULPI_RESET_GPIO_Port, nULPI_RESET_Pin, GPIO_PIN_SET);
        s2s_delay_ms(5);
    #endif

    MX_USB_DEVICE_Init(); // USB lun config now available.
    isUsbStarted = 1;

    // Optional bootup delay
    int delaySeconds = 0;
    while (delaySeconds < scsiDev.boardCfg.startupDelay) {
        // Keep the USB connection working, otherwise it's very hard to revert
        // silly extra-long startup delay settings.
        int i;
        for (i = 0; i < 200; i++) {
            s2s_delay_ms(5);
            scsiDev.watchdogTick++;
            s2s_configPoll();
        }
        ++delaySeconds;
    }

#ifdef TEST_SD_CARDS
    s2s_ledOn();
    for (int h = 0; h < 1000; ++h)
    {
        uint8_t g = BSP_SD_ReadBlocks_DMA(scsiDev.data, (1000 - h) * 1000, 64);
        if (g != MSD_OK)
        {
            while (1) {}
        }
        if (h & 1)
        {
            uint8_t r = BSP_SD_ReadBlocks_DMA(scsiDev.data, h * 1000, 1);
            if (r != MSD_OK)
            {
                while (1) {}
            }
        }
        else
        {
            uint8_t random[1024];
            for (int p = 0; p < 512; ++p) random[p] = h + p ^ 0xAA;
            BSP_SD_WriteBlocks_DMA(random, h * 2000, 1);
            BSP_SD_ReadBlocks_DMA(scsiDev.data, h * 2000, 1);
            BSP_SD_WriteBlocks_DMA(random, h * 2000 + 1, 2);
            BSP_SD_ReadBlocks_DMA(&(scsiDev.data[512]), h * 2000 + 1, 2);
            if (memcmp(random, scsiDev.data, 512) ||
                memcmp(random, &(scsiDev.data[512]), 1024))
            {
                while (1) {}
            }
        }
    }
    s2s_ledOff();
#endif

    lastSDPoll = lastSDKeepAlive = s2s_getTime_ms();
}

void mainLoop()
{
    scsiDev.watchdogTick++;

    scsiPoll();
    scsiDiskPoll();
    s2s_configPoll();

#ifdef S2S_USB_FS
    int usbBusy = s2s_usbDevicePoll(&hUsbDeviceFS);
#endif
#ifdef S2S_USB_HS
    int usbBusy = s2s_usbDevicePoll(&hUsbDeviceHS);
#endif

#if 0
    sdPoll();
#endif

    // TODO test if USB transfer is in progress
    if (unlikely(scsiDev.phase == BUS_FREE) && !usbBusy)
    {
        if (unlikely(s2s_elapsedTime_ms(lastSDPoll) > 200))
        {
            lastSDPoll = s2s_getTime_ms();
            if (sdInit())
            {
                s2s_configInit(&scsiDev.boardCfg);
                scsiPhyConfig();
                scsiInit();

                if (isUsbStarted)
                {
#ifdef S2S_USB_FS
                    USBD_Stop(&hUsbDeviceFS);
                    s2s_delay_ms(128);
                    USBD_Start(&hUsbDeviceFS);
#endif
#ifdef S2S_USB_HS
                    USBD_Stop(&hUsbDeviceHS);
                    s2s_delay_ms(128);
                    USBD_Start(&hUsbDeviceHS);
#endif
                }
            }
        }
        else if (lastSDKeepAlive > 10000) // 10 seconds
        {
            // 2021 boards fail if there's no commands sent in a while
            sdKeepAlive();
            lastSDKeepAlive = s2s_getTime_ms();
        }
    }
    else if (usbBusy || ((scsiDev.phase >= 0) && (blockDev.state & DISK_PRESENT)))
    {
        // don't waste time scanning SD cards while we're doing disk IO
        lastSDPoll = lastSDKeepAlive = s2s_getTime_ms();
    }
}

