/*******************************************************************************
* File Name: USBFS_1_audio.h
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

#if !defined(CY_USBFS_USBFS_1_audio_H)
#define CY_USBFS_USBFS_1_audio_H

#include "cytypes.h"


/***************************************
* Custom Declarations
***************************************/

/* `#START CUSTOM_CONSTANTS` Place your declaration here */

/* `#END` */


/***************************************
*  Constants for USBFS_1_audio API.
***************************************/

/* Audio Class-Specific Request Codes (AUDIO Table A-9) */
#define USBFS_1_REQUEST_CODE_UNDEFINED     (0x00u)
#define USBFS_1_SET_CUR                    (0x01u)
#define USBFS_1_GET_CUR                    (0x81u)
#define USBFS_1_SET_MIN                    (0x02u)
#define USBFS_1_GET_MIN                    (0x82u)
#define USBFS_1_SET_MAX                    (0x03u)
#define USBFS_1_GET_MAX                    (0x83u)
#define USBFS_1_SET_RES                    (0x04u)
#define USBFS_1_GET_RES                    (0x84u)
#define USBFS_1_SET_MEM                    (0x05u)
#define USBFS_1_GET_MEM                    (0x85u)
#define USBFS_1_GET_STAT                   (0xFFu)

/* Endpoint Control Selectors (AUDIO Table A-19) */
#define USBFS_1_EP_CONTROL_UNDEFINED       (0x00u)
#define USBFS_1_SAMPLING_FREQ_CONTROL      (0x01u)
#define USBFS_1_PITCH_CONTROL              (0x02u)

/* Feature Unit Control Selectors (AUDIO Table A-11) */
#define USBFS_1_FU_CONTROL_UNDEFINED       (0x00u)
#define USBFS_1_MUTE_CONTROL               (0x01u)
#define USBFS_1_VOLUME_CONTROL             (0x02u)
#define USBFS_1_BASS_CONTROL               (0x03u)
#define USBFS_1_MID_CONTROL                (0x04u)
#define USBFS_1_TREBLE_CONTROL             (0x05u)
#define USBFS_1_GRAPHIC_EQUALIZER_CONTROL  (0x06u)
#define USBFS_1_AUTOMATIC_GAIN_CONTROL     (0x07u)
#define USBFS_1_DELAY_CONTROL              (0x08u)
#define USBFS_1_BASS_BOOST_CONTROL         (0x09u)
#define USBFS_1_LOUDNESS_CONTROL           (0x0Au)

#define USBFS_1_SAMPLE_FREQ_LEN            (3u)
#define USBFS_1_VOLUME_LEN                 (2u)

#if !defined(USER_SUPPLIED_DEFAULT_VOLUME_VALUE)
    #define USBFS_1_VOL_MIN_MSB            (0x80u)
    #define USBFS_1_VOL_MIN_LSB            (0x01u)
    #define USBFS_1_VOL_MAX_MSB            (0x7Fu)
    #define USBFS_1_VOL_MAX_LSB            (0xFFu)
    #define USBFS_1_VOL_RES_MSB            (0x00u)
    #define USBFS_1_VOL_RES_LSB            (0x01u)
#endif /* USER_SUPPLIED_DEFAULT_VOLUME_VALUE */


/***************************************
* External data references
***************************************/

extern volatile uint8 USBFS_1_currentSampleFrequency[USBFS_1_MAX_EP]
                                                             [USBFS_1_SAMPLE_FREQ_LEN];
extern volatile uint8 USBFS_1_frequencyChanged;
extern volatile uint8 USBFS_1_currentMute;
extern volatile uint8 USBFS_1_currentVolume[USBFS_1_VOLUME_LEN];
extern volatile uint8 USBFS_1_minimumVolume[USBFS_1_VOLUME_LEN];
extern volatile uint8 USBFS_1_maximumVolume[USBFS_1_VOLUME_LEN];
extern volatile uint8 USBFS_1_resolutionVolume[USBFS_1_VOLUME_LEN];

#endif /* End CY_USBFS_USBFS_1_audio_H */


/* [] END OF FILE */
