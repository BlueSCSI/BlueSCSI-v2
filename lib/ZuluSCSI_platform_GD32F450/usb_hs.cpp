/** 
 * ZuluSCSI™ - Copyright (c) 2023-2025 Rabbit Hole Computing™
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


#include "usb_hs.h"
#include "ZuluSCSI_v1_4_gpio.h"
#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"

extern "C" 
{
#include <gd32_cdc_acm_core.h>
#include <gd32f4xx_gpio.h>
#include <drv_usb_hw.h>
#include <drv_usb_core.h>
#include <drv_usbd_int.h>
usb_core_driver cdc_acm;
}


void usb_hs_init(void)
{
    // USB HS clock
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_USBHSULPI);
    rcu_periph_clock_enable(RCU_USBHS);

    // usb_clock_active(&cdc_acm);

    // USB HS pins    
    gpio_mode_set(USB_HS_ULPI_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USB_HS_ULPI_CLK_PIN);
    gpio_mode_set(USB_HS_ULPI_DIR_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USB_HS_ULPI_DIR_PIN);
    gpio_mode_set(USB_HS_ULPI_NXT_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USB_HS_ULPI_NXT_PIN);
    gpio_mode_set(USB_HS_ULPI_STP_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USB_HS_ULPI_STP_PIN);
    gpio_mode_set(USB_HS_ULPI_D0_PORT,  GPIO_MODE_AF, GPIO_PUPD_NONE, USB_HS_ULPI_D0_PIN);
    gpio_mode_set(USB_HS_ULPI_D1_TO_7_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USB_HS_ULPI_D1_TO_7_PIN);
    
    gpio_output_options_set(USB_HS_ULPI_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, USB_HS_ULPI_CLK_PIN);
    gpio_output_options_set(USB_HS_ULPI_DIR_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, USB_HS_ULPI_DIR_PIN);
    gpio_output_options_set(USB_HS_ULPI_NXT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, USB_HS_ULPI_NXT_PIN);
    gpio_output_options_set(USB_HS_ULPI_STP_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, USB_HS_ULPI_STP_PIN);
    gpio_output_options_set(USB_HS_ULPI_D0_PORT,  GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, USB_HS_ULPI_D0_PIN);
    gpio_output_options_set(USB_HS_ULPI_D1_TO_7_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_200MHZ, USB_HS_ULPI_D1_TO_7_PIN);

    gpio_af_set(USB_HS_ULPI_CLK_PORT, USB_HS_ULPI_AF, USB_HS_ULPI_CLK_PIN);
    gpio_af_set(USB_HS_ULPI_DIR_PORT, USB_HS_ULPI_AF, USB_HS_ULPI_DIR_PIN);
    gpio_af_set(USB_HS_ULPI_NXT_PORT, USB_HS_ULPI_AF, USB_HS_ULPI_NXT_PIN);
    gpio_af_set(USB_HS_ULPI_STP_PORT, USB_HS_ULPI_AF, USB_HS_ULPI_STP_PIN);
    gpio_af_set(USB_HS_ULPI_D0_PORT,  USB_HS_ULPI_AF, USB_HS_ULPI_D0_PIN);
    gpio_af_set(USB_HS_ULPI_D1_TO_7_PORT, USB_HS_ULPI_AF, USB_HS_ULPI_D1_TO_7_PIN);

    
    usbd_init(&cdc_acm, USB_CORE_ENUM_HS, &gd32_cdc_desc, &gd32_cdc_class);

    // USB HS Interrupt config
    nvic_irq_enable((uint8_t)USBHS_IRQn, 0U, 0U);
    
#ifdef USB_HS_DEDICATED_EP1_ENABLED
    nvic_irq_enable(USBHS_EP1_Out_IRQn, 3, 0);
    nvic_irq_enable(USBHS_EP1_In_IRQn, 3, 0);
#endif /* USB_HS_DEDICATED_EP1_ENABLED */

    // USB HS wake functionality 
    /* enable the power module clock */
    // rcu_periph_clock_enable(RCU_PMU);

    
    /* USB wakeup EXTI line configuration */
    // exti_interrupt_flag_clear(EXTI_18);
    // exti_init(EXTI_18, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    // exti_interrupt_enable(EXTI_18);

    // nvic_irq_enable((uint8_t)USBFS_WKUP_IRQn, 1U, 0U);

}

bool usb_hs_ready(void)
{
    if (USBD_CONFIGURED == cdc_acm.dev.cur_status) 
    {
        if (1U == gd32_cdc_acm_check_ready(&cdc_acm)) {
            return true;
        }
    }
    return false;
}

void usb_hs_send(uint8_t *data, uint32_t length)
{
    gd32_cdc_acm_data_send(&cdc_acm, data, length);
}

extern "C" {
void usb_udelay (const uint32_t usec) 
{
    delay_us(usec);
}

void usb_mdelay(const uint32_t msec)
{
    delay(msec);
}

void USBHS_IRQHandler(void)
{
    usbd_isr(&cdc_acm);
}
#ifdef USB_HS_DEDICATED_EP1_ENABLED

/*!
    \brief      this function handles EP1_IN Handler
    \param[in]  none
    \param[out] none
    \retval     none
*/
__attribute__((interrupt, naked))
void USBHS_EP1_In_IRQHandler(void)
{
    usbd_int_dedicated_ep1in (&cdc_acm);
}

/*!
    \brief      this function handles EP1_OUT Handler
    \param[in]  none
    \param[out] none
    \retval     none
*/
__attribute__((interrupt, naked))
void USBHS_EP1_Out_IRQHandler(void)
{
    usbd_int_dedicated_ep1out (&cdc_acm);
}

#endif /* USB_HS_DEDICATED_EP1_ENABLED */

}