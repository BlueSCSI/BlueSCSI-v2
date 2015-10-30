/*******************************************************************************
* File Name: USBFS_audio.c
* Version 2.80
*
* Description:
*  USB AUDIO Class request handler.
*
* Related Document:
*  Universal Serial Bus Device Class Definition for Audio Devices Release 1.0
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "USBFS.h"


#if defined(USBFS_ENABLE_AUDIO_CLASS)

#include "USBFS_audio.h"
#include "USBFS_pvt.h"
#if defined(USBFS_ENABLE_MIDI_STREAMING)
    #include "USBFS_midi.h"
#endif /*  USBFS_ENABLE_MIDI_STREAMING*/


/***************************************
* Custom Declarations
***************************************/

/* `#START CUSTOM_DECLARATIONS` Place your declaration here */

/* `#END` */


#if !defined(USER_SUPPLIED_AUDIO_HANDLER)


/***************************************
*    AUDIO Variables
***************************************/

#if defined(USBFS_ENABLE_AUDIO_STREAMING)
    volatile uint8 USBFS_currentSampleFrequency[USBFS_MAX_EP][USBFS_SAMPLE_FREQ_LEN];
    volatile uint8 USBFS_frequencyChanged;
    volatile uint8 USBFS_currentMute;
    volatile uint8 USBFS_currentVolume[USBFS_VOLUME_LEN];
    volatile uint8 USBFS_minimumVolume[USBFS_VOLUME_LEN] = {USBFS_VOL_MIN_LSB,
                                                                                  USBFS_VOL_MIN_MSB};
    volatile uint8 USBFS_maximumVolume[USBFS_VOLUME_LEN] = {USBFS_VOL_MAX_LSB,
                                                                                  USBFS_VOL_MAX_MSB};
    volatile uint8 USBFS_resolutionVolume[USBFS_VOLUME_LEN] = {USBFS_VOL_RES_LSB,
                                                                                     USBFS_VOL_RES_MSB};
#endif /*  USBFS_ENABLE_AUDIO_STREAMING */


/*******************************************************************************
* Function Name: USBFS_DispatchAUDIOClassRqst
********************************************************************************
*
* Summary:
*  This routine dispatches class requests
*
* Parameters:
*  None.
*
* Return:
*  requestHandled
*
* Global variables:
*   USBFS_currentSampleFrequency: Contains the current audio Sample
*       Frequency. It is set by the Host using SET_CUR request to the endpoint.
*   USBFS_frequencyChanged: This variable is used as a flag for the
*       user code, to be aware that Host has been sent request for changing
*       Sample Frequency. Sample frequency will be sent on the next OUT
*       transaction. It is contains endpoint address when set. The following
*       code is recommended for detecting new Sample Frequency in main code:
*       if((USBFS_frequencyChanged != 0) &&
*       (USBFS_transferState == USBFS_TRANS_STATE_IDLE))
*       {
*          USBFS_frequencyChanged = 0;
*       }
*       USBFS_transferState variable is checked to be sure that
*             transfer completes.
*   USBFS_currentMute: Contains mute configuration set by Host.
*   USBFS_currentVolume: Contains volume level set by Host.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_DispatchAUDIOClassRqst(void) 
{
    uint8 requestHandled = USBFS_FALSE;
    uint8 bmRequestType  = CY_GET_REG8(USBFS_bmRequestType);

    #if defined(USBFS_ENABLE_AUDIO_STREAMING)
        uint8 epNumber;
        epNumber = CY_GET_REG8(USBFS_wIndexLo) & USBFS_DIR_UNUSED;
    #endif /*  USBFS_ENABLE_AUDIO_STREAMING */


    if ((bmRequestType & USBFS_RQST_DIR_MASK) == USBFS_RQST_DIR_D2H)
    {
        /* Control Read */
        if((bmRequestType & USBFS_RQST_RCPT_MASK) == USBFS_RQST_RCPT_EP)
        {
            /* Endpoint */
            switch (CY_GET_REG8(USBFS_bRequest))
            {
                case USBFS_GET_CUR:
                #if defined(USBFS_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_SAMPLING_FREQ_CONTROL)
                    {
                         /* point Control Selector is Sampling Frequency */
                        USBFS_currentTD.wCount = USBFS_SAMPLE_FREQ_LEN;
                        USBFS_currentTD.pData  = USBFS_currentSampleFrequency[epNumber];
                        requestHandled   = USBFS_InitControlRead();
                    }
                #endif /*  USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_READ_REQUESTS` Place other request handler here */

                /* `#END` */
                
                #ifdef USBFS_DISPATCH_AUDIO_CLASS_AUDIO_READ_REQUESTS_CALLBACK
                    USBFS_DispatchAUDIOClass_AUDIO_READ_REQUESTS_Callback();
                #endif /* USBFS_DISPATCH_AUDIO_CLASS_AUDIO_READ_REQUESTS_CALLBACK */

                    break;
                default:
                    break;
            }
        }
        else if((bmRequestType & USBFS_RQST_RCPT_MASK) == USBFS_RQST_RCPT_IFC)
        {
            /* Interface or Entity ID */
            switch (CY_GET_REG8(USBFS_bRequest))
            {
                case USBFS_GET_CUR:
                #if defined(USBFS_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_MUTE_CONTROL)
                    {
                        /* `#START MUTE_CONTROL_GET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        #ifdef USBFS_DISPATCH_AUDIO_CLASS_MUTE_CONTROL_GET_REQUEST_CALLBACK
                            USBFS_DispatchAUDIOClass_MUTE_CONTROL_GET_REQUEST_Callback();
                        #endif /* USBFS_DISPATCH_AUDIO_CLASS_MUTE_CONTROL_GET_REQUEST_CALLBACK */

                        /* Entity ID Control Selector is MUTE */
                        USBFS_currentTD.wCount = 1u;
                        USBFS_currentTD.pData  = &USBFS_currentMute;
                        requestHandled   = USBFS_InitControlRead();
                    }
                    else if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                        /* `#START VOLUME_CONTROL_GET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        #ifdef USBFS_DISPATCH_AUDIO_CLASS_VOLUME_CONTROL_GET_REQUEST_CALLBACK
                            USBFS_DispatchAUDIOClass_VOLUME_CONTROL_GET_REQUEST_Callback();
                        #endif /* USBFS_DISPATCH_AUDIO_CLASS_VOLUME_CONTROL_GET_REQUEST_CALLBACK */

                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = USBFS_currentVolume;
                        requestHandled   = USBFS_InitControlRead();
                    }
                    else
                    {
                        /* `#START OTHER_GET_CUR_REQUESTS` Place other request handler here */

                        /* `#END` */

                        #ifdef USBFS_DISPATCH_AUDIO_CLASS_OTHER_GET_CUR_REQUESTS_CALLBACK
                            USBFS_DispatchAUDIOClass_OTHER_GET_CUR_REQUESTS_Callback();
                        #endif /* USBFS_DISPATCH_AUDIO_CLASS_OTHER_GET_CUR_REQUESTS_CALLBACK */
                    }
                    break;
                case USBFS_GET_MIN:    /* GET_MIN */
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = &USBFS_minimumVolume[0];
                        requestHandled   = USBFS_InitControlRead();
                    }
                    break;
                case USBFS_GET_MAX:    /* GET_MAX */
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = &USBFS_maximumVolume[0];
                        requestHandled   = USBFS_InitControlRead();
                    }
                    break;
                case USBFS_GET_RES:    /* GET_RES */
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                         /* Entity ID Control Selector is VOLUME, */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = &USBFS_resolutionVolume[0];
                        requestHandled   = USBFS_InitControlRead();
                    }
                    break;
                /* The contents of the status message is reserved for future use.
                *  For the time being, a null packet should be returned in the data stage of the
                *  control transfer, and the received null packet should be ACKed.
                */
                case USBFS_GET_STAT:
                        USBFS_currentTD.wCount = 0u;
                        requestHandled   = USBFS_InitControlWrite();

                #endif /*  USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_WRITE_REQUESTS` Place other request handler here */

                /* `#END` */

                #ifdef USBFS_DISPATCH_AUDIO_CLASS_AUDIO_WRITE_REQUESTS_CALLBACK
                    USBFS_DispatchAUDIOClass_AUDIO_WRITE_REQUESTS_Callback();
                #endif /* USBFS_DISPATCH_AUDIO_CLASS_AUDIO_WRITE_REQUESTS_CALLBACK */

                    break;
                default:
                    break;
            }
        }
        else
        {   /* USBFS_RQST_RCPT_OTHER */
        }
    }
    else
    {
        /* Control Write */
        if((bmRequestType & USBFS_RQST_RCPT_MASK) == USBFS_RQST_RCPT_EP)
        {
            /* point */
            switch (CY_GET_REG8(USBFS_bRequest))
            {
                case USBFS_SET_CUR:
                #if defined(USBFS_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_SAMPLING_FREQ_CONTROL)
                    {
                         /* point Control Selector is Sampling Frequency */
                        USBFS_currentTD.wCount = USBFS_SAMPLE_FREQ_LEN;
                        USBFS_currentTD.pData  = USBFS_currentSampleFrequency[epNumber];
                        requestHandled   = USBFS_InitControlWrite();
                        USBFS_frequencyChanged = epNumber;
                    }
                #endif /*  USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_SAMPLING_FREQ_REQUESTS` Place other request handler here */

                /* `#END` */

                #ifdef USBFS_DISPATCH_AUDIO_CLASS_AUDIO_SAMPLING_FREQ_REQUESTS_CALLBACK
                    USBFS_DispatchAUDIOClass_AUDIO_SAMPLING_FREQ_REQUESTS_Callback();
                #endif /* USBFS_DISPATCH_AUDIO_CLASS_AUDIO_SAMPLING_FREQ_REQUESTS_CALLBACK */

                    break;
                default:
                    break;
            }
        }
        else if((bmRequestType & USBFS_RQST_RCPT_MASK) == USBFS_RQST_RCPT_IFC)
        {
            /* Interface or Entity ID */
            switch (CY_GET_REG8(USBFS_bRequest))
            {
                case USBFS_SET_CUR:
                #if defined(USBFS_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_MUTE_CONTROL)
                    {
                        /* `#START MUTE_SET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        #ifdef USBFS_DISPATCH_AUDIO_CLASS_MUTE_SET_REQUEST_CALLBACK
                            USBFS_DispatchAUDIOClass_MUTE_SET_REQUEST_Callback();
                        #endif /* USBFS_DISPATCH_AUDIO_CLASS_MUTE_SET_REQUEST_CALLBACK */

                        /* Entity ID Control Selector is MUTE */
                        USBFS_currentTD.wCount = 1u;
                        USBFS_currentTD.pData  = &USBFS_currentMute;
                        requestHandled   = USBFS_InitControlWrite();
                    }
                    else if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                        /* `#START VOLUME_CONTROL_SET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        #ifdef USBFS_DISPATCH_AUDIO_CLASS_VOLUME_CONTROL_SET_REQUEST_CALLBACK
                            USBFS_DispatchAUDIOClass_VOLUME_CONTROL_SET_REQUEST_Callback();
                        #endif /* USBFS_DISPATCH_AUDIO_CLASS_VOLUME_CONTROL_SET_REQUEST_CALLBACK */

                        /* Entity ID Control Selector is VOLUME */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = USBFS_currentVolume;
                        requestHandled   = USBFS_InitControlWrite();
                    }
                    else
                    {
                        /* `#START OTHER_SET_CUR_REQUESTS` Place other request handler here */

                        /* `#END` */

                        #ifdef USBFS_DISPATCH_AUDIO_CLASS_OTHER_SET_CUR_REQUESTS_CALLBACK
                            USBFS_DispatchAUDIOClass_OTHER_SET_CUR_REQUESTS_Callback();
                        #endif /* USBFS_DISPATCH_AUDIO_CLASS_OTHER_SET_CUR_REQUESTS_CALLBACK */
                    }
                #endif /*  USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_CONTROL_SEL_REQUESTS` Place other request handler here */

                /* `#END` */

                #ifdef USBFS_DISPATCH_AUDIO_CLASS_AUDIO_CONTROL_SEL_REQUESTS_CALLBACK
                    USBFS_DispatchAUDIOClass_AUDIO_CONTROL_SEL_REQUESTS_Callback();
                #endif /* USBFS_DISPATCH_AUDIO_CLASS_AUDIO_CONTROL_SEL_REQUESTS_CALLBACK */

                    break;
                default:
                    break;
            }
        }
        else
        {
            /* USBFS_RQST_RCPT_OTHER */
        }
    }

    return(requestHandled);
}

#endif /* USER_SUPPLIED_AUDIO_HANDLER */


/*******************************************************************************
* Additional user functions supporting AUDIO Requests
********************************************************************************/

/* `#START AUDIO_FUNCTIONS` Place any additional functions here */

/* `#END` */

#endif  /*  USBFS_ENABLE_AUDIO_CLASS */


/* [] END OF FILE */
