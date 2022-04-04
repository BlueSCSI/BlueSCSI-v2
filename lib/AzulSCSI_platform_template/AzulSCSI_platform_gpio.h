// Example GPIO definitions for AzulSCSI platform

#pragma once

// SCSI data output port.
// The output data is written using BSRR mechanism, so all data pins must be on same GPIO port.
// The output pins are open-drain in hardware, using separate buffer chips for driving.
#define SCSI_OUT_PORT GPIOD
#define SCSI_OUT_DB0  GPIO_PIN_0
#define SCSI_OUT_DB1  GPIO_PIN_1
#define SCSI_OUT_DB2  GPIO_PIN_2
#define SCSI_OUT_DB3  GPIO_PIN_3
#define SCSI_OUT_DB4  GPIO_PIN_4
#define SCSI_OUT_DB5  GPIO_PIN_5
#define SCSI_OUT_DB6  GPIO_PIN_6
#define SCSI_OUT_DB7  GPIO_PIN_7
#define SCSI_OUT_DBP  GPIO_PIN_8
#define SCSI_OUT_REQ  GPIO_PIN_9
#define SCSI_OUT_DATA_MASK (SCSI_OUT_DB0 | SCSI_OUT_DB1 | SCSI_OUT_DB2 | SCSI_OUT_DB3 | SCSI_OUT_DB4 | SCSI_OUT_DB5 | SCSI_OUT_DB6 | SCSI_OUT_DB7 | SCSI_OUT_DBP)

// SCSI input data port (can be same as output port)
#define SCSI_IN_PORT  GPIOE
#define SCSI_IN_DB0   GPIO_PIN_0
#define SCSI_IN_DB1   GPIO_PIN_1
#define SCSI_IN_DB2   GPIO_PIN_2
#define SCSI_IN_DB3   GPIO_PIN_3
#define SCSI_IN_DB4   GPIO_PIN_4
#define SCSI_IN_DB5   GPIO_PIN_5
#define SCSI_IN_DB6   GPIO_PIN_6
#define SCSI_IN_DB7   GPIO_PIN_7
#define SCSI_IN_DBP   GPIO_PIN_8
#define SCSI_IN_MASK  (SCSI_IN_DB7|SCSI_IN_DB6|SCSI_IN_DB5|SCSI_IN_DB4|SCSI_IN_DB3|SCSI_IN_DB2|SCSI_IN_DB1|SCSI_IN_DB0|SCSI_IN_DBP)
#define SCSI_IN_SHIFT 8

// SCSI output status lines
#define SCSI_OUT_IO_PORT  GPIOD
#define SCSI_OUT_IO_PIN   GPIO_PIN_10
#define SCSI_OUT_CD_PORT  GPIOD
#define SCSI_OUT_CD_PIN   GPIO_PIN_11
#define SCSI_OUT_SEL_PORT GPIOD
#define SCSI_OUT_SEL_PIN  GPIO_PIN_12
#define SCSI_OUT_MSG_PORT GPIOD
#define SCSI_OUT_MSG_PIN  GPIO_PIN_13
#define SCSI_OUT_RST_PORT GPIOD
#define SCSI_OUT_RST_PIN  GPIO_PIN_14
#define SCSI_OUT_BSY_PORT GPIOD
#define SCSI_OUT_BSY_PIN  GPIO_PIN_15
#define SCSI_OUT_REQ_PORT SCSI_OUT_PORT
#define SCSI_OUT_REQ_PIN  SCSI_OUT_REQ

// SCSI input status signals (can be same as output port)
#define SCSI_SEL_PORT GPIOD
#define SCSI_SEL_PIN  GPIO_PIN_12
#define SCSI_ACK_PORT GPIOE
#define SCSI_ACK_PIN  GPIO_PIN_0
#define SCSI_ATN_PORT GPIOE
#define SCSI_ATN_PIN  GPIO_PIN_1
#define SCSI_BSY_PORT GPIOE
#define SCSI_BSY_PIN  GPIO_PIN_2
#define SCSI_RST_PORT GPIOE
#define SCSI_RST_PIN  GPIO_PIN_3

// Status LED pins
#define LED_PORT     GPIOE
#define LED_PIN      GPIO_PIN_4
#define LED_ON()     LED_PORT->BSRR = LED_PIN
#define LED_OFF()    LED_PORT->BRR = LED_PIN