/*******************************************************************************
* File Name: USBFS_1_cdc.c
* Version 2.60
*
* Description:
*  USB HID Class request handler.
*
* Note:
*
********************************************************************************
* Copyright 2012-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS_1.h"

#if defined(USBFS_1_ENABLE_CDC_CLASS)

#include "USBFS_1_cdc.h"
#include "USBFS_1_pvt.h"


/***************************************
*    CDC Variables
***************************************/

volatile uint8 USBFS_1_lineCoding[USBFS_1_LINE_CODING_SIZE];
volatile uint8 USBFS_1_lineChanged;
volatile uint16 USBFS_1_lineControlBitmap;
volatile uint8 USBFS_1_cdc_data_in_ep;
volatile uint8 USBFS_1_cdc_data_out_ep;


/***************************************
*     Static Function Prototypes
***************************************/
static uint16 USBFS_1_StrLen(const char8 string[]) ;


/***************************************
* Custom Declarations
***************************************/

/* `#START CDC_CUSTOM_DECLARATIONS` Place your declaration here */

/* `#END` */


/*******************************************************************************
* Function Name: USBFS_1_DispatchCDCClassRqst
********************************************************************************
*
* Summary:
*  This routine dispatches CDC class requests.
*
* Parameters:
*  None.
*
* Return:
*  requestHandled
*
* Global variables:
*   USBFS_1_lineCoding: Contains the current line coding structure.
*     It is set by the Host using SET_LINE_CODING request and returned to the
*     user code by the USBFS_GetDTERate(), USBFS_GetCharFormat(),
*     USBFS_GetParityType(), USBFS_GetDataBits() APIs.
*   USBFS_1_lineControlBitmap: Contains the current control signal
*     bitmap. It is set by the Host using SET_CONTROL_LINE request and returned
*     to the user code by the USBFS_GetLineControl() API.
*   USBFS_1_lineChanged: This variable is used as a flag for the
*     USBFS_IsLineChanged() API, to be aware that Host has been sent request
*     for changing Line Coding or Control Bitmap.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_DispatchCDCClassRqst(void) 
{
    uint8 requestHandled = USBFS_1_FALSE;

    if ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_DIR_MASK) == USBFS_1_RQST_DIR_D2H)
    {   /* Control Read */
        switch (CY_GET_REG8(USBFS_1_bRequest))
        {
            case USBFS_1_CDC_GET_LINE_CODING:
                USBFS_1_currentTD.count = USBFS_1_LINE_CODING_SIZE;
                USBFS_1_currentTD.pData = USBFS_1_lineCoding;
                requestHandled  = USBFS_1_InitControlRead();
                break;

            /* `#START CDC_READ_REQUESTS` Place other request handler here */

            /* `#END` */

            default:    /* requestHandled is initialized as FALSE by default */
                break;
        }
    }
    else if ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_DIR_MASK) == \
                                                                            USBFS_1_RQST_DIR_H2D)
    {   /* Control Write */
        switch (CY_GET_REG8(USBFS_1_bRequest))
        {
            case USBFS_1_CDC_SET_LINE_CODING:
                USBFS_1_currentTD.count = USBFS_1_LINE_CODING_SIZE;
                USBFS_1_currentTD.pData = USBFS_1_lineCoding;
                USBFS_1_lineChanged |= USBFS_1_LINE_CODING_CHANGED;
                requestHandled = USBFS_1_InitControlWrite();
                break;

            case USBFS_1_CDC_SET_CONTROL_LINE_STATE:
                USBFS_1_lineControlBitmap = CY_GET_REG8(USBFS_1_wValueLo);
                USBFS_1_lineChanged |= USBFS_1_LINE_CONTROL_CHANGED;
                requestHandled = USBFS_1_InitNoDataControlTransfer();
                break;

            /* `#START CDC_WRITE_REQUESTS` Place other request handler here */

            /* `#END` */

            default:    /* requestHandled is initialized as FALSE by default */
                break;
        }
    }
    else
    {   /* requestHandled is initialized as FALSE by default */
    }

    return(requestHandled);
}


/***************************************
* Optional CDC APIs
***************************************/
#if (USBFS_1_ENABLE_CDC_CLASS_API != 0u)


    /*******************************************************************************
    * Function Name: USBFS_1_CDC_Init
    ********************************************************************************
    *
    * Summary:
    *  This function initialize the CDC interface to be ready for the receive data
    *  from the PC.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    * Global variables:
    *   USBFS_1_lineChanged: Initialized to zero.
    *   USBFS_1_cdc_data_out_ep: Used as an OUT endpoint number.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    void USBFS_1_CDC_Init(void) 
    {
        USBFS_1_lineChanged = 0u;
        USBFS_1_EnableOutEP(USBFS_1_cdc_data_out_ep);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_PutData
    ********************************************************************************
    *
    * Summary:
    *  Sends a specified number of bytes from the location specified by a
    *  pointer to the PC.
    *
    * Parameters:
    *  pData: pointer to the buffer containing data to be sent.
    *  length: Specifies the number of bytes to send from the pData
    *  buffer. Maximum length will be limited by the maximum packet
    *  size for the endpoint.
    *
    * Return:
    *  None.
    *
    * Global variables:
    *   USBFS_1_cdc_data_in_ep: CDC IN endpoint number used for sending
    *     data.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    void USBFS_1_PutData(const uint8* pData, uint16 length) 
    {
        /* Limits length to maximum packet size for the EP */
        if(length > USBFS_1_EP[USBFS_1_cdc_data_in_ep].bufferSize)
        {
            /* Caution: Data will be lost if length is greater than Max Packet Length */
            length = USBFS_1_EP[USBFS_1_cdc_data_in_ep].bufferSize;
             /* Halt CPU in debug mode */
            CYASSERT(0u != 0u);
        }
        USBFS_1_LoadInEP(USBFS_1_cdc_data_in_ep, pData, length);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_StrLen
    ********************************************************************************
    *
    * Summary:
    *  Calculates length of a null terminated string.
    *
    * Parameters:
    *  string: pointer to the string.
    *
    * Return:
    *  Length of the string
    *
    *******************************************************************************/
    static uint16 USBFS_1_StrLen(const char8 string[]) 
    {
        uint16 len = 0u;

        while (string[len] != (char8)0)
        {
            len++;
        }

        return (len);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_PutString
    ********************************************************************************
    *
    * Summary:
    *  Sends a null terminated string to the PC.
    *
    * Parameters:
    *  string: pointer to the string to be sent to the PC
    *
    * Return:
    *  None.
    *
    * Global variables:
    *   USBFS_1_cdc_data_in_ep: CDC IN endpoint number used for sending
    *     data.
    *
    * Reentrant:
    *  No.
    *
    * Theory:
    *  This function will block if there is not enough memory to place the whole
    *  string, it will block until the entire string has been written to the
    *  transmit buffer.
    *
    *******************************************************************************/
    void USBFS_1_PutString(const char8 string[]) 
    {
        uint16 str_length;
        uint16 send_length;
        uint16 buf_index = 0u;

        /* Get length of the null terminated string */
        str_length = USBFS_1_StrLen(string);
        do
        {
            /* Limits length to maximum packet size for the EP */
            send_length = (str_length > USBFS_1_EP[USBFS_1_cdc_data_in_ep].bufferSize) ?
                          USBFS_1_EP[USBFS_1_cdc_data_in_ep].bufferSize : str_length;
             /* Enable IN transfer */
            USBFS_1_LoadInEP(USBFS_1_cdc_data_in_ep, (const uint8 *)&string[buf_index], send_length);
            str_length -= send_length;

            /* If more data are present to send */
            if(str_length > 0u)
            {
                buf_index += send_length;
                /* Wait for the Host to read it. */
                while(USBFS_1_EP[USBFS_1_cdc_data_in_ep].apiEpState ==
                                          USBFS_1_IN_BUFFER_FULL)
                {
                    ;
                }
            }
        }while(str_length > 0u);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_PutChar
    ********************************************************************************
    *
    * Summary:
    *  Writes a single character to the PC.
    *
    * Parameters:
    *  txDataByte: Character to be sent to the PC.
    *
    * Return:
    *  None.
    *
    * Global variables:
    *   USBFS_1_cdc_data_in_ep: CDC IN endpoint number used for sending
    *     data.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    void USBFS_1_PutChar(char8 txDataByte) 
    {
        uint8 dataByte;
        dataByte = (uint8)txDataByte;

        USBFS_1_LoadInEP(USBFS_1_cdc_data_in_ep, &dataByte, 1u);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_PutCRLF
    ********************************************************************************
    *
    * Summary:
    *  Sends a carriage return (0x0D) and line feed (0x0A) to the PC
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    * Global variables:
    *   USBFS_1_cdc_data_in_ep: CDC IN endpoint number used for sending
    *     data.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    void USBFS_1_PutCRLF(void) 
    {
        const uint8 CYCODE txData[] = {0x0Du, 0x0Au};

        USBFS_1_LoadInEP(USBFS_1_cdc_data_in_ep, (const uint8 *)txData, 2u);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetCount
    ********************************************************************************
    *
    * Summary:
    *  This function returns the number of bytes that were received from the PC.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Returns the number of received bytes.
    *
    * Global variables:
    *   USBFS_1_cdc_data_out_ep: CDC OUT endpoint number used.
    *
    *******************************************************************************/
    uint16 USBFS_1_GetCount(void) 
    {
        uint16 bytesCount = 0u;

        if (USBFS_1_EP[USBFS_1_cdc_data_out_ep].apiEpState == USBFS_1_OUT_BUFFER_FULL)
        {
            bytesCount = USBFS_1_GetEPCount(USBFS_1_cdc_data_out_ep);
        }

        return(bytesCount);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_DataIsReady
    ********************************************************************************
    *
    * Summary:
    *  Returns a nonzero value if the component received data or received
    *  zero-length packet. The GetAll() or GetData() API should be called to read
    *  data from the buffer and re-init OUT endpoint even when zero-length packet
    *  received.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  If the OUT packet received this function returns a nonzero value.
    *  Otherwise zero is returned.
    *
    * Global variables:
    *   USBFS_1_cdc_data_out_ep: CDC OUT endpoint number used.
    *
    *******************************************************************************/
    uint8 USBFS_1_DataIsReady(void) 
    {
        return(USBFS_1_EP[USBFS_1_cdc_data_out_ep].apiEpState);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_CDCIsReady
    ********************************************************************************
    *
    * Summary:
    *  Returns a nonzero value if the component is ready to send more data to the
    *  PC. Otherwise returns zero. Should be called before sending new data to
    *  ensure the previous data has finished sending.This function returns the
    *  number of bytes that were received from the PC.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  If the buffer can accept new data then this function returns a nonzero value.
    *  Otherwise zero is returned.
    *
    * Global variables:
    *   USBFS_1_cdc_data_in_ep: CDC IN endpoint number used.
    *
    *******************************************************************************/
    uint8 USBFS_1_CDCIsReady(void) 
    {
        return(USBFS_1_EP[USBFS_1_cdc_data_in_ep].apiEpState);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetData
    ********************************************************************************
    *
    * Summary:
    *  Gets a specified number of bytes from the input buffer and places it in a
    *  data array specified by the passed pointer.
    *  USBFS_1_DataIsReady() API should be called before, to be sure
    *  that data is received from the Host.
    *
    * Parameters:
    *  pData: Pointer to the data array where data will be placed.
    *  Length: Number of bytes to read into the data array from the RX buffer.
    *          Maximum length is limited by the the number of received bytes.
    *
    * Return:
    *  Number of bytes received.
    *
    * Global variables:
    *   USBFS_1_cdc_data_out_ep: CDC OUT endpoint number used.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    uint16 USBFS_1_GetData(uint8* pData, uint16 length) 
    {
        return(USBFS_1_ReadOutEP(USBFS_1_cdc_data_out_ep, pData, length));
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetAll
    ********************************************************************************
    *
    * Summary:
    *  Gets all bytes of received data from the input buffer and places it into a
    *  specified data array. USBFS_1_DataIsReady() API should be called
    *  before, to be sure that data is received from the Host.
    *
    * Parameters:
    *  pData: Pointer to the data array where data will be placed.
    *
    * Return:
    *  Number of bytes received.
    *
    * Global variables:
    *   USBFS_1_cdc_data_out_ep: CDC OUT endpoint number used.
    *   USBFS_1_EP[].bufferSize: EP max packet size is used as a length
    *     to read all data from the EP buffer.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    uint16 USBFS_1_GetAll(uint8* pData) 
    {
        return (USBFS_1_ReadOutEP(USBFS_1_cdc_data_out_ep, pData,
                                           USBFS_1_EP[USBFS_1_cdc_data_out_ep].bufferSize));
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetChar
    ********************************************************************************
    *
    * Summary:
    *  Reads one byte of received data from the buffer.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Received one character.
    *
    * Global variables:
    *   USBFS_1_cdc_data_out_ep: CDC OUT endpoint number used.
    *
    * Reentrant:
    *  No.
    *
    *******************************************************************************/
    uint8 USBFS_1_GetChar(void) 
    {
         uint8 rxData;

        (void) USBFS_1_ReadOutEP(USBFS_1_cdc_data_out_ep, &rxData, 1u);

        return(rxData);
    }

    /*******************************************************************************
    * Function Name: USBFS_1_IsLineChanged
    ********************************************************************************
    *
    * Summary:
    *  This function returns clear on read status of the line.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  If SET_LINE_CODING or CDC_SET_CONTROL_LINE_STATE request received then not
    *  zero value returned. Otherwise zero is returned.
    *
    * Global variables:
    *  USBFS_1_transferState - it is checked to be sure then OUT data
    *    phase has been complete, and data written to the lineCoding or Control
    *    Bitmap buffer.
    *  USBFS_1_lineChanged: used as a flag to be aware that Host has been
    *    sent request for changing Line Coding or Control Bitmap.
    *
    *******************************************************************************/
    uint8 USBFS_1_IsLineChanged(void) 
    {
        uint8 state = 0u;

        /* transferState is checked to be sure then OUT data phase has been complete */
        if(USBFS_1_transferState == USBFS_1_TRANS_STATE_IDLE)
        {
            if(USBFS_1_lineChanged != 0u)
            {
                state = USBFS_1_lineChanged;
                USBFS_1_lineChanged = 0u;
            }
        }

        return(state);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetDTERate
    ********************************************************************************
    *
    * Summary:
    *  Returns the data terminal rate set for this port in bits per second.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Returns a uint32 value of the data rate in bits per second.
    *
    * Global variables:
    *  USBFS_1_lineCoding: First four bytes converted to uint32
    *    depend on compiler, and returned as a data rate.
    *
    *******************************************************************************/
    uint32 USBFS_1_GetDTERate(void) 
    {
        uint32 rate;

        rate = USBFS_1_lineCoding[USBFS_1_LINE_CODING_RATE + 3u];
        rate = (rate << 8u) | USBFS_1_lineCoding[USBFS_1_LINE_CODING_RATE + 2u];
        rate = (rate << 8u) | USBFS_1_lineCoding[USBFS_1_LINE_CODING_RATE + 1u];
        rate = (rate << 8u) | USBFS_1_lineCoding[USBFS_1_LINE_CODING_RATE];

        return(rate);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetCharFormat
    ********************************************************************************
    *
    * Summary:
    *  Returns the number of stop bits.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Returns the number of stop bits.
    *
    * Global variables:
    *  USBFS_1_lineCoding: used to get a parameter.
    *
    *******************************************************************************/
    uint8 USBFS_1_GetCharFormat(void) 
    {
        return(USBFS_1_lineCoding[USBFS_1_LINE_CODING_STOP_BITS]);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetParityType
    ********************************************************************************
    *
    * Summary:
    *  Returns the parity type for the CDC port.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Returns the parity type.
    *
    * Global variables:
    *  USBFS_1_lineCoding: used to get a parameter.
    *
    *******************************************************************************/
    uint8 USBFS_1_GetParityType(void) 
    {
        return(USBFS_1_lineCoding[USBFS_1_LINE_CODING_PARITY]);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetDataBits
    ********************************************************************************
    *
    * Summary:
    *  Returns the number of data bits for the CDC port.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Returns the number of data bits.
    *  The number of data bits can be 5, 6, 7, 8 or 16.
    *
    * Global variables:
    *  USBFS_1_lineCoding: used to get a parameter.
    *
    *******************************************************************************/
    uint8 USBFS_1_GetDataBits(void) 
    {
        return(USBFS_1_lineCoding[USBFS_1_LINE_CODING_DATA_BITS]);
    }


    /*******************************************************************************
    * Function Name: USBFS_1_GetLineControl
    ********************************************************************************
    *
    * Summary:
    *  Returns Line control bitmap.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  Returns Line control bitmap.
    *
    * Global variables:
    *  USBFS_1_lineControlBitmap: used to get a parameter.
    *
    *******************************************************************************/
    uint16 USBFS_1_GetLineControl(void) 
    {
        return(USBFS_1_lineControlBitmap);
    }

#endif  /* End USBFS_1_ENABLE_CDC_CLASS_API*/


/*******************************************************************************
* Additional user functions supporting CDC Requests
********************************************************************************/

/* `#START CDC_FUNCTIONS` Place any additional functions here */

/* `#END` */

#endif  /* End USBFS_1_ENABLE_CDC_CLASS*/


/* [] END OF FILE */
