/*******************************************************************************
* File Name: USBFS_cls.c
* Version 2.80
*
* Description:
*  USB Class request handler.
*
* Note:
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS.h"

#if(USBFS_EXTERN_CLS == USBFS_FALSE)

#include "USBFS_pvt.h"



/***************************************
* User Implemented Class Driver Declarations.
***************************************/
/* `#START USER_DEFINED_CLASS_DECLARATIONS` Place your declaration here */

/* `#END` */


/*******************************************************************************
* Function Name: USBFS_DispatchClassRqst
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
uint8 USBFS_DispatchClassRqst(void) 
{
    uint8 requestHandled = USBFS_FALSE;
    uint8 interfaceNumber = 0u;

    switch(CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_RCPT_MASK)
    {
        case USBFS_RQST_RCPT_IFC:        /* Class-specific request directed to an interface */
            interfaceNumber = CY_GET_REG8(USBFS_wIndexLo); /* wIndexLo contain Interface number */
            break;
        case USBFS_RQST_RCPT_EP:         /* Class-specific request directed to the endpoint */
            /* Find related interface to the endpoint, wIndexLo contain EP number */
            interfaceNumber = USBFS_EP[CY_GET_REG8(USBFS_wIndexLo) &
                              USBFS_DIR_UNUSED].interface;
            break;
        default:    /* RequestHandled is initialized as FALSE by default */
            break;
    }
    /* Handle Class request depend on interface type */
    switch(USBFS_interfaceClass[interfaceNumber])
    {
        case USBFS_CLASS_HID:
            #if defined(USBFS_ENABLE_HID_CLASS)
                requestHandled = USBFS_DispatchHIDClassRqst();
            #endif /* USBFS_ENABLE_HID_CLASS */
            break;
        case USBFS_CLASS_AUDIO:
            #if defined(USBFS_ENABLE_AUDIO_CLASS)
                requestHandled = USBFS_DispatchAUDIOClassRqst();
            #endif /* USBFS_CLASS_AUDIO */
            break;
        case USBFS_CLASS_CDC:
            #if defined(USBFS_ENABLE_CDC_CLASS)
                requestHandled = USBFS_DispatchCDCClassRqst();
            #endif /* USBFS_ENABLE_CDC_CLASS */
            break;
        default:    /* requestHandled is initialized as FALSE by default */
            break;
    }

    /* `#START USER_DEFINED_CLASS_CODE` Place your Class request here */

    /* `#END` */

    #ifdef USBFS_DISPATCH_CLASS_RQST_CALLBACK
        USBFS_DispatchClassRqst_Callback();
    #endif /* USBFS_DISPATCH_CLASS_RQST_CALLBACK */

    return(requestHandled);
}


/*******************************************************************************
* Additional user functions supporting Class Specific Requests
********************************************************************************/

/* `#START CLASS_SPECIFIC_FUNCTIONS` Place any additional functions here */

/* `#END` */

#endif /* USBFS_EXTERN_CLS */


/* [] END OF FILE */
