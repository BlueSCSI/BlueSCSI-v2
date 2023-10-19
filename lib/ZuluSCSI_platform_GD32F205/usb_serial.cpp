/** 
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
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


#include "ZuluSCSI_platform.h"
#include "usb_serial.h"

extern "C" 
{
#include <gd32_cdc_acm_core.h>
#include <drv_usb_hw.h>
#include <usbd_core.h>
#include <drv_usbd_int.h>
usb_core_driver cdc_acm;
}


void usb_serial_init(void)
{
    // set USB full speed prescaler and turn on USB clock
    rcu_usbfs_trng_clock_config(RCU_CKUSB_CKPLL_DIV2_5);
    rcu_periph_clock_enable(RCU_USBFS);
    usbd_init(&cdc_acm, USB_CORE_ENUM_FS, &gd32_cdc_desc, &gd32_cdc_class);

    // USB full speed Interrupt config
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    nvic_irq_enable((uint8_t)USBFS_IRQn, 2U, 0U);
}


bool usb_serial_ready(void)
{
    if (USBD_CONFIGURED == cdc_acm.dev.cur_status) 
    {
        if (1U == gd32_cdc_acm_check_ready(&cdc_acm)) 
        {
            return true;
        }
    }
    return false;
}

void usb_serial_send(uint8_t *data, uint32_t length)
{
    gd32_cdc_acm_data_send(&cdc_acm, data, length);
}

void USBFS_IRQHandler(void)
{
    usbd_isr(&cdc_acm);
}

void usb_udelay (const uint32_t usec)
{
    delay_us(usec);
}


void usb_mdelay (const uint32_t msec)
{
    delay(msec);
}