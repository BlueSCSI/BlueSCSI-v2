/** 
 * ZuluSCSI™ - Copyright (c) 2022 Rabbit Hole Computing™
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

// GPIO definitions for ZuluSCSI v1.0

#pragma once

// SCSI data output port.
// The output data is written using BSRR mechanism, so all data pins must be on same GPIO port.
// The output pins are open-drain in hardware, using separate buffer chips for driving.
#define SCSI_OUT_PORT GPIOD
#define SCSI_OUT_DB7  GPIO_PIN_0
#define SCSI_OUT_DB6  GPIO_PIN_1
#define SCSI_OUT_DB5  GPIO_PIN_2
#define SCSI_OUT_DB4  GPIO_PIN_3
#define SCSI_OUT_DB3  GPIO_PIN_4
#define SCSI_OUT_DB2  GPIO_PIN_5
#define SCSI_OUT_DB1  GPIO_PIN_6
#define SCSI_OUT_DB0  GPIO_PIN_7
#define SCSI_OUT_DBP  GPIO_PIN_15
#define SCSI_OUT_REQ  GPIO_PIN_9
#define SCSI_OUT_DATA_MASK (SCSI_OUT_DB0 | SCSI_OUT_DB1 | SCSI_OUT_DB2 | SCSI_OUT_DB3 | SCSI_OUT_DB4 | SCSI_OUT_DB5 | SCSI_OUT_DB6 | SCSI_OUT_DB7 | SCSI_OUT_DBP)
#define SCSI_OUT_REQ_IDX 9

// SCSI input data port
#define SCSI_IN_PORT  GPIOE
#define SCSI_IN_DB7   GPIO_PIN_15
#define SCSI_IN_DB6   GPIO_PIN_14
#define SCSI_IN_DB5   GPIO_PIN_13
#define SCSI_IN_DB4   GPIO_PIN_12
#define SCSI_IN_DB3   GPIO_PIN_11
#define SCSI_IN_DB2   GPIO_PIN_10
#define SCSI_IN_DB1   GPIO_PIN_9
#define SCSI_IN_DB0   GPIO_PIN_8
#define SCSI_IN_DBP   GPIO_PIN_7
#define SCSI_IN_MASK  (SCSI_IN_DB7|SCSI_IN_DB6|SCSI_IN_DB5|SCSI_IN_DB4|SCSI_IN_DB3|SCSI_IN_DB2|SCSI_IN_DB1|SCSI_IN_DB0|SCSI_IN_DBP)
#define SCSI_IN_SHIFT 8

// SCSI output status lines
#define SCSI_OUT_IO_PORT  GPIOD
#define SCSI_OUT_IO_PIN   GPIO_PIN_8
#define SCSI_OUT_CD_PORT  GPIOD
#define SCSI_OUT_CD_PIN   GPIO_PIN_10
#define SCSI_OUT_SEL_PORT GPIOD
#define SCSI_OUT_SEL_PIN  GPIO_PIN_11
#define SCSI_OUT_MSG_PORT GPIOD
#define SCSI_OUT_MSG_PIN  GPIO_PIN_12
#define SCSI_OUT_RST_PORT GPIOD
#define SCSI_OUT_RST_PIN  GPIO_PIN_13
#define SCSI_OUT_BSY_PORT GPIOD
#define SCSI_OUT_BSY_PIN  GPIO_PIN_14
#define SCSI_OUT_REQ_PORT SCSI_OUT_PORT
#define SCSI_OUT_REQ_PIN  SCSI_OUT_REQ

// SCSI input status signals
#define SCSI_ACK_PORT GPIOB
#define SCSI_ACK_PIN  GPIO_PIN_12
#define SCSI_IN_ACK_IDX 12

// The SCSI_ATN pin was PB0 in prototype 2022a, but was moved to PC6 for 5V-tolerance
#ifdef ZULUSCSI_2022A_REVISION
#define SCSI_ATN_PORT GPIOB
#define SCSI_ATN_PIN  GPIO_PIN_0
#else
#define SCSI_ATN_PORT GPIOC
#define SCSI_ATN_PIN  GPIO_PIN_6
#endif

// SEL pin uses EXTI interrupt
#define SCSI_SEL_PORT GPIOB
#define SCSI_SEL_PIN  GPIO_PIN_11
#define SCSI_SEL_EXTI EXTI_11
#define SCSI_SEL_EXTI_SOURCE_PORT GPIO_PORT_SOURCE_GPIOB
#define SCSI_SEL_EXTI_SOURCE_PIN GPIO_PIN_SOURCE_11
#define SCSI_SEL_IRQ EXTI10_15_IRQHandler
#define SCSI_SEL_IRQn EXTI10_15_IRQn

// BSY pin uses EXTI interrupt
#define SCSI_BSY_PORT GPIOB
#define SCSI_BSY_PIN  GPIO_PIN_10
#define SCSI_BSY_EXTI EXTI_10
#define SCSI_BSY_EXTI_SOURCE_PORT GPIO_PORT_SOURCE_GPIOB
#define SCSI_BSY_EXTI_SOURCE_PIN  GPIO_PIN_SOURCE_10
#define SCSI_BSY_IRQ  EXTI10_15_IRQHandler
#define SCSI_BSY_IRQn EXTI10_15_IRQn

// RST pin uses EXTI interrupt
#define SCSI_RST_PORT GPIOB
#define SCSI_RST_PIN  GPIO_PIN_13
#define SCSI_RST_EXTI EXTI_13
#define SCSI_RST_EXTI_SOURCE_PORT GPIO_PORT_SOURCE_GPIOB
#define SCSI_RST_EXTI_SOURCE_PIN GPIO_PIN_SOURCE_13
#define SCSI_RST_IRQ  EXTI10_15_IRQHandler
#define SCSI_RST_IRQn EXTI10_15_IRQn

// Terminator enable/disable config, active low
#define SCSI_TERM_EN_PORT GPIOC
#define SCSI_TERM_EN_PIN  GPIO_PIN_8

// SD card pins
#define SD_PORT      GPIOA
#define SD_CS_PIN    GPIO_PIN_4
#define SD_CLK_PIN   GPIO_PIN_5
#define SD_MISO_PIN  GPIO_PIN_6
#define SD_MOSI_PIN  GPIO_PIN_7
#define SD_SPI       SPI0
#define SD_SPI_RX_DMA_CHANNEL DMA_CH1
#define SD_SPI_TX_DMA_CHANNEL DMA_CH2

// DIP switches
#define DIP_PORT     GPIOB
#define DIPSW1_PIN   GPIO_PIN_4
#define DIPSW2_PIN   GPIO_PIN_5
#define DIPSW3_PIN   GPIO_PIN_6

// Status LED pins
#define LED_PORT     GPIOC
#define LED_I_PIN    GPIO_PIN_4
#define LED_E_PIN    GPIO_PIN_5
#define LED_PINS     (LED_I_PIN | LED_E_PIN)
#define LED_ON()     gpio_bit_reset(LED_PORT, LED_PINS)
#define LED_OFF()    gpio_bit_set(LED_PORT, LED_PINS)

// Ejection buttons are available on expansion header J303.
// PE5 = channel 1, PE6 = channel 2
// Connect button between GPIO and GND pin.
#define EJECT_1_PORT    GPIOE
#define EJECT_1_PIN     GPIO_PIN_5
#define EJECT_2_PORT    GPIOE
#define EJECT_2_PIN     GPIO_PIN_6
