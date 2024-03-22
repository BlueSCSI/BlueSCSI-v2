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

}
extern "C" usb_core_driver cdc_acm;

void usb_serial_init(void)
{
    // set USB full speed prescaler and turn on USB clock
#ifdef USE_USB_FS
    rcu_pll48m_clock_config(RCU_PLL48MSRC_PLLQ);
    rcu_ck48m_clock_config(RCU_CK48MSRC_PLL48M);
    rcu_periph_clock_enable(RCU_USBFS);
#elif defined(USE_USB_HS)
    #ifdef USE_EMBEDDED_PHY
        rcu_pll48m_clock_config(RCU_PLL48MSRC_PLLQ);
        rcu_ck48m_clock_config(RCU_CK48MSRC_PLL48M);
    #elif defined(USE_ULPI_PHY)
        rcu_periph_clock_enable(RCU_USBHSULPI);
    #endif /* USE_EMBEDDED_PHY */

    rcu_periph_clock_enable(RCU_USBHS);
#endif    
    usbd_init(&cdc_acm, USB_CORE_ENUM_HS, &gd32_cdc_desc, &gd32_cdc_class);
}


bool usb_serial_ready(void)
{
    // check that (our) serial is the currently active class
    if ((USBD_CONFIGURED == cdc_acm.dev.cur_status) && (cdc_acm.dev.desc == &gd32_cdc_desc)) 
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

