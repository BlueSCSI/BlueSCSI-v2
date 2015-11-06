/**
  ******************************************************************************
  * @file    usbd_hid.c
  * @author  MCD Application Team
  * @version V2.4.1
  * @date    19-June-2015
  * @brief   This file provides the HID core functions.
  *
  * @verbatim
  *      
  *          ===================================================================      
  *                                HID Class  Description
  *          =================================================================== 
  *           This module manages the HID class V1.11 following the "Device Class Definition
  *           for Human Interface Devices (HID) Version 1.11 Jun 27, 2001".
  *           This driver implements the following aspects of the specification:
  *             - The Boot Interface Subclass
  *             - The Mouse protocol
  *             - Usage Page : Generic Desktop
  *             - Usage : Joystick
  *             - Collection : Application 
  *      
  * @note     In HS mode and when the DMA is used, all variables and data structures
  *           dealing with the DMA during the transaction process should be 32-bit aligned.
  *           
  *      
  *  @endverbatim
  *
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

/* Includes ------------------------------------------------------------------*/
#include "usbd_hid.h"
#include "usbd_composite.h"
#include "usbd_msc.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"



int usbdReportReady = 0; // Global to allow poll-based HID report processing



/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ]  __ALIGN_END  =
{
  /* 18 */
  0x09,         /*bLength: HID Descriptor size*/
  HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
  0x11,         /*bcdHID: HID Class Spec release number*/
  0x01,
  0x00,         /*bCountryCode: Hardware target country*/
  0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
  0x22,         /*bDescriptorType*/
  HID_GENERIC_REPORT_DESC_SIZE,/*wItemLength: Total length of Report descriptor*/
  0x00,
};

__ALIGN_BEGIN static uint8_t HID_GENERIC_ReportDesc[HID_GENERIC_REPORT_DESC_SIZE]  __ALIGN_END =
{
0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
0x09, 0x00,        // Usage (0x00)
0xA1, 0x00,        // Collection (Physical)
0x09, 0x00,        //   Usage (0x00)
0xA1, 0x00,        //   Collection (Physical)
0x09, 0x00,        //     Usage (0x00)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0xFF,        //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x40,        //     Report Count (64)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0x00,        //     Usage (0x00)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0xFF,        //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x40,        //     Report Count (64)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0xC0,              // End Collection
};


/**
  * @brief  USBD_HID_SendReport 
  *         Send HID Report
  * @param  pdev: device instance
  * @param  buff: pointer to report
  * @retval status
  */
uint8_t USBD_HID_SendReport     (USBD_HandleTypeDef  *pdev, 
		const uint8_t *report,
		uint16_t len)
{
	USBD_CompositeClassData *classData = (USBD_CompositeClassData*) pdev->pClassData;
	USBD_HID_HandleTypeDef *hhid = &(classData->hid);

	if (pdev->dev_state == USBD_STATE_CONFIGURED )
	{
		if(hhid->state == HID_IDLE)
		{
			hhid->state = HID_BUSY;
			USBD_LL_Transmit (pdev,
					HID_EPIN_ADDR,
					(uint8_t*)report,
					len);
		}
	}
	return USBD_OK;
}

/**
  * @brief  USBD_HID_GetPollingInterval 
  *         return polling interval from endpoint descriptor
  * @param  pdev: device instance
  * @retval polling interval
  */
uint32_t USBD_HID_GetPollingInterval (USBD_HandleTypeDef *pdev)
{
	/* Sets the data transfer polling interval for low and full 
	speed transfers */
	return HID_FS_BINTERVAL;
}

uint8_t USBD_HID_GetReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint8_t maxLen)
{
	USBD_CompositeClassData *classData = (USBD_CompositeClassData*) pdev->pClassData;
	USBD_HID_HandleTypeDef *hhid = &(classData->hid);

	uint8_t count = USBD_GetRxCount(pdev, HID_EPOUT_ADDR);
	memcpy(report, hhid->rxBuffer, count < maxLen ? count : maxLen);

	/* Prepare Out endpoint to receive next packet */
	hhid->reportReady = 0;
	USBD_LL_PrepareReceive(pdev, HID_EPOUT_ADDR, hhid->rxBuffer, sizeof(hhid->rxBuffer));

	return count;
}

uint8_t USBD_HID_IsBusy(USBD_HandleTypeDef *pdev)
{
	return ((USBD_CompositeClassData*)pdev->pClassData)->hid.state != HID_IDLE;
}

uint8_t USBD_HID_IsReportReady(USBD_HandleTypeDef *pdev) {
	USBD_CompositeClassData *classData = (USBD_CompositeClassData*) pdev->pClassData;
	USBD_HID_HandleTypeDef *hhid = &(classData->hid);

	return hhid->reportReady == 1;
}

const uint8_t* USBD_HID_GetDesc()
{
	return USBD_HID_Desc;
}


const uint8_t* USBD_HID_GetReportDesc()
{
	return HID_GENERIC_ReportDesc;
}

