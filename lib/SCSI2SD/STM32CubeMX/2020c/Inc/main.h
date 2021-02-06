/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FPGA_GPIO2_Pin GPIO_PIN_2
#define FPGA_GPIO2_GPIO_Port GPIOE
#define FPGA_GPIO3_Pin GPIO_PIN_3
#define FPGA_GPIO3_GPIO_Port GPIOE
#define USB_ID_PASSTHRU_Pin GPIO_PIN_5
#define USB_ID_PASSTHRU_GPIO_Port GPIOE
#define UNUSED_PE6_Pin GPIO_PIN_6
#define UNUSED_PE6_GPIO_Port GPIOE
#define nULPI_RESET_Pin GPIO_PIN_2
#define nULPI_RESET_GPIO_Port GPIOA
#define nSPICFG_CS_Pin GPIO_PIN_4
#define nSPICFG_CS_GPIO_Port GPIOA
#define VER_ID1_Pin GPIO_PIN_4
#define VER_ID1_GPIO_Port GPIOC
#define VER_ID2_Pin GPIO_PIN_5
#define VER_ID2_GPIO_Port GPIOC
#define BOOT1_Pin GPIO_PIN_2
#define BOOT1_GPIO_Port GPIOB
#define nTERM_EN_Pin GPIO_PIN_14
#define nTERM_EN_GPIO_Port GPIOB
#define LED_IO_Pin GPIO_PIN_15
#define LED_IO_GPIO_Port GPIOB
#define FPGA_RST_Pin GPIO_PIN_13
#define FPGA_RST_GPIO_Port GPIOD
#define nFGPA_CRESET_B_Pin GPIO_PIN_6
#define nFGPA_CRESET_B_GPIO_Port GPIOC
#define nFGPA_CDONE_Pin GPIO_PIN_7
#define nFGPA_CDONE_GPIO_Port GPIOC
#define OTG_FS_VBUS_UNUSED_Pin GPIO_PIN_9
#define OTG_FS_VBUS_UNUSED_GPIO_Port GPIOA
#define FSMC_UNUSED_CLK_Pin GPIO_PIN_3
#define FSMC_UNUSED_CLK_GPIO_Port GPIOD
#define NWAIT_UNUSED_Pin GPIO_PIN_6
#define NWAIT_UNUSED_GPIO_Port GPIOD
#define nSD_WP_Pin GPIO_PIN_8
#define nSD_WP_GPIO_Port GPIOB
#define nSD_CD_Pin GPIO_PIN_9
#define nSD_CD_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
