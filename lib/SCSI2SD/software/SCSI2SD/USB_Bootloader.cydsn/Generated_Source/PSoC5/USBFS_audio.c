/*******************************************************************************
* File Name: USBFS_audio.c
* Version 2.60
*
* Description:
*  USB AUDIO Class request handler.
*
* Note:
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
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
#endif /* End USBFS_ENABLE_MIDI_STREAMING*/


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
#endif /* End USBFS_ENABLE_AUDIO_STREAMING */


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

    #if defined(USBFS_ENABLE_AUDIO_STREAMING)
        uint8 epNumber;
        epNumber = CY_GET_REG8(USBFS_wIndexLo) & USBFS_DIR_UNUSED;
    #endif /* End USBFS_ENABLE_AUDIO_STREAMING */

    if ((CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_DIR_MASK) == USBFS_RQST_DIR_D2H)
    {
        /* Control Read */
        if((CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_RCPT_MASK) == \
                                                                                    USBFS_RQST_RCPT_EP)
        {
            /* Endpoint */
            switch (CY_GET_REG8(USBFS_bRequest))
            {
                case USBFS_GET_CUR:
                #if defined(USBFS_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_SAMPLING_FREQ_CONTROL)
                    {
                         /* Endpoint Control Selector is Sampling Frequency */
                        USBFS_currentTD.wCount = USBFS_SAMPLE_FREQ_LEN;
                        USBFS_currentTD.pData  = USBFS_currentSampleFrequency[epNumber];
                        requestHandled   = USBFS_InitControlRead();
                    }
                #endif /* End USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_READ_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else if((CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_RCPT_MASK) == \
                                                                                    USBFS_RQST_RCPT_IFC)
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
                        
                         /* Entity ID Control Selector is MUTE */
                        USBFS_currentTD.wCount = 1u;
                        USBFS_currentTD.pData  = &USBFS_currentMute;
                        requestHandled   = USBFS_InitControlRead();
                    }
                    else if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                        /* `#START VOLUME_CONTROL_GET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = USBFS_currentVolume;
                        requestHandled   = USBFS_InitControlRead();
                    }
                    else
                    {
                        /* `#START OTHER_GET_CUR_REQUESTS` Place other request handler here */

                        /* `#END` */
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

                #endif /* End USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_WRITE_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else
        {   /* USBFS_RQST_RCPT_OTHER */
        }
    }
    else if ((CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_DIR_MASK) == \
                                                                                    USBFS_RQST_DIR_H2D)
    {
        /* Control Write */
        if((CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_RCPT_MASK) == \
                                                                                    USBFS_RQST_RCPT_EP)
        {
            /* Endpoint */
            switch (CY_GET_REG8(USBFS_bRequest))
            {
                case USBFS_SET_CUR:
                #if defined(USBFS_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_wValueHi) == USBFS_SAMPLING_FREQ_CONTROL)
                    {
                         /* Endpoint Control Selector is Sampling Frequency */
                        USBFS_currentTD.wCount = USBFS_SAMPLE_FREQ_LEN;
                        USBFS_currentTD.pData  = USBFS_currentSampleFrequency[epNumber];
                        requestHandled   = USBFS_InitControlWrite();
                        USBFS_frequencyChanged = epNumber;
                    }
                #endif /* End USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_SAMPLING_FREQ_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else if((CY_GET_REG8(USBFS_bmRequestType) & USBFS_RQST_RCPT_MASK) == \
                                                                                    USBFS_RQST_RCPT_IFC)
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

                        /* Entity ID Control Selector is MUTE */
                        USBFS_currentTD.wCount = 1u;
                        USBFS_currentTD.pData  = &USBFS_currentMute;
                        requestHandled   = USBFS_InitControlWrite();
                    }
                    else if(CY_GET_REG8(USBFS_wValueHi) == USBFS_VOLUME_CONTROL)
                    {
                        /* `#START VOLUME_CONTROL_SET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        /* Entity ID Control Selector is VOLUME */
                        USBFS_currentTD.wCount = USBFS_VOLUME_LEN;
                        USBFS_currentTD.pData  = USBFS_currentVolume;
                        requestHandled   = USBFS_InitControlWrite();
                    }
                    else
                    {
                        /* `#START OTHER_SET_CUR_REQUESTS` Place other request handler here */

                        /* `#END` */
                    }
                #endif /* End USBFS_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_CONTROL_SEL_REQUESTS` Place other request handler here */

                /* `#END` */
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
    {   /* requestHandled is initialized as FALSE by default */
    }

    return(requestHandled);
}


#endif /* USER_SUPPLIED_AUDIO_HANDLER */


/*******************************************************************************
* Additional user functions supporting AUDIO Requests
********************************************************************************/

/* `#START AUDIO_FUNCTIONS` Place any additional functions here */

/* `#END` */

#endif  /* End USBFS_ENABLE_AUDIO_CLASS*/


/* [] END OF FILE */
