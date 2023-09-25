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

// GPIO definitions for ZuluSCSI v1.2

#pragma once

// SCSI data output port.
// The output data is written using BSRR mechanism, so all data pins must be on same GPIO port.
// The output pins are open-drain in hardware, using separate buffer chips for driving.
#define SCSI_OUT_PORT GPIOD
#define SCSI_OUT_DB7  GPIO_PIN_9
#define SCSI_OUT_DB6  GPIO_PIN_10
#define SCSI_OUT_DB5  GPIO_PIN_11
#define SCSI_OUT_DB4  GPIO_PIN_12
#define SCSI_OUT_DB3  GPIO_PIN_13
#define SCSI_OUT_DB2  GPIO_PIN_14
#define SCSI_OUT_DB1  GPIO_PIN_0
#define SCSI_OUT_DB0  GPIO_PIN_1
#define SCSI_OUT_DBP  GPIO_PIN_8
#define SCSI_OUT_REQ  GPIO_PIN_4
#define SCSI_OUT_DATA_MASK (SCSI_OUT_DB0 | SCSI_OUT_DB1 | SCSI_OUT_DB2 | SCSI_OUT_DB3 | SCSI_OUT_DB4 | SCSI_OUT_DB5 | SCSI_OUT_DB6 | SCSI_OUT_DB7 | SCSI_OUT_DBP)
#define SCSI_OUT_REQ_IDX 4

// Control signals to optional PLD device
#define SCSI_OUT_PLD1 GPIO_PIN_15
#define SCSI_OUT_PLD2 GPIO_PIN_3
#define SCSI_OUT_PLD3 GPIO_PIN_5
#define SCSI_OUT_PLD4 GPIO_PIN_7

// Control signals for timer based DMA acceleration
#define SCSI_TIMER TIMER7
#define SCSI_TIMER_RCU RCU_TIMER7
#define SCSI_TIMER_OUT_PORT GPIOB
#define SCSI_TIMER_OUT_PIN GPIO_PIN_1
#define SCSI_TIMER_IN_PORT GPIOC
#define SCSI_TIMER_IN_PIN GPIO_PIN_6
#define SCSI_TIMER_DMA DMA1
#define SCSI_TIMER_DMA_RCU RCU_DMA1
#define SCSI_TIMER_DMACHA DMA_CH4
#define SCSI_TIMER_DMACHB DMA_CH1
#define SCSI_TIMER_DMACHA_IRQ DMA1_Channel4_IRQHandler
#define SCSI_TIMER_DMACHA_IRQn DMA1_Channel4_IRQn
#define SCSI_TIMER_DMACHB_IRQ DMA1_Channel1_IRQHandler
#define SCSI_TIMER_DMACHB_IRQn DMA1_Channel1_IRQn

// GreenPAK logic chip pins
#define GREENPAK_I2C_ADDR 0x10
#define GREENPAK_I2C_PORT GPIOB
#define GREENPAK_I2C_SCL GPIO_PIN_8
#define GREENPAK_I2C_SDA GPIO_PIN_9
#define GREENPAK_PLD_IO1 GPIO_PIN_15
#define GREENPAK_PLD_IO2 GPIO_PIN_3
#define GREENPAK_PLD_IO3 GPIO_PIN_5
#define GREENPAK_PLD_IO4 GPIO_PIN_7
#define GREENPAK_PLD_IO2_EXTI EXTI_3
#define GREENPAK_PLD_IO2_EXTI_SOURCE_PORT GPIO_PORT_SOURCE_GPIOD
#define GREENPAK_PLD_IO2_EXTI_SOURCE_PIN  GPIO_PIN_SOURCE_3
#define GREENPAK_IRQ  EXTI3_IRQHandler
#define GREENPAK_IRQn EXTI3_IRQn

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
#define SCSI_OUT_IO_PORT  GPIOA
#define SCSI_OUT_IO_PIN   GPIO_PIN_4
#define SCSI_OUT_CD_PORT  GPIOA
#define SCSI_OUT_CD_PIN   GPIO_PIN_5
#define SCSI_OUT_SEL_PORT GPIOA
#define SCSI_OUT_SEL_PIN  GPIO_PIN_6
#define SCSI_OUT_MSG_PORT GPIOA
#define SCSI_OUT_MSG_PIN  GPIO_PIN_7
#define SCSI_OUT_RST_PORT GPIOB
#define SCSI_OUT_RST_PIN  GPIO_PIN_14
#define SCSI_OUT_BSY_PORT GPIOB
#define SCSI_OUT_BSY_PIN  GPIO_PIN_15
#define SCSI_OUT_REQ_PORT SCSI_OUT_PORT
#define SCSI_OUT_REQ_PIN  SCSI_OUT_REQ

// SCSI input status signals
#define SCSI_ACK_PORT GPIOA
#define SCSI_ACK_PIN  GPIO_PIN_0
#define SCSI_ATN_PORT GPIOB
#define SCSI_ATN_PIN  GPIO_PIN_12
#define SCSI_IN_ACK_IDX 0

// Extra signals used with EXMC for synchronous mode
#define SCSI_IN_ACK_EXMC_NWAIT_PORT GPIOD
#define SCSI_IN_ACK_EXMC_NWAIT_PIN  GPIO_PIN_6
#define SCSI_OUT_REQ_EXMC_NOE_PORT  GPIOD
#define SCSI_OUT_REQ_EXMC_NOE_PIN   GPIO_PIN_4
#define SCSI_OUT_REQ_EXMC_NOE_IDX   4
#define SCSI_EXMC_DATA_SHIFT 5
#define SCSI_EXMC_DMA DMA0
#define SCSI_EXMC_DMA_RCU RCU_DMA0
#define SCSI_EXMC_DMACH DMA_CH0
#define SCSI_SYNC_TIMER TIMER1
#define SCSI_SYNC_TIMER_RCU RCU_TIMER1

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
#define SCSI_TERM_EN_PORT GPIOB
#define SCSI_TERM_EN_PIN  GPIO_PIN_0

// SD card pins
#define SD_USE_SDIO 1
#define SD_SDIO_DATA_PORT GPIOC
#define SD_SDIO_D0        GPIO_PIN_8
#define SD_SDIO_D1        GPIO_PIN_9
#define SD_SDIO_D2        GPIO_PIN_10
#define SD_SDIO_D3        GPIO_PIN_11
#define SD_SDIO_CLK_PORT  GPIOC
#define SD_SDIO_CLK       GPIO_PIN_12
#define SD_SDIO_CMD_PORT  GPIOD
#define SD_SDIO_CMD       GPIO_PIN_2

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
