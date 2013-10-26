/*******************************************************************************
* File Name: USBFS_1_vnd.c
* Version 2.60
*
* Description:
*  USB vendor request handler.
*
* Note:
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS_1.h"
#include "USBFS_1_pvt.h"

#if(USBFS_1_EXTERN_VND == USBFS_1_FALSE)


/***************************************
* Vendor Specific Declarations
***************************************/

/* `#START VENDOR_SPECIFIC_DECLARATIONS` Place your declaration here */

/* `#END` */


/*******************************************************************************
* Function Name: USBFS_1_HandleVendorRqst
********************************************************************************
*
* Summary:
*  This routine provide users with a method to implement vendor specifc
*  requests.
*
*  To implement vendor specific requests, add your code in this function to
*  decode and disposition the request.  If the request is handled, your code
*  must set the variable "requestHandled" to TRUE, indicating that the
*  request has been handled.
*
* Parameters:
*  None.
*
* Return:
*  requestHandled.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_HandleVendorRqst(void) 
{
    uint8 requestHandled = USBFS_1_FALSE;

    if ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_DIR_MASK) == USBFS_1_RQST_DIR_D2H)
    {
        /* Control Read */
        switch (CY_GET_REG8(USBFS_1_bRequest))
        {
            case USBFS_1_GET_EXTENDED_CONFIG_DESCRIPTOR:
                #if defined(USBFS_1_ENABLE_MSOS_STRING)
                    USBFS_1_currentTD.pData = (volatile uint8 *)&USBFS_1_MSOS_CONFIGURATION_DESCR[0u];
                    USBFS_1_currentTD.count = USBFS_1_MSOS_CONFIGURATION_DESCR[0u];
                    requestHandled  = USBFS_1_InitControlRead();
                #endif /* End USBFS_1_ENABLE_MSOS_STRING */
                break;
            default:
                break;
        }
    }

    /* `#START VENDOR_SPECIFIC_CODE` Place your vendor specific request here */

    /* `#END` */

    return(requestHandled);
}


/*******************************************************************************
* Additional user functions supporting Vendor Specific Requests
********************************************************************************/

/* `#START VENDOR_SPECIFIC_FUNCTIONS` Place any additional functions here */

/* `#END` */


#endif /* USBFS_1_EXTERN_VND */


/* [] END OF FILE */
