/*******************************************************************************
* File Name: USBFS_1_audio.c
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

#include "USBFS_1.h"

#if defined(USBFS_1_ENABLE_AUDIO_CLASS)

#include "USBFS_1_audio.h"
#include "USBFS_1_pvt.h"
#if defined(USBFS_1_ENABLE_MIDI_STREAMING) 
    #include "USBFS_1_midi.h"
#endif /* End USBFS_1_ENABLE_MIDI_STREAMING*/


/***************************************
* Custom Declarations
***************************************/

/* `#START CUSTOM_DECLARATIONS` Place your declaration here */

/* `#END` */


#if !defined(USER_SUPPLIED_AUDIO_HANDLER)


/***************************************
*    AUDIO Variables
***************************************/

#if defined(USBFS_1_ENABLE_AUDIO_STREAMING)
    volatile uint8 USBFS_1_currentSampleFrequency[USBFS_1_MAX_EP][USBFS_1_SAMPLE_FREQ_LEN];
    volatile uint8 USBFS_1_frequencyChanged;
    volatile uint8 USBFS_1_currentMute;
    volatile uint8 USBFS_1_currentVolume[USBFS_1_VOLUME_LEN];
    volatile uint8 USBFS_1_minimumVolume[USBFS_1_VOLUME_LEN] = {USBFS_1_VOL_MIN_LSB,
                                                                                  USBFS_1_VOL_MIN_MSB};
    volatile uint8 USBFS_1_maximumVolume[USBFS_1_VOLUME_LEN] = {USBFS_1_VOL_MAX_LSB,
                                                                                  USBFS_1_VOL_MAX_MSB};
    volatile uint8 USBFS_1_resolutionVolume[USBFS_1_VOLUME_LEN] = {USBFS_1_VOL_RES_LSB,
                                                                                     USBFS_1_VOL_RES_MSB};
#endif /* End USBFS_1_ENABLE_AUDIO_STREAMING */


/*******************************************************************************
* Function Name: USBFS_1_DispatchAUDIOClassRqst
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
*   USBFS_1_currentSampleFrequency: Contains the current audio Sample
*       Frequency. It is set by the Host using SET_CUR request to the endpoint.
*   USBFS_1_frequencyChanged: This variable is used as a flag for the
*       user code, to be aware that Host has been sent request for changing
*       Sample Frequency. Sample frequency will be sent on the next OUT
*       transaction. It is contains endpoint address when set. The following
*       code is recommended for detecting new Sample Frequency in main code:
*       if((USBFS_1_frequencyChanged != 0) &&
*       (USBFS_1_transferState == USBFS_1_TRANS_STATE_IDLE))
*       {
*          USBFS_1_frequencyChanged = 0;
*       }
*       USBFS_1_transferState variable is checked to be sure that
*             transfer completes.
*   USBFS_1_currentMute: Contains mute configuration set by Host.
*   USBFS_1_currentVolume: Contains volume level set by Host.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 USBFS_1_DispatchAUDIOClassRqst(void) 
{
    uint8 requestHandled = USBFS_1_FALSE;

    #if defined(USBFS_1_ENABLE_AUDIO_STREAMING)
        uint8 epNumber;
        epNumber = CY_GET_REG8(USBFS_1_wIndexLo) & USBFS_1_DIR_UNUSED;
    #endif /* End USBFS_1_ENABLE_AUDIO_STREAMING */

    if ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_DIR_MASK) == USBFS_1_RQST_DIR_D2H)
    {
        /* Control Read */
        if((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK) == \
                                                                                    USBFS_1_RQST_RCPT_EP)
        {
            /* Endpoint */
            switch (CY_GET_REG8(USBFS_1_bRequest))
            {
                case USBFS_1_GET_CUR:
                #if defined(USBFS_1_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_SAMPLING_FREQ_CONTROL)
                    {
                         /* Endpoint Control Selector is Sampling Frequency */
                        USBFS_1_currentTD.wCount = USBFS_1_SAMPLE_FREQ_LEN;
                        USBFS_1_currentTD.pData  = USBFS_1_currentSampleFrequency[epNumber];
                        requestHandled   = USBFS_1_InitControlRead();
                    }
                #endif /* End USBFS_1_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_READ_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else if((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK) == \
                                                                                    USBFS_1_RQST_RCPT_IFC)
        {
            /* Interface or Entity ID */
            switch (CY_GET_REG8(USBFS_1_bRequest))
            {
                case USBFS_1_GET_CUR:
                #if defined(USBFS_1_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_MUTE_CONTROL)
                    {
                        /* `#START MUTE_CONTROL_GET_REQUEST` Place multi-channel handler here */

                        /* `#END` */
                        
                         /* Entity ID Control Selector is MUTE */
                        USBFS_1_currentTD.wCount = 1u;
                        USBFS_1_currentTD.pData  = &USBFS_1_currentMute;
                        requestHandled   = USBFS_1_InitControlRead();
                    }
                    else if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_VOLUME_CONTROL)
                    {
                        /* `#START VOLUME_CONTROL_GET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_1_currentTD.wCount = USBFS_1_VOLUME_LEN;
                        USBFS_1_currentTD.pData  = USBFS_1_currentVolume;
                        requestHandled   = USBFS_1_InitControlRead();
                    }
                    else
                    {
                        /* `#START OTHER_GET_CUR_REQUESTS` Place other request handler here */

                        /* `#END` */
                    }
                    break;
                case USBFS_1_GET_MIN:    /* GET_MIN */
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_VOLUME_CONTROL)
                    {
                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_1_currentTD.wCount = USBFS_1_VOLUME_LEN;
                        USBFS_1_currentTD.pData  = &USBFS_1_minimumVolume[0];
                        requestHandled   = USBFS_1_InitControlRead();
                    }
                    break;
                case USBFS_1_GET_MAX:    /* GET_MAX */
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_VOLUME_CONTROL)
                    {
                        /* Entity ID Control Selector is VOLUME, */
                        USBFS_1_currentTD.wCount = USBFS_1_VOLUME_LEN;
                        USBFS_1_currentTD.pData  = &USBFS_1_maximumVolume[0];
                        requestHandled   = USBFS_1_InitControlRead();
                    }
                    break;
                case USBFS_1_GET_RES:    /* GET_RES */
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_VOLUME_CONTROL)
                    {
                         /* Entity ID Control Selector is VOLUME, */
                        USBFS_1_currentTD.wCount = USBFS_1_VOLUME_LEN;
                        USBFS_1_currentTD.pData  = &USBFS_1_resolutionVolume[0];
                        requestHandled   = USBFS_1_InitControlRead();
                    }
                    break;
                /* The contents of the status message is reserved for future use.
                *  For the time being, a null packet should be returned in the data stage of the
                *  control transfer, and the received null packet should be ACKed.
                */
                case USBFS_1_GET_STAT:
                        USBFS_1_currentTD.wCount = 0u;
                        requestHandled   = USBFS_1_InitControlWrite();

                #endif /* End USBFS_1_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_WRITE_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else
        {   /* USBFS_1_RQST_RCPT_OTHER */
        }
    }
    else if ((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_DIR_MASK) == \
                                                                                    USBFS_1_RQST_DIR_H2D)
    {
        /* Control Write */
        if((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK) == \
                                                                                    USBFS_1_RQST_RCPT_EP)
        {
            /* Endpoint */
            switch (CY_GET_REG8(USBFS_1_bRequest))
            {
                case USBFS_1_SET_CUR:
                #if defined(USBFS_1_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_SAMPLING_FREQ_CONTROL)
                    {
                         /* Endpoint Control Selector is Sampling Frequency */
                        USBFS_1_currentTD.wCount = USBFS_1_SAMPLE_FREQ_LEN;
                        USBFS_1_currentTD.pData  = USBFS_1_currentSampleFrequency[epNumber];
                        requestHandled   = USBFS_1_InitControlWrite();
                        USBFS_1_frequencyChanged = epNumber;
                    }
                #endif /* End USBFS_1_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_SAMPLING_FREQ_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else if((CY_GET_REG8(USBFS_1_bmRequestType) & USBFS_1_RQST_RCPT_MASK) == \
                                                                                    USBFS_1_RQST_RCPT_IFC)
        {
            /* Interface or Entity ID */
            switch (CY_GET_REG8(USBFS_1_bRequest))
            {
                case USBFS_1_SET_CUR:
                #if defined(USBFS_1_ENABLE_AUDIO_STREAMING)
                    if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_MUTE_CONTROL)
                    {
                        /* `#START MUTE_SET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        /* Entity ID Control Selector is MUTE */
                        USBFS_1_currentTD.wCount = 1u;
                        USBFS_1_currentTD.pData  = &USBFS_1_currentMute;
                        requestHandled   = USBFS_1_InitControlWrite();
                    }
                    else if(CY_GET_REG8(USBFS_1_wValueHi) == USBFS_1_VOLUME_CONTROL)
                    {
                        /* `#START VOLUME_CONTROL_SET_REQUEST` Place multi-channel handler here */

                        /* `#END` */

                        /* Entity ID Control Selector is VOLUME */
                        USBFS_1_currentTD.wCount = USBFS_1_VOLUME_LEN;
                        USBFS_1_currentTD.pData  = USBFS_1_currentVolume;
                        requestHandled   = USBFS_1_InitControlWrite();
                    }
                    else
                    {
                        /* `#START OTHER_SET_CUR_REQUESTS` Place other request handler here */

                        /* `#END` */
                    }
                #endif /* End USBFS_1_ENABLE_AUDIO_STREAMING */

                /* `#START AUDIO_CONTROL_SEL_REQUESTS` Place other request handler here */

                /* `#END` */
                    break;
                default:
                    break;
            }
        }
        else
        {   /* USBFS_1_RQST_RCPT_OTHER */
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

#endif  /* End USBFS_1_ENABLE_AUDIO_CLASS*/


/* [] END OF FILE */
