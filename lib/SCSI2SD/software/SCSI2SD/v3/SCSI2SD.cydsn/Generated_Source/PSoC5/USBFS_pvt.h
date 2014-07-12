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

#if !defined(CY_USBFS_USBFS_pvt_H)
#define CY_USBFS_USBFS_pvt_H


/***************************************
*     Private Variables
***************************************/

/* Generated external references for descriptors*/
extern const uint8 CYCODE USBFS_DEVICE0_DESCR[18u];
extern const uint8 CYCODE USBFS_DEVICE0_CONFIGURATION0_DESCR[73u];
extern const T_USBFS_LUT CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_TABLE[1u];
extern const T_USBFS_LUT CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_TABLE[1u];
extern const T_USBFS_EP_SETTINGS_BLOCK CYCODE USBFS_DEVICE0_CONFIGURATION0_EP_SETTINGS_TABLE[4u];
extern const uint8 CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE_CLASS[2u];
extern const T_USBFS_LUT CYCODE USBFS_DEVICE0_CONFIGURATION0_TABLE[5u];
extern const T_USBFS_LUT CYCODE USBFS_DEVICE0_TABLE[2u];
extern const T_USBFS_LUT CYCODE USBFS_TABLE[1u];
extern const uint8 CYCODE USBFS_SN_STRING_DESCRIPTOR[10];
extern const uint8 CYCODE USBFS_STRING_DESCRIPTORS[45u];
extern T_USBFS_XFER_STATUS_BLOCK USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_IN_RPT_SCB;
extern uint8 USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_IN_BUF[
            USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_IN_BUF_SIZE];
extern T_USBFS_XFER_STATUS_BLOCK USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_OUT_RPT_SCB;
extern uint8 USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_OUT_BUF[
            USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_OUT_BUF_SIZE];
extern T_USBFS_XFER_STATUS_BLOCK USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_IN_RPT_SCB;
extern uint8 USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_IN_BUF[
            USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_IN_BUF_SIZE];
extern T_USBFS_XFER_STATUS_BLOCK USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_OUT_RPT_SCB;
extern uint8 USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_OUT_BUF[
            USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_OUT_BUF_SIZE];
extern const uint8 CYCODE USBFS_HIDREPORT_DESCRIPTOR1[40u];
extern const T_USBFS_TD CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_IN_RPT_TABLE[1u];
extern const T_USBFS_TD CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_OUT_RPT_TABLE[1u];
extern const T_USBFS_LUT CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_TABLE[5u];
extern const T_USBFS_TD CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_IN_RPT_TABLE[1u];
extern const T_USBFS_TD CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_OUT_RPT_TABLE[1u];
extern const T_USBFS_LUT CYCODE USBFS_DEVICE0_CONFIGURATION0_INTERFACE1_ALTERNATE0_HID_TABLE[5u];


extern const uint8 CYCODE USBFS_MSOS_DESCRIPTOR[USBFS_MSOS_DESCRIPTOR_LENGTH];
extern const uint8 CYCODE USBFS_MSOS_CONFIGURATION_DESCR[USBFS_MSOS_CONF_DESCR_LENGTH];
#if defined(USBFS_ENABLE_IDSN_STRING)
    extern uint8 USBFS_idSerialNumberStringDescriptor[USBFS_IDSN_DESCR_LENGTH];
#endif /* USBFS_ENABLE_IDSN_STRING */

extern volatile uint8 USBFS_interfaceNumber;
extern volatile uint8 USBFS_interfaceSetting[USBFS_MAX_INTERFACES_NUMBER];
extern volatile uint8 USBFS_interfaceSetting_last[USBFS_MAX_INTERFACES_NUMBER];
extern volatile uint8 USBFS_deviceAddress;
extern volatile uint8 USBFS_interfaceStatus[USBFS_MAX_INTERFACES_NUMBER];
extern const uint8 CYCODE *USBFS_interfaceClass;

extern volatile T_USBFS_EP_CTL_BLOCK USBFS_EP[USBFS_MAX_EP];
extern volatile T_USBFS_TD USBFS_currentTD;

#if(USBFS_EP_MM != USBFS__EP_MANUAL)
    extern uint8 USBFS_DmaChan[USBFS_MAX_EP];
    extern uint8 USBFS_DmaTd[USBFS_MAX_EP];
#endif /* End USBFS_EP_MM */

extern volatile uint8 USBFS_ep0Toggle;
extern volatile uint8 USBFS_lastPacketSize;
extern volatile uint8 USBFS_ep0Mode;
extern volatile uint8 USBFS_ep0Count;
extern volatile uint16 USBFS_transferByteCount;


/***************************************
*     Private Function Prototypes
***************************************/
void  USBFS_ReInitComponent(void) ;
void  USBFS_HandleSetup(void) ;
void  USBFS_HandleIN(void) ;
void  USBFS_HandleOUT(void) ;
void  USBFS_LoadEP0(void) ;
uint8 USBFS_InitControlRead(void) ;
uint8 USBFS_InitControlWrite(void) ;
void  USBFS_ControlReadDataStage(void) ;
void  USBFS_ControlReadStatusStage(void) ;
void  USBFS_ControlReadPrematureStatus(void)
                                                ;
uint8 USBFS_InitControlWrite(void) ;
uint8 USBFS_InitZeroLengthControlTransfer(void)
                                                ;
void  USBFS_ControlWriteDataStage(void) ;
void  USBFS_ControlWriteStatusStage(void) ;
void  USBFS_ControlWritePrematureStatus(void)
                                                ;
uint8 USBFS_InitNoDataControlTransfer(void) ;
void  USBFS_NoDataControlStatusStage(void) ;
void  USBFS_InitializeStatusBlock(void) ;
void  USBFS_UpdateStatusBlock(uint8 completionCode) ;
uint8 USBFS_DispatchClassRqst(void) ;

void USBFS_Config(uint8 clearAltSetting) ;
void USBFS_ConfigAltChanged(void) ;
void USBFS_ConfigReg(void) ;

const T_USBFS_LUT CYCODE *USBFS_GetConfigTablePtr(uint8 c)
                                                            ;
const T_USBFS_LUT CYCODE *USBFS_GetDeviceTablePtr(void)
                                                            ;
const uint8 CYCODE *USBFS_GetInterfaceClassTablePtr(void)
                                                    ;
uint8 USBFS_ClearEndpointHalt(void) ;
uint8 USBFS_SetEndpointHalt(void) ;
uint8 USBFS_ValidateAlternateSetting(void) ;

void USBFS_SaveConfig(void) ;
void USBFS_RestoreConfig(void) ;

#if defined(USBFS_ENABLE_IDSN_STRING)
    void USBFS_ReadDieID(uint8 descr[]) ;
#endif /* USBFS_ENABLE_IDSN_STRING */

#if defined(USBFS_ENABLE_HID_CLASS)
    uint8 USBFS_DispatchHIDClassRqst(void);
#endif /* End USBFS_ENABLE_HID_CLASS */
#if defined(USBFS_ENABLE_AUDIO_CLASS)
    uint8 USBFS_DispatchAUDIOClassRqst(void);
#endif /* End USBFS_ENABLE_HID_CLASS */
#if defined(USBFS_ENABLE_CDC_CLASS)
    uint8 USBFS_DispatchCDCClassRqst(void);
#endif /* End USBFS_ENABLE_CDC_CLASS */

CY_ISR_PROTO(USBFS_EP_0_ISR);
#if(USBFS_EP1_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_1_ISR);
#endif /* End USBFS_EP1_ISR_REMOVE */
#if(USBFS_EP2_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_2_ISR);
#endif /* End USBFS_EP2_ISR_REMOVE */
#if(USBFS_EP3_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_3_ISR);
#endif /* End USBFS_EP3_ISR_REMOVE */
#if(USBFS_EP4_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_4_ISR);
#endif /* End USBFS_EP4_ISR_REMOVE */
#if(USBFS_EP5_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_5_ISR);
#endif /* End USBFS_EP5_ISR_REMOVE */
#if(USBFS_EP6_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_6_ISR);
#endif /* End USBFS_EP6_ISR_REMOVE */
#if(USBFS_EP7_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_7_ISR);
#endif /* End USBFS_EP7_ISR_REMOVE */
#if(USBFS_EP8_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_EP_8_ISR);
#endif /* End USBFS_EP8_ISR_REMOVE */
CY_ISR_PROTO(USBFS_BUS_RESET_ISR);
#if(USBFS_SOF_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_SOF_ISR);
#endif /* End USBFS_SOF_ISR_REMOVE */
#if(USBFS_EP_MM != USBFS__EP_MANUAL)
    CY_ISR_PROTO(USBFS_ARB_ISR);
#endif /* End USBFS_EP_MM */
#if(USBFS_DP_ISR_REMOVE == 0u)
    CY_ISR_PROTO(USBFS_DP_ISR);
#endif /* End USBFS_DP_ISR_REMOVE */


/***************************************
* Request Handlers
***************************************/

uint8 USBFS_HandleStandardRqst(void) ;
uint8 USBFS_DispatchClassRqst(void) ;
uint8 USBFS_HandleVendorRqst(void) ;


/***************************************
*    HID Internal references
***************************************/
#if defined(USBFS_ENABLE_HID_CLASS)
    void USBFS_FindReport(void) ;
    void USBFS_FindReportDescriptor(void) ;
    void USBFS_FindHidClassDecriptor(void) ;
#endif /* USBFS_ENABLE_HID_CLASS */


/***************************************
*    MIDI Internal references
***************************************/
#if defined(USBFS_ENABLE_MIDI_STREAMING)
    void USBFS_MIDI_IN_EP_Service(void) ;
#endif /* USBFS_ENABLE_MIDI_STREAMING */


#endif /* CY_USBFS_USBFS_pvt_H */


/* [] END OF FILE */
