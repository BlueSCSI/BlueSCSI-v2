/*!
    \file    usb_conf.h
    \brief   USB core driver basic configuration

    \version 2020-07-28, V3.0.0, firmware for GD32F20x
*/

/*
    Copyright (c) 2020, GigaDevice Semiconductor Inc. 

    Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this 
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*/

#ifndef __USB_CONF_H
#define __USB_CONF_H

#include "stdlib.h"
#include "gd32f20x.h"

/* USB Core and PHY interface configuration */

/****************** USB FS PHY CONFIGURATION *******************************
 *  The USB FS Core supports one on-chip Full Speed PHY.
 *  The USE_EMBEDDED_PHY symbol is defined in the project compiler preprocessor
 *  when FS core is used.
*******************************************************************************/

#ifdef USE_USB_FS
    #define USB_FS_CORE
#endif

/*******************************************************************************
 *                      FIFO Size Configuration in Device mode
 *
 *  (i) Receive data FIFO size = RAM for setup packets + 
 *                   OUT endpoint control information +
 *                   data OUT packets + miscellaneous
 *      Space = ONE 32-bits words
 *      --> RAM for setup packets = 10 spaces
 *          (n is the nbr of CTRL EPs the device core supports)
 *      --> OUT EP CTRL info = 1 space
 *          (one space for status information written to the FIFO along with each 
 *          received packet)
 *      --> Data OUT packets = (Largest Packet Size / 4) + 1 spaces 
 *          (MINIMUM to receive packets)
 *      --> OR data OUT packets  = at least 2* (Largest Packet Size / 4) + 1 spaces 
 *          (if high-bandwidth EP is enabled or multiple isochronous EPs)
 *      --> Miscellaneous = 1 space per OUT EP
 *          (one space for transfer complete status information also pushed to the 
 *          FIFO with each endpoint's last packet)
 *
 *  (ii) MINIMUM RAM space required for each IN EP Tx FIFO = MAX packet size for 
 *       that particular IN EP. More space allocated in the IN EP Tx FIFO results
 *       in a better performance on the USB and can hide latencies on the AHB.
 *
 *  (iii) TXn min size = 16 words. (n:Transmit FIFO index)
 *
 *  (iv) When a TxFIFO is not used, the Configuration should be as follows:
 *       case 1: n > m and Txn is not used (n,m:Transmit FIFO indexes)
 *       --> Txm can use the space allocated for Txn.
 *       case 2: n < m and Txn is not used (n,m:Transmit FIFO indexes)
 *       --> Txn should be configured with the minimum space of 16 words
 *
 *  (v) The FIFO is used optimally when used TxFIFOs are allocated in the top 
 *      of the FIFO.Ex: use EP1 and EP2 as IN instead of EP1 and EP3 as IN ones.
*******************************************************************************/

#ifdef USB_FS_CORE
    #define RX_FIFO_FS_SIZE                         128U
    #define USB_RX_FIFO_FS_SIZE                     RX_FIFO_FS_SIZE
    #define TX0_FIFO_FS_SIZE                        64U
    #define TX1_FIFO_FS_SIZE                        128U
    #define TX2_FIFO_FS_SIZE                        0U
    #define TX3_FIFO_FS_SIZE                        0U
#endif /* USB_FS_CORE */

#define USB_SOF_OUTPUT              1U
#define USB_LOW_POWER               0U

/* if uncomment it, need jump to USB JP */
//#define VBUS_SENSING_ENABLED

//#define USE_HOST_MODE
#define USE_DEVICE_MODE
//#define USE_OTG_MODE

#ifndef USE_DEVICE_MODE
    #ifndef USE_HOST_MODE
        #error  "USE_DEVICE_MODE or USE_HOST_MODE should be defined!"
    #endif
#endif

#define __ALIGN_BEGIN
#define __ALIGN_END

/* __packed keyword used to decrease the data type alignment to 1-byte */
#if defined (__GNUC__)       /* GNU Compiler */
    #ifndef __packed
        #define __packed __attribute__ ((__packed__))
    #endif
#elif defined (__TASKING__)    /* TASKING Compiler */
    #define __packed __unaligned
#endif /* __CC_ARM */

#endif /* __USB_CONF_H */