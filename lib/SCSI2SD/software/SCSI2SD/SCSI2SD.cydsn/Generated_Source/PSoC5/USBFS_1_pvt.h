/*******************************************************************************
* File Name: .h
* Version 2.60
*
* Description:
*  This private file provides constants and parameter values for the
*  USBFS Component.
*  Please do not use this file or its content in your project.
*
* Note:
*
********************************************************************************
* Copyright 2013, Cypress Semiconductor Corporation. All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_USBFS_USBFS_1_pvt_H)
#define CY_USBFS_USBFS_1_pvt_H


/***************************************
*     Private Variables
***************************************/

/* Generated external references for descriptors*/
extern const uint8 CYCODE USBFS_1_DEVICE0_DESCR[18u];
extern const uint8 CYCODE USBFS_1_DEVICE0_CONFIGURATION0_DESCR[25u];
extern const T_USBFS_1_EP_SETTINGS_BLOCK CYCODE USBFS_1_DEVICE0_CONFIGURATION0_EP_SETTINGS_TABLE[1u];
extern const uint8 CYCODE USBFS_1_DEVICE0_CONFIGURATION0_INTERFACE_CLASS[1u];
extern const T_USBFS_1_LUT CYCODE USBFS_1_DEVICE0_CONFIGURATION0_TABLE[4u];
extern const T_USBFS_1_LUT CYCODE USBFS_1_DEVICE0_TABLE[2u];
extern const T_USBFS_1_LUT CYCODE USBFS_1_TABLE[1u];


extern const uint8 CYCODE USBFS_1_MSOS_DESCRIPTOR[USBFS_1_MSOS_DESCRIPTOR_LENGTH];
extern const uint8 CYCODE USBFS_1_MSOS_CONFIGURATION_DESCR[USBFS_1_MSOS_CONF_DESCR_LENGTH];
#if defined(USBFS_1_ENABLE_IDSN_STRING)
    extern uint8 USBFS_1_idSerialNumberStringDescriptor[USBFS_1_IDSN_DESCR_LENGTH];
#endif /* USBFS_1_ENABLE_IDSN_STRING */

extern volatile uint8 USBFS_1_interfaceNumber;
extern volatile uint8 USBFS_1_interfaceSetting[USBFS_1_MAX_INTERFACES_NUMBER];
extern volatile uint8 USBFS_1_interfaceSetting_last[USBFS_1_MAX_INTERFACES_NUMBER];
extern volatile uint8 USBFS_1_deviceAddress;
extern volatile uint8 USBFS_1_interfaceStatus[USBFS_1_MAX_INTERFACES_NUMBER];
extern const uint8 CYCODE *USBFS_1_interfaceClass;

extern volatile T_USBFS_1_EP_CTL_BLOCK USBFS_1_EP[USBFS_1_MAX_EP];
extern volatile T_USBFS_1_TD USBFS_1_currentTD;

#if(USBFS_1_EP_MM != USBFS_1__EP_MANUAL)
    extern uint8 USBFS_1_DmaChan[USBFS_1_MAX_EP];
    extern uint8 USBFS_1_DmaTd[USBFS_1_MAX_EP];
#endif /* End USBFS_1_EP_MM */

extern volatile uint8 USBFS_1_ep0Toggle;
extern volatile uint8 USBFS_1_lastPacketSize;
extern volatile uint8 USBFS_1_ep0Mode;
extern volatile uint8 USBFS_1_ep0Count;
extern volatile uint16 USBFS_1_transferByteCount;


/***************************************
*     Private Function Prototypes
***************************************/
void  USBFS_1_ReInitComponent(void) ;
void  USBFS_1_HandleSetup(void) ;
void  USBFS_1_HandleIN(void) ;
void  USBFS_1_HandleOUT(void) ;
void  USBFS_1_LoadEP0(void) ;
uint8 USBFS_1_InitControlRead(void) ;
uint8 USBFS_1_InitControlWrite(void) ;
void  USBFS_1_ControlReadDataStage(void) ;
void  USBFS_1_ControlReadStatusStage(void) ;
void  USBFS_1_ControlReadPrematureStatus(void)
                                                ;
uint8 USBFS_1_InitControlWrite(void) ;
uint8 USBFS_1_InitZeroLengthControlTransfer(void)
                                                ;
void  USBFS_1_ControlWriteDataStage(void) ;
void  USBFS_1_ControlWriteStatusStage(void) ;
void  USBFS_1_ControlWritePrematureStatus(void)
                                                ;
uint8 USBFS_1_InitNoDataControlTransfer(void) ;
void  USBFS_1_NoDataControlStatusStage(void) ;
void  USBFS_1_InitializeStatusBlock(void) ;
void  USBFS_1_UpdateStatusBlock(uint8 completionCode) ;
uint8 USBFS_1_DispatchClassRqst(void) ;

void USBFS_1_Config(uint8 clearAltSetting) ;
void USBFS_1_ConfigAltChanged(void) ;
void USBFS_1_ConfigReg(void) ;

const T_USBFS_1_LUT CYCODE *USBFS_1_GetConfigTablePtr(uint8 c)
                                                            ;
const T_USBFS_1_LUT CYCODE *USBFS_1_GetDeviceTablePtr(void)
                                                            ;
const uint8 CYCODE *USBFS_1_GetInterfaceClassTablePtr(void)
                                                    ;
uint8 USBFS_1_ClearEndpointHalt(void) ;
uint8 USBFS_1_SetEndpointHalt(void) ;
uint8 USBFS_1_ValidateAlternateSetting(void) ;

void USBFS_1_SaveConfig(void) ;
void USBFS_1_RestoreConfig(void) ;

#if defined(USBFS_1_ENABLE_IDSN_STRING)
    void USBFS_1_ReadDieID(uint8 descr[]) ;
#endif /* USBFS_1_ENABLE_IDSN_STRING */

#if defined(USBFS_1_ENABLE_HID_CLASS)
    uint8 USBFS_1_DispatchHIDClassRqst(void);
#endif /* End USBFS_1_ENABLE_HID_CLASS */
#if defined(USBFS_1_ENABLE_AUDIO_CLASS)
    uint8 USBFS_1_DispatchAUDIOClassRqst(void);
#endif /* End USBFS_1_ENABLE_HID_CLASS */
#if defined(USBFS_1_ENABLE_CDC_CLASS)
    uint8 USBFS_1_DispatchCDCClassRqst(void);
#endif /* End USBFS_1_ENABLE_CDC_CLASS */

CY_ISR_PROTO(USBFS_1_EP_0_ISR);
#if(USBFS_1_EP1_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_1_ISR);
#endif /* End USBFS_1_EP1_ISR_REMOVE */
#if(USBFS_1_EP2_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_2_ISR);
#endif /* End USBFS_1_EP2_ISR_REMOVE */
#if(USBFS_1_EP3_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_3_ISR);
#endif /* End USBFS_1_EP3_ISR_REMOVE */
#if(USBFS_1_EP4_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_4_ISR);
#endif /* End USBFS_1_EP4_ISR_REMOVE */
#if(USBFS_1_EP5_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_5_ISR);
#endif /* End USBFS_1_EP5_ISR_REMOVE */
#if(USBFS_1_EP6_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_6_ISR);
#endif /* End USBFS_1_EP6_ISR_REMOVE */
#if(USBFS_1_EP7_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_7_ISR);
#endif /* End USBFS_1_EP7_ISR_REMOVE */
#if(USBFS_1_EP8_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_EP_8_ISR);
#endif /* End USBFS_1_EP8_ISR_REMOVE */
CY_ISR_PROTO(USBFS_1_BUS_RESET_ISR);
#if(USBFS_1_SOF_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_SOF_ISR);
#endif /* End USBFS_1_SOF_ISR_REMOVE */
#if(USBFS_1_EP_MM != USBFS_1__EP_MANUAL)
    CY_ISR_PROTO(USBFS_1_ARB_ISR);
#endif /* End USBFS_1_EP_MM */
#if(USBFS_1_DP_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_1_DP_ISR);
#endif /* End USBFS_1_DP_ISR_REMOVE */


/***************************************
* Request Handlers
***************************************/

uint8 USBFS_1_HandleStandardRqst(void) ;
uint8 USBFS_1_DispatchClassRqst(void) ;
uint8 USBFS_1_HandleVendorRqst(void) ;


/***************************************
*    HID Internal references
***************************************/
#if defined(USBFS_1_ENABLE_HID_CLASS)
    void USBFS_1_FindReport(void) ;
    void USBFS_1_FindReportDescriptor(void) ;
    void USBFS_1_FindHidClassDecriptor(void) ;
#endif /* USBFS_1_ENABLE_HID_CLASS */


/***************************************
*    MIDI Internal references
***************************************/
#if defined(USBFS_1_ENABLE_MIDI_STREAMING)
    void USBFS_1_MIDI_IN_EP_Service(void) ;
#endif /* USBFS_1_ENABLE_MIDI_STREAMING */


#endif /* CY_USBFS_USBFS_1_pvt_H */


/* [] END OF FILE */
