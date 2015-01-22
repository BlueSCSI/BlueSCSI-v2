/*******************************************************************************
* File Name: USBFS_cdc.h
* Version 2.80
*
* Description:
*  Header File for the USBFS component.
*  Contains CDC class prototypes and constant values.
*
* Related Document:
*  Universal Serial Bus Class Definitions for Communication Devices Version 1.1
*
********************************************************************************
* Copyright 2012-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_USBFS_USBFS_cdc_H)
#define CY_USBFS_USBFS_cdc_H

#include "cytypes.h"


/***************************************
* Prototypes of the USBFS_cdc API.
***************************************/

#if (USBFS_ENABLE_CDC_CLASS_API != 0u)
    void USBFS_CDC_Init(void) ;
    void USBFS_PutData(const uint8* pData, uint16 length) ;
    void USBFS_PutString(const char8 string[]) ;
    void USBFS_PutChar(char8 txDataByte) ;
    void USBFS_PutCRLF(void) ;
    uint16 USBFS_GetCount(void) ;
    uint8 USBFS_CDCIsReady(void) ;
    uint8 USBFS_DataIsReady(void) ;
    uint16 USBFS_GetData(uint8* pData, uint16 length) ;
    uint16 USBFS_GetAll(uint8* pData) ;
    uint8 USBFS_GetChar(void) ;
    uint8 USBFS_IsLineChanged(void) ;
    uint32 USBFS_GetDTERate(void) ;
    uint8 USBFS_GetCharFormat(void) ;
    uint8 USBFS_GetParityType(void) ;
    uint8 USBFS_GetDataBits(void) ;
    uint16 USBFS_GetLineControl(void) ;
#endif  /*  USBFS_ENABLE_CDC_CLASS_API */


/***************************************
*  Constants for USBFS_cdc API.
***************************************/

/* CDC Class-Specific Request Codes (CDC ver 1.2 Table 19) */
#define USBFS_CDC_SET_LINE_CODING        (0x20u)
#define USBFS_CDC_GET_LINE_CODING        (0x21u)
#define USBFS_CDC_SET_CONTROL_LINE_STATE (0x22u)

#define USBFS_LINE_CODING_CHANGED        (0x01u)
#define USBFS_LINE_CONTROL_CHANGED       (0x02u)

#define USBFS_1_STOPBIT                  (0x00u)
#define USBFS_1_5_STOPBITS               (0x01u)
#define USBFS_2_STOPBITS                 (0x02u)

#define USBFS_PARITY_NONE                (0x00u)
#define USBFS_PARITY_ODD                 (0x01u)
#define USBFS_PARITY_EVEN                (0x02u)
#define USBFS_PARITY_MARK                (0x03u)
#define USBFS_PARITY_SPACE               (0x04u)

#define USBFS_LINE_CODING_SIZE           (0x07u)
#define USBFS_LINE_CODING_RATE           (0x00u)
#define USBFS_LINE_CODING_STOP_BITS      (0x04u)
#define USBFS_LINE_CODING_PARITY         (0x05u)
#define USBFS_LINE_CODING_DATA_BITS      (0x06u)

#define USBFS_LINE_CONTROL_DTR           (0x01u)
#define USBFS_LINE_CONTROL_RTS           (0x02u)


/***************************************
* External data references
***************************************/

extern volatile uint8 USBFS_lineCoding[USBFS_LINE_CODING_SIZE];
extern volatile uint8 USBFS_lineChanged;
extern volatile uint16 USBFS_lineControlBitmap;
extern volatile uint8 USBFS_cdc_data_in_ep;
extern volatile uint8 USBFS_cdc_data_out_ep;

#endif /*  CY_USBFS_USBFS_cdc_H */


/* [] END OF FILE */
