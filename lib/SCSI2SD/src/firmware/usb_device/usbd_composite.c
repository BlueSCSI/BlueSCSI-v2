/**
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
#include "usbd_composite.h"
#include "usbd_hid.h"
#include "usbd_msc.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"



static uint8_t  USBD_Composite_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx);

static uint8_t  USBD_Composite_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx);

static uint8_t  USBD_Composite_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

static uint8_t  *USBD_Composite_GetCfgDesc (uint16_t *length);

static uint8_t  *USBD_Composite_GetDeviceQualifierDesc (uint16_t *length);

static uint8_t  USBD_Composite_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_Composite_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);


USBD_ClassTypeDef USBD_Composite =
{
	USBD_Composite_Init,
	USBD_Composite_DeInit,
	USBD_Composite_Setup,
	NULL, /*EP0_TxSent*/  
	NULL, /*EP0_RxReady*/
	USBD_Composite_DataIn, /*DataIn*/
	USBD_Composite_DataOut, /*DataOut*/
	NULL, /*SOF */
	NULL,
	NULL,      
	USBD_Composite_GetCfgDesc,
	USBD_Composite_GetCfgDesc,
	USBD_Composite_GetCfgDesc,
	USBD_Composite_GetDeviceQualifierDesc,
};

__ALIGN_BEGIN static uint8_t USBD_Composite_CfgDesc[USB_COMPOSITE_CONFIG_DESC_SIZ]  __ALIGN_END =
{
  0x09, /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION, /* bDescriptorType: Configuration */
  USB_COMPOSITE_CONFIG_DESC_SIZ,
  /* wTotalLength: Bytes returned */
  0x00,
  0x02,         /*bNumInterfaces: 1 interface*/
  0x01,         /*bConfigurationValue: Configuration value*/
  0x00,         /*iConfiguration: Index of string descriptor describing
  the configuration*/
  0x80,         /*bmAttributes: bus powered */
  0xFA,         /*MaxPower 500 mA: this current is used for detecting Vbus*/
  
  /************** Descriptor of GENERIC interface ****************/
  /* 09 */
  0x09,         /*bLength: Interface Descriptor size*/
  USB_DESC_TYPE_INTERFACE,/*bDescriptorType: Interface descriptor type*/
  0x00,         /*bInterfaceNumber: Number of Interface*/
  0x00,         /*bAlternateSetting: Alternate setting*/
  0x02,         /*bNumEndpoints*/
  0x03,         /*bInterfaceClass: HID*/
  0x00,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
  0x00,         /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
  0,            /*iInterface: Index of string descriptor*/
  /******************** Descriptor of GENERIC HID ********************/
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
  /******************** Descriptor of Generic HID endpoint ********************/
  /* 27 */
  0x07,          /*bLength: Endpoint Descriptor size*/
  USB_DESC_TYPE_ENDPOINT, /*bDescriptorType:*/
  
  HID_EPIN_ADDR,     /*bEndpointAddress: Endpoint Address (IN)*/
  0x03,          /*bmAttributes: Interrupt endpoint*/
  HID_EPIN_SIZE, /*wMaxPacketSize: 64 Byte max */
  0x00,
  HID_FS_BINTERVAL,          /*bInterval*/
  /* 34 */

  /******************** Descriptor of GENERIC HID endpoint ********************/
  /* 34 */
  0x07,          /*bLength: Endpoint Descriptor size*/
  USB_DESC_TYPE_ENDPOINT, /*bDescriptorType:*/
  
  HID_EPOUT_ADDR,    /*bEndpointAddress: Endpoint Address (OUT)*/
  0x03,          /*bmAttributes: Interrupt endpoint*/
  HID_EPOUT_SIZE, /*wMaxPacketSize */
  0x00,
  HID_FS_BINTERVAL,          /*bInterval*/
  /* 41 */

  /********************  Mass Storage interface ********************/
  0x09,   /* bLength: Interface Descriptor size */
  USB_DESC_TYPE_INTERFACE,   /* bDescriptorType: */
  0x01,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints*/
  0x08,   /* bInterfaceClass: MSC Class */
  0x06,   /* bInterfaceSubClass : SCSI transparent*/
  0x50,   /* nInterfaceProtocol */
  0x00,          /* iInterface: */
  /********************  Mass Storage Endpoints ********************/
  0x07,   /*Endpoint descriptor length = 7*/
  0x05,   /*Endpoint descriptor type */
  MSC_EPIN_ADDR,   /*Endpoint address */
  0x02,   /*Bulk endpoint type */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00,   /*Polling interval in milliseconds */
  
  0x07,   /*Endpoint descriptor length = 7 */
  0x05,   /*Endpoint descriptor type */
  MSC_EPOUT_ADDR,   /*Endpoint address  */
  0x02,   /*Bulk endpoint type */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00     /*Polling interval in milliseconds*/

} ;

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_Composite_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC]  __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};


static uint8_t  USBD_Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
	uint8_t ret = 0;

	// HID Endpoints
	USBD_LL_OpenEP(pdev, HID_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_EPIN_SIZE);
	USBD_LL_OpenEP(pdev, HID_EPOUT_ADDR, USBD_EP_TYPE_INTR, HID_EPOUT_SIZE);


	// MSC Endpoints
	USBD_LL_OpenEP(pdev, MSC_EPOUT_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
	USBD_LL_OpenEP(pdev, MSC_EPIN_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);


	// Use static memory. This limits us to a single HID and MSC devicd
	static USBD_CompositeClassData classData;
	classData.hid.state = HID_IDLE;
	classData.hid.reportReady = 0;
	pdev->pClassData = &classData;

	MSC_BOT_Init(pdev);

	// Prepare Out endpoint to receive next HID packet
	USBD_LL_PrepareReceive(
		pdev,
		HID_EPOUT_ADDR,
		classData.hid.rxBuffer,
		sizeof(classData.hid.rxBuffer));

	return ret;
}

static uint8_t USBD_Composite_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
	USBD_LL_CloseEP(pdev, HID_EPIN_ADDR);
	USBD_LL_CloseEP(pdev, HID_EPOUT_ADDR);
	USBD_LL_CloseEP(pdev, MSC_EPOUT_ADDR);
	USBD_LL_CloseEP(pdev, MSC_EPIN_ADDR);

	MSC_BOT_DeInit(pdev);

	pdev->pClassData = NULL;
	return USBD_OK;
}


static uint8_t USBD_Composite_Setup(
	USBD_HandleTypeDef *pdev,
	USBD_SetupReqTypedef *req)
{
	uint16_t len = 0;
	uint8_t  *pbuf = NULL;

	USBD_CompositeClassData *classData = (USBD_CompositeClassData*) pdev->pClassData;
	USBD_HID_HandleTypeDef *hhid = &(classData->hid);
	USBD_MSC_BOT_HandleTypeDef *hmsc = &(classData->msc);

	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
	case USB_REQ_TYPE_CLASS :
		switch (req->bRequest)
		{
			case HID_REQ_SET_PROTOCOL:
				hhid->Protocol = (uint8_t)(req->wValue);
				break;
			case HID_REQ_GET_PROTOCOL:
				USBD_CtlSendData (pdev, (uint8_t *)&hhid->Protocol, 1);
				break;
			case HID_REQ_SET_IDLE:
				hhid->IdleState = (uint8_t)(req->wValue >> 8);
				break;
			case HID_REQ_GET_IDLE:
				USBD_CtlSendData (pdev, (uint8_t *)&hhid->IdleState, 1);
				break;

			case BOT_GET_MAX_LUN :
				if((req->wValue  == 0) &&
					(req->wLength == 1) &&
					((req->bmRequest & 0x80) == 0x80))
				{
					hmsc->max_lun = ((USBD_StorageTypeDef *)pdev->pUserData)->GetMaxLun();
					USBD_CtlSendData (pdev, (uint8_t *)&hmsc->max_lun, 1);
				}
				else
				{
					USBD_CtlError(pdev , req);
					return USBD_FAIL;
				}
				break;

			case BOT_RESET :
				if((req->wValue  == 0) &&
					(req->wLength == 0) &&
					((req->bmRequest & 0x80) != 0x80))
				{
					MSC_BOT_Reset(pdev);
				}
				else
				{
					USBD_CtlError(pdev , req);
					return USBD_FAIL;
				}
				break;


			default:
				USBD_CtlError (pdev, req);
				return USBD_FAIL;
		} break;


	case USB_REQ_TYPE_STANDARD:
		switch (req->bRequest)
		{
			case USB_REQ_GET_DESCRIPTOR:
				if( req->wValue >> 8 == HID_REPORT_DESC)
				{
					len = MIN(HID_GENERIC_REPORT_DESC_SIZE , req->wLength);
					pbuf = (uint8_t*) USBD_HID_GetReportDesc();
				}
				else if( req->wValue >> 8 == HID_DESCRIPTOR_TYPE)
				{
					pbuf = (uint8_t*) USBD_HID_GetDesc();
					len = MIN(USB_HID_DESC_SIZ , req->wLength);
				}
				USBD_CtlSendData (pdev, pbuf, len);
				break;

			case USB_REQ_GET_INTERFACE :
				if (req->wIndex == 0)
				{
					USBD_CtlSendData (pdev, (uint8_t *)&hhid->AltSetting, 1);
				}
				else
				{
					USBD_CtlSendData (pdev, (uint8_t *)&hmsc->interface, 1);
				}
				break;
			case USB_REQ_SET_INTERFACE :
				if (req->wIndex == 0)
				{
					hhid->AltSetting = (uint8_t)(req->wValue);
				}
				else
				{
					hmsc->interface = (uint8_t)(req->wValue);
				}
				break;

			case USB_REQ_CLEAR_FEATURE:
				/* Flush the FIFO and Clear the stall status */
				USBD_LL_FlushEP(pdev, (uint8_t)req->wIndex);

				/* Reactivate the EP */
				USBD_LL_CloseEP (pdev , (uint8_t)req->wIndex);
				switch ((uint8_t)req->wIndex)
				{
				case MSC_EPIN_ADDR:
					USBD_LL_OpenEP(pdev, MSC_EPIN_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
					break;

				case MSC_EPOUT_ADDR:
					USBD_LL_OpenEP(pdev, MSC_EPOUT_ADDR, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);  
					break;

				case HID_EPIN_ADDR:
					USBD_LL_OpenEP(pdev, HID_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_EPIN_SIZE);
					break;

				case HID_EPOUT_ADDR:
					USBD_LL_OpenEP(pdev, HID_EPOUT_ADDR, USBD_EP_TYPE_INTR, HID_EPOUT_SIZE);
					break;
				}

				/* Handle BOT error */
				MSC_BOT_CplClrFeature(pdev, (uint8_t)req->wIndex);
				break;

		} break;
	}

	return USBD_OK;
}


int FIXME_IN = 0;
int FIXME_OUT = 0;
USBD_HandleTypeDef  *pdevtmp;
static uint8_t USBD_Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
pdevtmp = pdev;
	if (epnum == (HID_EPIN_ADDR & 0x7F))
	{
		USBD_CompositeClassData *classData = (USBD_CompositeClassData*) pdev->pClassData;
		USBD_HID_HandleTypeDef *hhid = &(classData->hid);
		/* Ensure that the FIFO is empty before a new transfer, this condition could 
		be caused by  a new transfer before the end of the previous transfer */
		hhid->state = HID_IDLE;
	}
	else if (epnum == (MSC_EPIN_ADDR & 0x7F))
	{
	FIXME_IN = epnum;
	//	MSC_BOT_DataIn(pdev , epnum);
	}
	return USBD_OK;
}

static uint8_t USBD_Composite_DataOut(USBD_HandleTypeDef  *pdev, uint8_t epnum)
{
pdevtmp = pdev;
	if (epnum == (HID_EPOUT_ADDR & 0x7F))
	{
		USBD_CompositeClassData *classData = (USBD_CompositeClassData*) pdev->pClassData;
		USBD_HID_HandleTypeDef *hhid = &(classData->hid);
		hhid->reportReady = 1;
	}
	else if (epnum == (MSC_EPOUT_ADDR & 0x7F))
	{
	FIXME_OUT = epnum;
		//MSC_BOT_DataOut(pdev, epnum);
	}
	return USBD_OK;
}

void s2s_usbDevicePoll(void) {
	if (FIXME_IN) {
		int tmp = FIXME_IN;
		FIXME_IN = 0;
		MSC_BOT_DataIn(pdevtmp, tmp);
	}

	if (FIXME_OUT) {
		int tmp = FIXME_OUT;
		FIXME_OUT = 0;
		MSC_BOT_DataOut(pdevtmp, tmp);
	}
}


static uint8_t *USBD_Composite_GetDeviceQualifierDesc (uint16_t *length)
{
	*length = sizeof (USBD_Composite_DeviceQualifierDesc);
	return USBD_Composite_DeviceQualifierDesc;
}


static uint8_t *USBD_Composite_GetCfgDesc (uint16_t *length)
{
	*length = sizeof (USBD_Composite_CfgDesc);
	return USBD_Composite_CfgDesc;
}
