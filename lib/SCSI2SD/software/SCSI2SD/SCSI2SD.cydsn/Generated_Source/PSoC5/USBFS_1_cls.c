/*******************************************************************************
* File Name: USBFS_1_cls.c
* Version 2.60
*
* Description:
*  USB Class request handler.
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

#if(USBFS_1_EXTERN_CLS == USBFS_1_FALSE)

#include "USBFS_1_pvt.h"


/***************************************
* User Implemented Class Driver Declarations.
***************************************/
/* `#START USER_DEFINED_CLASS_DECLARATIONS` Place your declaration here */

/* `#END` */


/*******************************************************************************
* Function Name: USBFS_1_DispatchClassRqst
********************************************************************************
* Summary:
*  This routine dispatches class specific requests depend on interface class.
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
uint8 USBFS_1_DispatchClassRqst(void) 
{
    uint8 requestHandled = USBFS_1_FALSE;
    uint8 interfaceNumber = 0u;

    switch(CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK)
    {
        case USBFS_1_RQST_RCPT_IFC:        /* Class-specific request directed to an interface */
            interfaceNumber = CY_GET_REG8(USBFS_1_wIndexLo); /* wIndexLo contain Interface number */
            break;
        case USBFS_1_RQST_RCPT_EP:         /* Class-specific request directed to the endpoint */
            /* Find related interface to the endpoint, wIndexLo contain EP number */
            interfaceNumber =
                USBFS_1_EP[CY_GET_REG8(USBFS_1_wIndexLo) & USBFS_1_DIR_UNUSED].interface;
            break;
        default:    /* RequestHandled is initialized as FALSE by default */
            break;
    }
    /* Handle Class request depend on interface type */
    switch(USBFS_1_interfaceClass[interfaceNumber])
    {
        case USBFS_1_CLASS_HID:
            #if defined(USBFS_1_ENABLE_HID_CLASS)
                requestHandled = USBFS_1_DispatchHIDClassRqst();
            #endif /* USBFS_1_ENABLE_HID_CLASS */
            break;
        case USBFS_1_CLASS_AUDIO:
            #if defined(USBFS_1_ENABLE_AUDIO_CLASS)
                requestHandled = USBFS_1_DispatchAUDIOClassRqst();
            #endif /* USBFS_1_ENABLE_HID_CLASS */
            break;
        case USBFS_1_CLASS_CDC:
            #if defined(USBFS_1_ENABLE_CDC_CLASS)
                requestHandled = USBFS_1_DispatchCDCClassRqst();
            #endif /* USBFS_1_ENABLE_CDC_CLASS */
            break;
        default:    /* requestHandled is initialized as FALSE by default */
            break;
    }

    /* `#START USER_DEFINED_CLASS_CODE` Place your Class request here */

    /* `#END` */

    return(requestHandled);
}


/*******************************************************************************
* Additional user functions supporting Class Specific Requests
********************************************************************************/

/* `#START CLASS_SPECIFIC_FUNCTIONS` Place any additional functions here */

/* `#END` */

#endif /* USBFS_1_EXTERN_CLS */


/* [] END OF FILE */
