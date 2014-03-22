/*******************************************************************************
* File Name: USBFS_audio.h
* Version 2.60
*
* Description:
*  Header File for the USFS component. Contains prototypes and constant values.
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_USBFS_USBFS_audio_H)
#define CY_USBFS_USBFS_audio_H

#include "cytypes.h"


/***************************************
* Custom Declarations
***************************************/

/* `#START CUSTOM_CONSTANTS` Place your declaration here */

/* `#END` */


/***************************************
*  Constants for USBFS_audio API.
***************************************/

/* Audio Class-Specific Request Codes (AUDIO Table A-9) */
#define USBFS_REQUEST_CODE_UNDEFINED     (0x00u)
#define USBFS_SET_CUR                    (0x01u)
#define USBFS_GET_CUR                    (0x81u)
#define USBFS_SET_MIN                    (0x02u)
#define USBFS_GET_MIN                    (0x82u)
#define USBFS_SET_MAX                    (0x03u)
#define USBFS_GET_MAX                    (0x83u)
#define USBFS_SET_RES                    (0x04u)
#define USBFS_GET_RES                    (0x84u)
#define USBFS_SET_MEM                    (0x05u)
#define USBFS_GET_MEM                    (0x85u)
#define USBFS_GET_STAT                   (0xFFu)

/* Endpoint Control Selectors (AUDIO Table A-19) */
#define USBFS_EP_CONTROL_UNDEFINED       (0x00u)
#define USBFS_SAMPLING_FREQ_CONTROL      (0x01u)
#define USBFS_PITCH_CONTROL              (0x02u)

/* Feature Unit Control Selectors (AUDIO Table A-11) */
#define USBFS_FU_CONTROL_UNDEFINED       (0x00u)
#define USBFS_MUTE_CONTROL               (0x01u)
#define USBFS_VOLUME_CONTROL             (0x02u)
#define USBFS_BASS_CONTROL               (0x03u)
#define USBFS_MID_CONTROL                (0x04u)
#define USBFS_TREBLE_CONTROL             (0x05u)
#define USBFS_GRAPHIC_EQUALIZER_CONTROL  (0x06u)
#define USBFS_AUTOMATIC_GAIN_CONTROL     (0x07u)
#define USBFS_DELAY_CONTROL              (0x08u)
#define USBFS_BASS_BOOST_CONTROL         (0x09u)
#define USBFS_LOUDNESS_CONTROL           (0x0Au)

#define USBFS_SAMPLE_FREQ_LEN            (3u)
#define USBFS_VOLUME_LEN                 (2u)

#if !defined(USER_SUPPLIED_DEFAULT_VOLUME_VALUE)
    #define USBFS_VOL_MIN_MSB            (0x80u)
    #define USBFS_VOL_MIN_LSB            (0x01u)
    #define USBFS_VOL_MAX_MSB            (0x7Fu)
    #define USBFS_VOL_MAX_LSB            (0xFFu)
    #define USBFS_VOL_RES_MSB            (0x00u)
    #define USBFS_VOL_RES_LSB            (0x01u)
#endif /* USER_SUPPLIED_DEFAULT_VOLUME_VALUE */


/***************************************
* External data references
***************************************/

extern volatile uint8 USBFS_currentSampleFrequency[USBFS_MAX_EP]
                                                             [USBFS_SAMPLE_FREQ_LEN];
extern volatile uint8 USBFS_frequencyChanged;
extern volatile uint8 USBFS_currentMute;
extern volatile uint8 USBFS_currentVolume[USBFS_VOLUME_LEN];
extern volatile uint8 USBFS_minimumVolume[USBFS_VOLUME_LEN];
extern volatile uint8 USBFS_maximumVolume[USBFS_VOLUME_LEN];
extern volatile uint8 USBFS_resolutionVolume[USBFS_VOLUME_LEN];

#endif /* End CY_USBFS_USBFS_audio_H */


/* [] END OF FILE */
