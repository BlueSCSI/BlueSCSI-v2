/**
  ******************************************************************************
  * @file    usbd_hid.h
  * @author  MCD Application Team
  * @version V2.4.1
  * @date    19-June-2015
  * @brief   Header file for the usbd_hid_core.c file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_COMPOSITE_H
#define __USB_COMPOSITE_H

#include  "usbd_ioreq.h"
#include  "usbd_hid.h"
#include  "usbd_msc.h"


#define USB_COMPOSITE_CONFIG_DESC_SIZ       64

extern USBD_ClassTypeDef  USBD_Composite;


typedef struct {
	__ALIGN_BEGIN USBD_HID_HandleTypeDef hid __ALIGN_END;
	__ALIGN_BEGIN USBD_MSC_BOT_HandleTypeDef msc __ALIGN_END;

	int DataInReady; // Endpoint number, 0 if not ready.
	int DataOutReady;// Endpoint number, 0 if not ready.
} USBD_CompositeClassData;


void s2s_usbDevicePoll(USBD_HandleTypeDef* pdev);

static inline uint8_t USBD_Composite_IsConfigured(USBD_HandleTypeDef *pdev) {
	return pdev->dev_state == USBD_STATE_CONFIGURED;
}

#endif

