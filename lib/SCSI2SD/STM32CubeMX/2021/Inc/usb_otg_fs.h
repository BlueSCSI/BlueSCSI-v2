/**
  ******************************************************************************
  * File Name          : USB_OTG_FS.h
  * Description        : This file provides code for the configuration
  *                      of the USB_OTG_FS instances.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __usb_otg_fs_H
#define __usb_otg_fs_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_USB_OTG_FS_PCD_Init(void);
void MX_USB_OTG_HS_PCD_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ usb_otg_fs_H */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
