/*******************************************************************************
* File Name: USBFS.h
* Version 2.80
*
* Description:
*  Header File for the USBFS component. Contains prototypes and constant values.
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_USBFS_USBFS_H)
#define CY_USBFS_USBFS_H

#include "cytypes.h"
#include "cydevice_trm.h"
#include "cyfitter.h"
#include "CyLib.h"

/*  User supplied definitions. */
/* `#START USER_DEFINITIONS` Place your declaration here */

/* `#END` */


/***************************************
* Conditional Compilation Parameters
***************************************/

/* Check to see if required defines such as CY_PSOC5LP are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5LP)
    #error Component USBFS_v2_80 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5LP) */


/***************************************
*  Memory Type Definitions
***************************************/

/* Renamed Type Definitions for backward compatibility.
*  Should not be used in new designs.
*/
#define USBFS_CODE CYCODE
#define USBFS_FAR CYFAR
#if defined(__C51__) || defined(__CX51__)
    #define USBFS_DATA data
    #define USBFS_XDATA xdata
#else
    #define USBFS_DATA
    #define USBFS_XDATA
#endif /*  __C51__ */
#define USBFS_NULL       NULL


/***************************************
* Enumerated Types and Parameters
***************************************/

#define USBFS__EP_MANUAL 0
#define USBFS__EP_DMAMANUAL 1
#define USBFS__EP_DMAAUTO 2

#define USBFS__MA_STATIC 0
#define USBFS__MA_DYNAMIC 1



/***************************************
*    Initial Parameter Constants
***************************************/

#define USBFS_NUM_DEVICES   (1u)
#define USBFS_ENABLE_DESCRIPTOR_STRINGS   
#define USBFS_ENABLE_SN_STRING   
#define USBFS_ENABLE_STRINGS   
#define USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_IN_BUF_SIZE   (65u)
#define USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_NUM_IN_RPTS   (1u)
#define USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_OUT_BUF_SIZE   (65u)
#define USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_NUM_OUT_RPTS   (1u)
#define USBFS_DEVICE0_CONFIGURATION0_INTERFACE0_ALTERNATE0_HID_COUNT   (1u)
#define USBFS_ENABLE_HID_CLASS   
#define USBFS_HID_RPT_1_SIZE_LSB   (0x24u)
#define USBFS_HID_RPT_1_SIZE_MSB   (0x00u)
#define USBFS_MAX_REPORTID_NUMBER   (0u)

#define USBFS_MON_VBUS                       (0u)
#define USBFS_EXTERN_VBUS                    (0u)
#define USBFS_EXTERN_VND                     (0u)
#define USBFS_EXTERN_CLS                     (0u)
#define USBFS_MAX_INTERFACES_NUMBER          (1u)
#define USBFS_EP0_ISR_REMOVE                 (0u)
#define USBFS_EP1_ISR_REMOVE                 (0u)
#define USBFS_EP2_ISR_REMOVE                 (0u)
#define USBFS_EP3_ISR_REMOVE                 (1u)
#define USBFS_EP4_ISR_REMOVE                 (1u)
#define USBFS_EP5_ISR_REMOVE                 (1u)
#define USBFS_EP6_ISR_REMOVE                 (1u)
#define USBFS_EP7_ISR_REMOVE                 (1u)
#define USBFS_EP8_ISR_REMOVE                 (1u)
#define USBFS_EP_MM                          (0u)
#define USBFS_EP_MA                          (0u)
#define USBFS_EP_DMA_AUTO_OPT                (0u)
#define USBFS_DMA1_REMOVE                    (1u)
#define USBFS_DMA2_REMOVE                    (1u)
#define USBFS_DMA3_REMOVE                    (1u)
#define USBFS_DMA4_REMOVE                    (1u)
#define USBFS_DMA5_REMOVE                    (1u)
#define USBFS_DMA6_REMOVE                    (1u)
#define USBFS_DMA7_REMOVE                    (1u)
#define USBFS_DMA8_REMOVE                    (1u)
#define USBFS_SOF_ISR_REMOVE                 (0u)
#define USBFS_ARB_ISR_REMOVE                 (0u)
#define USBFS_DP_ISR_REMOVE                  (0u)
#define USBFS_ENABLE_CDC_CLASS_API           (1u)
#define USBFS_ENABLE_MIDI_API                (1u)
#define USBFS_MIDI_EXT_MODE                  (0u)


/***************************************
*    Data Struct Definition
***************************************/

typedef struct
{
    uint8  attrib;
    uint8  apiEpState;
    uint8  hwEpState;
    uint8  epToggle;
    uint8  addr;
    uint8  epMode;
    uint16 buffOffset;
    uint16 bufferSize;
    uint8  interface;
} T_USBFS_EP_CTL_BLOCK;

typedef struct
{
    uint8  interface;
    uint8  altSetting;
    uint8  addr;
    uint8  attributes;
    uint16 bufferSize;
    uint8  bMisc;
} T_USBFS_EP_SETTINGS_BLOCK;

typedef struct
{
    uint8  status;
    uint16 length;
} T_USBFS_XFER_STATUS_BLOCK;

typedef struct
{
    uint16  count;
    volatile uint8 *pData;
    T_USBFS_XFER_STATUS_BLOCK *pStatusBlock;
} T_USBFS_TD;


typedef struct
{
    uint8   c;
    const void *p_list;
} T_USBFS_LUT;

/* Resume/Suspend API Support */
typedef struct
{
    uint8 enableState;
    uint8 mode;
} USBFS_BACKUP_STRUCT;


/* Renamed structure fields for backward compatibility.
*  Should not be used in new designs.
*/
#define wBuffOffset         buffOffset
#define wBufferSize         bufferSize
#define bStatus             status
#define wLength             length
#define wCount              count

/* Renamed global variable for backward compatibility.
*  Should not be used in new designs.
*/
#define CurrentTD           USBFS_currentTD


/***************************************
*       Function Prototypes
***************************************/

void   USBFS_Start(uint8 device, uint8 mode) ;
void   USBFS_Init(void) ;
void   USBFS_InitComponent(uint8 device, uint8 mode) ;
void   USBFS_Stop(void) ;
uint8  USBFS_CheckActivity(void) ;
uint8  USBFS_GetConfiguration(void) ;
uint8  USBFS_IsConfigurationChanged(void) ;
uint8  USBFS_GetInterfaceSetting(uint8 interfaceNumber)
                                                        ;
uint8  USBFS_GetEPState(uint8 epNumber) ;
uint16 USBFS_GetEPCount(uint8 epNumber) ;
void   USBFS_LoadInEP(uint8 epNumber, const uint8 pData[], uint16 length)
                                                                    ;
uint16 USBFS_ReadOutEP(uint8 epNumber, uint8 pData[], uint16 length)
                                                                    ;
void   USBFS_EnableOutEP(uint8 epNumber) ;
void   USBFS_DisableOutEP(uint8 epNumber) ;
void   USBFS_Force(uint8 bState) ;
uint8  USBFS_GetEPAckState(uint8 epNumber) ;
void   USBFS_SetPowerStatus(uint8 powerStatus) ;
uint8  USBFS_RWUEnabled(void) ;
void   USBFS_TerminateEP(uint8 ep) ;

void   USBFS_Suspend(void) ;
void   USBFS_Resume(void) ;

#if defined(USBFS_ENABLE_FWSN_STRING)
    void   USBFS_SerialNumString(uint8 snString[]) ;
#endif  /* USBFS_ENABLE_FWSN_STRING */
#if (USBFS_MON_VBUS == 1u)
    uint8  USBFS_VBusPresent(void) ;
#endif /*  USBFS_MON_VBUS */

#if defined(CYDEV_BOOTLOADER_IO_COMP) && ((CYDEV_BOOTLOADER_IO_COMP == CyBtldr_USBFS) || \
                                          (CYDEV_BOOTLOADER_IO_COMP == CyBtldr_Custom_Interface))

    void USBFS_CyBtldrCommStart(void) ;
    void USBFS_CyBtldrCommStop(void) ;
    void USBFS_CyBtldrCommReset(void) ;
    cystatus USBFS_CyBtldrCommWrite(const uint8 pData[], uint16 size, uint16 *count, uint8 timeOut) CYSMALL
                                                        ;
    cystatus USBFS_CyBtldrCommRead       (uint8 pData[], uint16 size, uint16 *count, uint8 timeOut) CYSMALL
                                                        ;

    #define USBFS_BTLDR_OUT_EP      (0x01u)
    #define USBFS_BTLDR_IN_EP       (0x02u)

    #define USBFS_BTLDR_SIZEOF_WRITE_BUFFER  (64u)   /* EP 1 OUT */
    #define USBFS_BTLDR_SIZEOF_READ_BUFFER   (64u)   /* EP 2 IN  */
    #define USBFS_BTLDR_MAX_PACKET_SIZE      USBFS_BTLDR_SIZEOF_WRITE_BUFFER

    #define USBFS_BTLDR_WAIT_1_MS            (1u)    /* Time Out quantity equal 1mS */

    /* These defines active if used USBFS interface as an
    *  IO Component for bootloading. When Custom_Interface selected
    *  in Bootloder configuration as the IO Component, user must
    *  provide these functions.
    */
    #if (CYDEV_BOOTLOADER_IO_COMP == CyBtldr_USBFS)
        #define CyBtldrCommStart        USBFS_CyBtldrCommStart
        #define CyBtldrCommStop         USBFS_CyBtldrCommStop
        #define CyBtldrCommReset        USBFS_CyBtldrCommReset
        #define CyBtldrCommWrite        USBFS_CyBtldrCommWrite
        #define CyBtldrCommRead         USBFS_CyBtldrCommRead
    #endif  /*End   CYDEV_BOOTLOADER_IO_COMP == CyBtldr_USBFS */

#endif /*  CYDEV_BOOTLOADER_IO_COMP  */

#if(USBFS_EP_MM != USBFS__EP_MANUAL)
    void USBFS_InitEP_DMA(uint8 epNumber, const uint8* pData)
                                                    ;
    void USBFS_Stop_DMA(uint8 epNumber) ;
#endif /*  USBFS_EP_MM != USBFS__EP_MANUAL) */

#if defined(USBFS_ENABLE_MIDI_STREAMING) && (USBFS_ENABLE_MIDI_API != 0u)
    void USBFS_MIDI_EP_Init(void) ;

    #if (USBFS_MIDI_IN_BUFF_SIZE > 0)
        void USBFS_MIDI_IN_Service(void) ;
        uint8 USBFS_PutUsbMidiIn(uint8 ic, const uint8 midiMsg[], uint8 cable)
                                                                ;
    #endif /* USBFS_MIDI_IN_BUFF_SIZE > 0 */

    #if (USBFS_MIDI_OUT_BUFF_SIZE > 0)
        void USBFS_MIDI_OUT_EP_Service(void) ;
    #endif /* USBFS_MIDI_OUT_BUFF_SIZE > 0 */

#endif /*  USBFS_ENABLE_MIDI_API != 0u */

/* Renamed Functions for backward compatibility.
*  Should not be used in new designs.
*/

#define USBFS_bCheckActivity             USBFS_CheckActivity
#define USBFS_bGetConfiguration          USBFS_GetConfiguration
#define USBFS_bGetInterfaceSetting       USBFS_GetInterfaceSetting
#define USBFS_bGetEPState                USBFS_GetEPState
#define USBFS_wGetEPCount                USBFS_GetEPCount
#define USBFS_bGetEPAckState             USBFS_GetEPAckState
#define USBFS_bRWUEnabled                USBFS_RWUEnabled
#define USBFS_bVBusPresent               USBFS_VBusPresent

#define USBFS_bConfiguration             USBFS_configuration
#define USBFS_bInterfaceSetting          USBFS_interfaceSetting
#define USBFS_bDeviceAddress             USBFS_deviceAddress
#define USBFS_bDeviceStatus              USBFS_deviceStatus
#define USBFS_bDevice                    USBFS_device
#define USBFS_bTransferState             USBFS_transferState
#define USBFS_bLastPacketSize            USBFS_lastPacketSize

#define USBFS_LoadEP                     USBFS_LoadInEP
#define USBFS_LoadInISOCEP               USBFS_LoadInEP
#define USBFS_EnableOutISOCEP            USBFS_EnableOutEP

#define USBFS_SetVector                  CyIntSetVector
#define USBFS_SetPriority                CyIntSetPriority
#define USBFS_EnableInt                  CyIntEnable


/***************************************
*          API Constants
***************************************/

#define USBFS_EP0                        (0u)
#define USBFS_EP1                        (1u)
#define USBFS_EP2                        (2u)
#define USBFS_EP3                        (3u)
#define USBFS_EP4                        (4u)
#define USBFS_EP5                        (5u)
#define USBFS_EP6                        (6u)
#define USBFS_EP7                        (7u)
#define USBFS_EP8                        (8u)
#define USBFS_MAX_EP                     (9u)

#define USBFS_TRUE                       (1u)
#define USBFS_FALSE                      (0u)

#define USBFS_NO_EVENT_ALLOWED           (2u)
#define USBFS_EVENT_PENDING              (1u)
#define USBFS_NO_EVENT_PENDING           (0u)

#define USBFS_IN_BUFFER_FULL             USBFS_NO_EVENT_PENDING
#define USBFS_IN_BUFFER_EMPTY            USBFS_EVENT_PENDING
#define USBFS_OUT_BUFFER_FULL            USBFS_EVENT_PENDING
#define USBFS_OUT_BUFFER_EMPTY           USBFS_NO_EVENT_PENDING

#define USBFS_FORCE_J                    (0xA0u)
#define USBFS_FORCE_K                    (0x80u)
#define USBFS_FORCE_SE0                  (0xC0u)
#define USBFS_FORCE_NONE                 (0x00u)

#define USBFS_IDLE_TIMER_RUNNING         (0x02u)
#define USBFS_IDLE_TIMER_EXPIRED         (0x01u)
#define USBFS_IDLE_TIMER_INDEFINITE      (0x00u)

#define USBFS_DEVICE_STATUS_BUS_POWERED  (0x00u)
#define USBFS_DEVICE_STATUS_SELF_POWERED (0x01u)

#define USBFS_3V_OPERATION               (0x00u)
#define USBFS_5V_OPERATION               (0x01u)
#define USBFS_DWR_VDDD_OPERATION         (0x02u)

#define USBFS_MODE_DISABLE               (0x00u)
#define USBFS_MODE_NAK_IN_OUT            (0x01u)
#define USBFS_MODE_STATUS_OUT_ONLY       (0x02u)
#define USBFS_MODE_STALL_IN_OUT          (0x03u)
#define USBFS_MODE_RESERVED_0100         (0x04u)
#define USBFS_MODE_ISO_OUT               (0x05u)
#define USBFS_MODE_STATUS_IN_ONLY        (0x06u)
#define USBFS_MODE_ISO_IN                (0x07u)
#define USBFS_MODE_NAK_OUT               (0x08u)
#define USBFS_MODE_ACK_OUT               (0x09u)
#define USBFS_MODE_RESERVED_1010         (0x0Au)
#define USBFS_MODE_ACK_OUT_STATUS_IN     (0x0Bu)
#define USBFS_MODE_NAK_IN                (0x0Cu)
#define USBFS_MODE_ACK_IN                (0x0Du)
#define USBFS_MODE_RESERVED_1110         (0x0Eu)
#define USBFS_MODE_ACK_IN_STATUS_OUT     (0x0Fu)
#define USBFS_MODE_MASK                  (0x0Fu)
#define USBFS_MODE_STALL_DATA_EP         (0x80u)

#define USBFS_MODE_ACKD                  (0x10u)
#define USBFS_MODE_OUT_RCVD              (0x20u)
#define USBFS_MODE_IN_RCVD               (0x40u)
#define USBFS_MODE_SETUP_RCVD            (0x80u)

#define USBFS_RQST_TYPE_MASK             (0x60u)
#define USBFS_RQST_TYPE_STD              (0x00u)
#define USBFS_RQST_TYPE_CLS              (0x20u)
#define USBFS_RQST_TYPE_VND              (0x40u)
#define USBFS_RQST_DIR_MASK              (0x80u)
#define USBFS_RQST_DIR_D2H               (0x80u)
#define USBFS_RQST_DIR_H2D               (0x00u)
#define USBFS_RQST_RCPT_MASK             (0x03u)
#define USBFS_RQST_RCPT_DEV              (0x00u)
#define USBFS_RQST_RCPT_IFC              (0x01u)
#define USBFS_RQST_RCPT_EP               (0x02u)
#define USBFS_RQST_RCPT_OTHER            (0x03u)

/* USB Class Codes */
#define USBFS_CLASS_DEVICE               (0x00u)     /* Use class code info from Interface Descriptors */
#define USBFS_CLASS_AUDIO                (0x01u)     /* Audio device */
#define USBFS_CLASS_CDC                  (0x02u)     /* Communication device class */
#define USBFS_CLASS_HID                  (0x03u)     /* Human Interface Device */
#define USBFS_CLASS_PDC                  (0x05u)     /* Physical device class */
#define USBFS_CLASS_IMAGE                (0x06u)     /* Still Imaging device */
#define USBFS_CLASS_PRINTER              (0x07u)     /* Printer device  */
#define USBFS_CLASS_MSD                  (0x08u)     /* Mass Storage device  */
#define USBFS_CLASS_HUB                  (0x09u)     /* Full/Hi speed Hub */
#define USBFS_CLASS_CDC_DATA             (0x0Au)     /* CDC data device */
#define USBFS_CLASS_SMART_CARD           (0x0Bu)     /* Smart Card device */
#define USBFS_CLASS_CSD                  (0x0Du)     /* Content Security device */
#define USBFS_CLASS_VIDEO                (0x0Eu)     /* Video device */
#define USBFS_CLASS_PHD                  (0x0Fu)     /* Personal Healthcare device */
#define USBFS_CLASS_WIRELESSD            (0xDCu)     /* Wireless Controller */
#define USBFS_CLASS_MIS                  (0xE0u)     /* Miscellaneous */
#define USBFS_CLASS_APP                  (0xEFu)     /* Application Specific */
#define USBFS_CLASS_VENDOR               (0xFFu)     /* Vendor specific */


/* Standard Request Types (Table 9-4) */
#define USBFS_GET_STATUS                 (0x00u)
#define USBFS_CLEAR_FEATURE              (0x01u)
#define USBFS_SET_FEATURE                (0x03u)
#define USBFS_SET_ADDRESS                (0x05u)
#define USBFS_GET_DESCRIPTOR             (0x06u)
#define USBFS_SET_DESCRIPTOR             (0x07u)
#define USBFS_GET_CONFIGURATION          (0x08u)
#define USBFS_SET_CONFIGURATION          (0x09u)
#define USBFS_GET_INTERFACE              (0x0Au)
#define USBFS_SET_INTERFACE              (0x0Bu)
#define USBFS_SYNCH_FRAME                (0x0Cu)

/* Vendor Specific Request Types */
/* Request for Microsoft OS String Descriptor */
#define USBFS_GET_EXTENDED_CONFIG_DESCRIPTOR (0x01u)

/* Descriptor Types (Table 9-5) */
#define USBFS_DESCR_DEVICE                   (1u)
#define USBFS_DESCR_CONFIG                   (2u)
#define USBFS_DESCR_STRING                   (3u)
#define USBFS_DESCR_INTERFACE                (4u)
#define USBFS_DESCR_ENDPOINT                 (5u)
#define USBFS_DESCR_DEVICE_QUALIFIER         (6u)
#define USBFS_DESCR_OTHER_SPEED              (7u)
#define USBFS_DESCR_INTERFACE_POWER          (8u)

/* Device Descriptor Defines */
#define USBFS_DEVICE_DESCR_LENGTH            (18u)
#define USBFS_DEVICE_DESCR_SN_SHIFT          (16u)

/* Config Descriptor Shifts and Masks */
#define USBFS_CONFIG_DESCR_LENGTH                (0u)
#define USBFS_CONFIG_DESCR_TYPE                  (1u)
#define USBFS_CONFIG_DESCR_TOTAL_LENGTH_LOW      (2u)
#define USBFS_CONFIG_DESCR_TOTAL_LENGTH_HI       (3u)
#define USBFS_CONFIG_DESCR_NUM_INTERFACES        (4u)
#define USBFS_CONFIG_DESCR_CONFIG_VALUE          (5u)
#define USBFS_CONFIG_DESCR_CONFIGURATION         (6u)
#define USBFS_CONFIG_DESCR_ATTRIB                (7u)
#define USBFS_CONFIG_DESCR_ATTRIB_SELF_POWERED   (0x40u)
#define USBFS_CONFIG_DESCR_ATTRIB_RWU_EN         (0x20u)

/* Feature Selectors (Table 9-6) */
#define USBFS_DEVICE_REMOTE_WAKEUP           (0x01u)
#define USBFS_ENDPOINT_HALT                  (0x00u)
#define USBFS_TEST_MODE                      (0x02u)

/* USB Device Status (Figure 9-4) */
#define USBFS_DEVICE_STATUS_BUS_POWERED      (0x00u)
#define USBFS_DEVICE_STATUS_SELF_POWERED     (0x01u)
#define USBFS_DEVICE_STATUS_REMOTE_WAKEUP    (0x02u)

/* USB Endpoint Status (Figure 9-4) */
#define USBFS_ENDPOINT_STATUS_HALT           (0x01u)

/* USB Endpoint Directions */
#define USBFS_DIR_IN                         (0x80u)
#define USBFS_DIR_OUT                        (0x00u)
#define USBFS_DIR_UNUSED                     (0x7Fu)

/* USB Endpoint Attributes */
#define USBFS_EP_TYPE_CTRL                   (0x00u)
#define USBFS_EP_TYPE_ISOC                   (0x01u)
#define USBFS_EP_TYPE_BULK                   (0x02u)
#define USBFS_EP_TYPE_INT                    (0x03u)
#define USBFS_EP_TYPE_MASK                   (0x03u)

#define USBFS_EP_SYNC_TYPE_NO_SYNC           (0x00u)
#define USBFS_EP_SYNC_TYPE_ASYNC             (0x04u)
#define USBFS_EP_SYNC_TYPE_ADAPTIVE          (0x08u)
#define USBFS_EP_SYNC_TYPE_SYNCHRONOUS       (0x0Cu)
#define USBFS_EP_SYNC_TYPE_MASK              (0x0Cu)

#define USBFS_EP_USAGE_TYPE_DATA             (0x00u)
#define USBFS_EP_USAGE_TYPE_FEEDBACK         (0x10u)
#define USBFS_EP_USAGE_TYPE_IMPLICIT         (0x20u)
#define USBFS_EP_USAGE_TYPE_RESERVED         (0x30u)
#define USBFS_EP_USAGE_TYPE_MASK             (0x30u)

/* point Status defines */
#define USBFS_EP_STATUS_LENGTH               (0x02u)

/* point Device defines */
#define USBFS_DEVICE_STATUS_LENGTH           (0x02u)

#define USBFS_STATUS_LENGTH_MAX \
                 ( (USBFS_EP_STATUS_LENGTH > USBFS_DEVICE_STATUS_LENGTH) ? \
                    USBFS_EP_STATUS_LENGTH : USBFS_DEVICE_STATUS_LENGTH )
/* Transfer Completion Notification */
#define USBFS_XFER_IDLE                      (0x00u)
#define USBFS_XFER_STATUS_ACK                (0x01u)
#define USBFS_XFER_PREMATURE                 (0x02u)
#define USBFS_XFER_ERROR                     (0x03u)

/* Driver State defines */
#define USBFS_TRANS_STATE_IDLE               (0x00u)
#define USBFS_TRANS_STATE_CONTROL_READ       (0x02u)
#define USBFS_TRANS_STATE_CONTROL_WRITE      (0x04u)
#define USBFS_TRANS_STATE_NO_DATA_CONTROL    (0x06u)

/* String Descriptor defines */
#define USBFS_STRING_MSOS                    (0xEEu)
#define USBFS_MSOS_DESCRIPTOR_LENGTH         (18u)
#define USBFS_MSOS_CONF_DESCR_LENGTH         (40u)

#if(USBFS_EP_MM == USBFS__EP_DMAMANUAL)
    /* DMA manual mode defines */
    #define USBFS_DMA_BYTES_PER_BURST        (0u)
    #define USBFS_DMA_REQUEST_PER_BURST      (0u)
#endif /*  USBFS_EP_MM == USBFS__EP_DMAMANUAL */
#if(USBFS_EP_MM == USBFS__EP_DMAAUTO)
    /* DMA automatic mode defines */
    #define USBFS_DMA_BYTES_PER_BURST        (32u)
    #define USBFS_DMA_BYTES_REPEAT           (2u)
    /* BUF_SIZE-BYTES_PER_BURST examples: 55-32 bytes  44-16 bytes 33-8 bytes 22-4 bytes 11-2 bytes */
    #define USBFS_DMA_BUF_SIZE               (0x55u)
    #define USBFS_DMA_REQUEST_PER_BURST      (1u)

    #if(USBFS_DMA1_REMOVE == 0u)
        #define USBFS_ep1_TD_TERMOUT_EN      USBFS_ep1__TD_TERMOUT_EN
    #else
        #define USBFS_ep1_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA1_REMOVE == 0u */
    #if(USBFS_DMA2_REMOVE == 0u)
        #define USBFS_ep2_TD_TERMOUT_EN      USBFS_ep2__TD_TERMOUT_EN
    #else
        #define USBFS_ep2_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA2_REMOVE == 0u */
    #if(USBFS_DMA3_REMOVE == 0u)
        #define USBFS_ep3_TD_TERMOUT_EN      USBFS_ep3__TD_TERMOUT_EN
    #else
        #define USBFS_ep3_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA3_REMOVE == 0u */
    #if(USBFS_DMA4_REMOVE == 0u)
        #define USBFS_ep4_TD_TERMOUT_EN      USBFS_ep4__TD_TERMOUT_EN
    #else
        #define USBFS_ep4_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA4_REMOVE == 0u */
    #if(USBFS_DMA5_REMOVE == 0u)
        #define USBFS_ep5_TD_TERMOUT_EN      USBFS_ep5__TD_TERMOUT_EN
    #else
        #define USBFS_ep5_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA5_REMOVE == 0u */
    #if(USBFS_DMA6_REMOVE == 0u)
        #define USBFS_ep6_TD_TERMOUT_EN      USBFS_ep6__TD_TERMOUT_EN
    #else
        #define USBFS_ep6_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA6_REMOVE == 0u */
    #if(USBFS_DMA7_REMOVE == 0u)
        #define USBFS_ep7_TD_TERMOUT_EN      USBFS_ep7__TD_TERMOUT_EN
    #else
        #define USBFS_ep7_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA7_REMOVE == 0u */
    #if(USBFS_DMA8_REMOVE == 0u)
        #define USBFS_ep8_TD_TERMOUT_EN      USBFS_ep8__TD_TERMOUT_EN
    #else
        #define USBFS_ep8_TD_TERMOUT_EN      (0u)
    #endif /* USBFS_DMA8_REMOVE == 0u */

    #define     USBFS_EP17_SR_MASK           (0x7fu)
    #define     USBFS_EP8_SR_MASK            (0x03u)

#endif /*  USBFS_EP_MM == USBFS__EP_DMAAUTO */

/* DIE ID string descriptor defines */
#if defined(USBFS_ENABLE_IDSN_STRING)
    #define USBFS_IDSN_DESCR_LENGTH          (0x22u)
#endif /* USBFS_ENABLE_IDSN_STRING */


/***************************************
* External data references
***************************************/

extern uint8 USBFS_initVar;
extern volatile uint8 USBFS_device;
extern volatile uint8 USBFS_transferState;
extern volatile uint8 USBFS_configuration;
extern volatile uint8 USBFS_configurationChanged;
extern volatile uint8 USBFS_deviceStatus;

/* HID Variables */
#if defined(USBFS_ENABLE_HID_CLASS)
    extern volatile uint8 USBFS_hidProtocol[USBFS_MAX_INTERFACES_NUMBER];
    extern volatile uint8 USBFS_hidIdleRate[USBFS_MAX_INTERFACES_NUMBER];
    extern volatile uint8 USBFS_hidIdleTimer[USBFS_MAX_INTERFACES_NUMBER];
#endif /* USBFS_ENABLE_HID_CLASS */


/***************************************
*              Registers
***************************************/

#define USBFS_ARB_CFG_PTR        (  (reg8 *) USBFS_USB__ARB_CFG)
#define USBFS_ARB_CFG_REG        (* (reg8 *) USBFS_USB__ARB_CFG)

#define USBFS_ARB_EP1_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP1_CFG)
#define USBFS_ARB_EP1_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP1_CFG)
#define USBFS_ARB_EP1_CFG_IND    USBFS_USB__ARB_EP1_CFG
#define USBFS_ARB_EP1_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP1_INT_EN)
#define USBFS_ARB_EP1_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP1_INT_EN)
#define USBFS_ARB_EP1_INT_EN_IND USBFS_USB__ARB_EP1_INT_EN
#define USBFS_ARB_EP1_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP1_SR)
#define USBFS_ARB_EP1_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP1_SR)
#define USBFS_ARB_EP1_SR_IND     USBFS_USB__ARB_EP1_SR

#define USBFS_ARB_EP2_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP2_CFG)
#define USBFS_ARB_EP2_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP2_CFG)
#define USBFS_ARB_EP2_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP2_INT_EN)
#define USBFS_ARB_EP2_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP2_INT_EN)
#define USBFS_ARB_EP2_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP2_SR)
#define USBFS_ARB_EP2_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP2_SR)

#define USBFS_ARB_EP3_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP3_CFG)
#define USBFS_ARB_EP3_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP3_CFG)
#define USBFS_ARB_EP3_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP3_INT_EN)
#define USBFS_ARB_EP3_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP3_INT_EN)
#define USBFS_ARB_EP3_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP3_SR)
#define USBFS_ARB_EP3_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP3_SR)

#define USBFS_ARB_EP4_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP4_CFG)
#define USBFS_ARB_EP4_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP4_CFG)
#define USBFS_ARB_EP4_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP4_INT_EN)
#define USBFS_ARB_EP4_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP4_INT_EN)
#define USBFS_ARB_EP4_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP4_SR)
#define USBFS_ARB_EP4_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP4_SR)

#define USBFS_ARB_EP5_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP5_CFG)
#define USBFS_ARB_EP5_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP5_CFG)
#define USBFS_ARB_EP5_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP5_INT_EN)
#define USBFS_ARB_EP5_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP5_INT_EN)
#define USBFS_ARB_EP5_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP5_SR)
#define USBFS_ARB_EP5_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP5_SR)

#define USBFS_ARB_EP6_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP6_CFG)
#define USBFS_ARB_EP6_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP6_CFG)
#define USBFS_ARB_EP6_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP6_INT_EN)
#define USBFS_ARB_EP6_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP6_INT_EN)
#define USBFS_ARB_EP6_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP6_SR)
#define USBFS_ARB_EP6_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP6_SR)

#define USBFS_ARB_EP7_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP7_CFG)
#define USBFS_ARB_EP7_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP7_CFG)
#define USBFS_ARB_EP7_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP7_INT_EN)
#define USBFS_ARB_EP7_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP7_INT_EN)
#define USBFS_ARB_EP7_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP7_SR)
#define USBFS_ARB_EP7_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP7_SR)

#define USBFS_ARB_EP8_CFG_PTR    (  (reg8 *) USBFS_USB__ARB_EP8_CFG)
#define USBFS_ARB_EP8_CFG_REG    (* (reg8 *) USBFS_USB__ARB_EP8_CFG)
#define USBFS_ARB_EP8_INT_EN_PTR (  (reg8 *) USBFS_USB__ARB_EP8_INT_EN)
#define USBFS_ARB_EP8_INT_EN_REG (* (reg8 *) USBFS_USB__ARB_EP8_INT_EN)
#define USBFS_ARB_EP8_SR_PTR     (  (reg8 *) USBFS_USB__ARB_EP8_SR)
#define USBFS_ARB_EP8_SR_REG     (* (reg8 *) USBFS_USB__ARB_EP8_SR)

#define USBFS_ARB_INT_EN_PTR     (  (reg8 *) USBFS_USB__ARB_INT_EN)
#define USBFS_ARB_INT_EN_REG     (* (reg8 *) USBFS_USB__ARB_INT_EN)
#define USBFS_ARB_INT_SR_PTR     (  (reg8 *) USBFS_USB__ARB_INT_SR)
#define USBFS_ARB_INT_SR_REG     (* (reg8 *) USBFS_USB__ARB_INT_SR)

#define USBFS_ARB_RW1_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW1_DR)
#define USBFS_ARB_RW1_DR_IND     USBFS_USB__ARB_RW1_DR
#define USBFS_ARB_RW1_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW1_RA)
#define USBFS_ARB_RW1_RA_IND     USBFS_USB__ARB_RW1_RA
#define USBFS_ARB_RW1_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW1_RA_MSB)
#define USBFS_ARB_RW1_RA_MSB_IND USBFS_USB__ARB_RW1_RA_MSB
#define USBFS_ARB_RW1_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW1_WA)
#define USBFS_ARB_RW1_WA_IND     USBFS_USB__ARB_RW1_WA
#define USBFS_ARB_RW1_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW1_WA_MSB)
#define USBFS_ARB_RW1_WA_MSB_IND USBFS_USB__ARB_RW1_WA_MSB

#define USBFS_ARB_RW2_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW2_DR)
#define USBFS_ARB_RW2_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW2_RA)
#define USBFS_ARB_RW2_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW2_RA_MSB)
#define USBFS_ARB_RW2_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW2_WA)
#define USBFS_ARB_RW2_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW2_WA_MSB)

#define USBFS_ARB_RW3_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW3_DR)
#define USBFS_ARB_RW3_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW3_RA)
#define USBFS_ARB_RW3_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW3_RA_MSB)
#define USBFS_ARB_RW3_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW3_WA)
#define USBFS_ARB_RW3_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW3_WA_MSB)

#define USBFS_ARB_RW4_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW4_DR)
#define USBFS_ARB_RW4_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW4_RA)
#define USBFS_ARB_RW4_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW4_RA_MSB)
#define USBFS_ARB_RW4_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW4_WA)
#define USBFS_ARB_RW4_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW4_WA_MSB)

#define USBFS_ARB_RW5_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW5_DR)
#define USBFS_ARB_RW5_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW5_RA)
#define USBFS_ARB_RW5_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW5_RA_MSB)
#define USBFS_ARB_RW5_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW5_WA)
#define USBFS_ARB_RW5_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW5_WA_MSB)

#define USBFS_ARB_RW6_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW6_DR)
#define USBFS_ARB_RW6_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW6_RA)
#define USBFS_ARB_RW6_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW6_RA_MSB)
#define USBFS_ARB_RW6_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW6_WA)
#define USBFS_ARB_RW6_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW6_WA_MSB)

#define USBFS_ARB_RW7_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW7_DR)
#define USBFS_ARB_RW7_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW7_RA)
#define USBFS_ARB_RW7_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW7_RA_MSB)
#define USBFS_ARB_RW7_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW7_WA)
#define USBFS_ARB_RW7_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW7_WA_MSB)

#define USBFS_ARB_RW8_DR_PTR     ((reg8 *) USBFS_USB__ARB_RW8_DR)
#define USBFS_ARB_RW8_RA_PTR     ((reg8 *) USBFS_USB__ARB_RW8_RA)
#define USBFS_ARB_RW8_RA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW8_RA_MSB)
#define USBFS_ARB_RW8_WA_PTR     ((reg8 *) USBFS_USB__ARB_RW8_WA)
#define USBFS_ARB_RW8_WA_MSB_PTR ((reg8 *) USBFS_USB__ARB_RW8_WA_MSB)

#define USBFS_BUF_SIZE_PTR       (  (reg8 *) USBFS_USB__BUF_SIZE)
#define USBFS_BUF_SIZE_REG       (* (reg8 *) USBFS_USB__BUF_SIZE)
#define USBFS_BUS_RST_CNT_PTR    (  (reg8 *) USBFS_USB__BUS_RST_CNT)
#define USBFS_BUS_RST_CNT_REG    (* (reg8 *) USBFS_USB__BUS_RST_CNT)
#define USBFS_CWA_PTR            (  (reg8 *) USBFS_USB__CWA)
#define USBFS_CWA_REG            (* (reg8 *) USBFS_USB__CWA)
#define USBFS_CWA_MSB_PTR        (  (reg8 *) USBFS_USB__CWA_MSB)
#define USBFS_CWA_MSB_REG        (* (reg8 *) USBFS_USB__CWA_MSB)
#define USBFS_CR0_PTR            (  (reg8 *) USBFS_USB__CR0)
#define USBFS_CR0_REG            (* (reg8 *) USBFS_USB__CR0)
#define USBFS_CR1_PTR            (  (reg8 *) USBFS_USB__CR1)
#define USBFS_CR1_REG            (* (reg8 *) USBFS_USB__CR1)

#define USBFS_DMA_THRES_PTR      (  (reg8 *) USBFS_USB__DMA_THRES)
#define USBFS_DMA_THRES_REG      (* (reg8 *) USBFS_USB__DMA_THRES)
#define USBFS_DMA_THRES_MSB_PTR  (  (reg8 *) USBFS_USB__DMA_THRES_MSB)
#define USBFS_DMA_THRES_MSB_REG  (* (reg8 *) USBFS_USB__DMA_THRES_MSB)

#define USBFS_EP_ACTIVE_PTR      (  (reg8 *) USBFS_USB__EP_ACTIVE)
#define USBFS_EP_ACTIVE_REG      (* (reg8 *) USBFS_USB__EP_ACTIVE)
#define USBFS_EP_TYPE_PTR        (  (reg8 *) USBFS_USB__EP_TYPE)
#define USBFS_EP_TYPE_REG        (* (reg8 *) USBFS_USB__EP_TYPE)

#define USBFS_EP0_CNT_PTR        (  (reg8 *) USBFS_USB__EP0_CNT)
#define USBFS_EP0_CNT_REG        (* (reg8 *) USBFS_USB__EP0_CNT)
#define USBFS_EP0_CR_PTR         (  (reg8 *) USBFS_USB__EP0_CR)
#define USBFS_EP0_CR_REG         (* (reg8 *) USBFS_USB__EP0_CR)
#define USBFS_EP0_DR0_PTR        (  (reg8 *) USBFS_USB__EP0_DR0)
#define USBFS_EP0_DR0_REG        (* (reg8 *) USBFS_USB__EP0_DR0)
#define USBFS_EP0_DR0_IND        USBFS_USB__EP0_DR0
#define USBFS_EP0_DR1_PTR        (  (reg8 *) USBFS_USB__EP0_DR1)
#define USBFS_EP0_DR1_REG        (* (reg8 *) USBFS_USB__EP0_DR1)
#define USBFS_EP0_DR2_PTR        (  (reg8 *) USBFS_USB__EP0_DR2)
#define USBFS_EP0_DR2_REG        (* (reg8 *) USBFS_USB__EP0_DR2)
#define USBFS_EP0_DR3_PTR        (  (reg8 *) USBFS_USB__EP0_DR3)
#define USBFS_EP0_DR3_REG        (* (reg8 *) USBFS_USB__EP0_DR3)
#define USBFS_EP0_DR4_PTR        (  (reg8 *) USBFS_USB__EP0_DR4)
#define USBFS_EP0_DR4_REG        (* (reg8 *) USBFS_USB__EP0_DR4)
#define USBFS_EP0_DR5_PTR        (  (reg8 *) USBFS_USB__EP0_DR5)
#define USBFS_EP0_DR5_REG        (* (reg8 *) USBFS_USB__EP0_DR5)
#define USBFS_EP0_DR6_PTR        (  (reg8 *) USBFS_USB__EP0_DR6)
#define USBFS_EP0_DR6_REG        (* (reg8 *) USBFS_USB__EP0_DR6)
#define USBFS_EP0_DR7_PTR        (  (reg8 *) USBFS_USB__EP0_DR7)
#define USBFS_EP0_DR7_REG        (* (reg8 *) USBFS_USB__EP0_DR7)

#define USBFS_OSCLK_DR0_PTR      (  (reg8 *) USBFS_USB__OSCLK_DR0)
#define USBFS_OSCLK_DR0_REG      (* (reg8 *) USBFS_USB__OSCLK_DR0)
#define USBFS_OSCLK_DR1_PTR      (  (reg8 *) USBFS_USB__OSCLK_DR1)
#define USBFS_OSCLK_DR1_REG      (* (reg8 *) USBFS_USB__OSCLK_DR1)

#define USBFS_PM_ACT_CFG_PTR     (  (reg8 *) USBFS_USB__PM_ACT_CFG)
#define USBFS_PM_ACT_CFG_REG     (* (reg8 *) USBFS_USB__PM_ACT_CFG)
#define USBFS_PM_STBY_CFG_PTR    (  (reg8 *) USBFS_USB__PM_STBY_CFG)
#define USBFS_PM_STBY_CFG_REG    (* (reg8 *) USBFS_USB__PM_STBY_CFG)

#define USBFS_SIE_EP_INT_EN_PTR  (  (reg8 *) USBFS_USB__SIE_EP_INT_EN)
#define USBFS_SIE_EP_INT_EN_REG  (* (reg8 *) USBFS_USB__SIE_EP_INT_EN)
#define USBFS_SIE_EP_INT_SR_PTR  (  (reg8 *) USBFS_USB__SIE_EP_INT_SR)
#define USBFS_SIE_EP_INT_SR_REG  (* (reg8 *) USBFS_USB__SIE_EP_INT_SR)

#define USBFS_SIE_EP1_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP1_CNT0)
#define USBFS_SIE_EP1_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP1_CNT0)
#define USBFS_SIE_EP1_CNT0_IND   USBFS_USB__SIE_EP1_CNT0
#define USBFS_SIE_EP1_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP1_CNT1)
#define USBFS_SIE_EP1_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP1_CNT1)
#define USBFS_SIE_EP1_CNT1_IND   USBFS_USB__SIE_EP1_CNT1
#define USBFS_SIE_EP1_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP1_CR0)
#define USBFS_SIE_EP1_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP1_CR0)
#define USBFS_SIE_EP1_CR0_IND    USBFS_USB__SIE_EP1_CR0

#define USBFS_SIE_EP2_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP2_CNT0)
#define USBFS_SIE_EP2_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP2_CNT0)
#define USBFS_SIE_EP2_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP2_CNT1)
#define USBFS_SIE_EP2_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP2_CNT1)
#define USBFS_SIE_EP2_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP2_CR0)
#define USBFS_SIE_EP2_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP2_CR0)

#define USBFS_SIE_EP3_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP3_CNT0)
#define USBFS_SIE_EP3_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP3_CNT0)
#define USBFS_SIE_EP3_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP3_CNT1)
#define USBFS_SIE_EP3_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP3_CNT1)
#define USBFS_SIE_EP3_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP3_CR0)
#define USBFS_SIE_EP3_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP3_CR0)

#define USBFS_SIE_EP4_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP4_CNT0)
#define USBFS_SIE_EP4_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP4_CNT0)
#define USBFS_SIE_EP4_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP4_CNT1)
#define USBFS_SIE_EP4_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP4_CNT1)
#define USBFS_SIE_EP4_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP4_CR0)
#define USBFS_SIE_EP4_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP4_CR0)

#define USBFS_SIE_EP5_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP5_CNT0)
#define USBFS_SIE_EP5_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP5_CNT0)
#define USBFS_SIE_EP5_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP5_CNT1)
#define USBFS_SIE_EP5_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP5_CNT1)
#define USBFS_SIE_EP5_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP5_CR0)
#define USBFS_SIE_EP5_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP5_CR0)

#define USBFS_SIE_EP6_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP6_CNT0)
#define USBFS_SIE_EP6_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP6_CNT0)
#define USBFS_SIE_EP6_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP6_CNT1)
#define USBFS_SIE_EP6_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP6_CNT1)
#define USBFS_SIE_EP6_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP6_CR0)
#define USBFS_SIE_EP6_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP6_CR0)

#define USBFS_SIE_EP7_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP7_CNT0)
#define USBFS_SIE_EP7_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP7_CNT0)
#define USBFS_SIE_EP7_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP7_CNT1)
#define USBFS_SIE_EP7_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP7_CNT1)
#define USBFS_SIE_EP7_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP7_CR0)
#define USBFS_SIE_EP7_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP7_CR0)

#define USBFS_SIE_EP8_CNT0_PTR   (  (reg8 *) USBFS_USB__SIE_EP8_CNT0)
#define USBFS_SIE_EP8_CNT0_REG   (* (reg8 *) USBFS_USB__SIE_EP8_CNT0)
#define USBFS_SIE_EP8_CNT1_PTR   (  (reg8 *) USBFS_USB__SIE_EP8_CNT1)
#define USBFS_SIE_EP8_CNT1_REG   (* (reg8 *) USBFS_USB__SIE_EP8_CNT1)
#define USBFS_SIE_EP8_CR0_PTR    (  (reg8 *) USBFS_USB__SIE_EP8_CR0)
#define USBFS_SIE_EP8_CR0_REG    (* (reg8 *) USBFS_USB__SIE_EP8_CR0)

#define USBFS_SOF0_PTR           (  (reg8 *) USBFS_USB__SOF0)
#define USBFS_SOF0_REG           (* (reg8 *) USBFS_USB__SOF0)
#define USBFS_SOF1_PTR           (  (reg8 *) USBFS_USB__SOF1)
#define USBFS_SOF1_REG           (* (reg8 *) USBFS_USB__SOF1)

#define USBFS_USB_CLK_EN_PTR     (  (reg8 *) USBFS_USB__USB_CLK_EN)
#define USBFS_USB_CLK_EN_REG     (* (reg8 *) USBFS_USB__USB_CLK_EN)

#define USBFS_USBIO_CR0_PTR      (  (reg8 *) USBFS_USB__USBIO_CR0)
#define USBFS_USBIO_CR0_REG      (* (reg8 *) USBFS_USB__USBIO_CR0)
#define USBFS_USBIO_CR1_PTR      (  (reg8 *) USBFS_USB__USBIO_CR1)
#define USBFS_USBIO_CR1_REG      (* (reg8 *) USBFS_USB__USBIO_CR1)
#if(!CY_PSOC5LP)
    #define USBFS_USBIO_CR2_PTR      (  (reg8 *) USBFS_USB__USBIO_CR2)
    #define USBFS_USBIO_CR2_REG      (* (reg8 *) USBFS_USB__USBIO_CR2)
#endif /*  CY_PSOC5LP */

#define USBFS_DIE_ID             CYDEV_FLSHID_CUST_TABLES_BASE

#define USBFS_PM_USB_CR0_PTR     (  (reg8 *) CYREG_PM_USB_CR0)
#define USBFS_PM_USB_CR0_REG     (* (reg8 *) CYREG_PM_USB_CR0)
#define USBFS_DYN_RECONFIG_PTR   (  (reg8 *) USBFS_USB__DYN_RECONFIG)
#define USBFS_DYN_RECONFIG_REG   (* (reg8 *) USBFS_USB__DYN_RECONFIG)

#define USBFS_DM_INP_DIS_PTR     (  (reg8 *) USBFS_Dm__INP_DIS)
#define USBFS_DM_INP_DIS_REG     (* (reg8 *) USBFS_Dm__INP_DIS)
#define USBFS_DP_INP_DIS_PTR     (  (reg8 *) USBFS_Dp__INP_DIS)
#define USBFS_DP_INP_DIS_REG     (* (reg8 *) USBFS_Dp__INP_DIS)
#define USBFS_DP_INTSTAT_PTR     (  (reg8 *) USBFS_Dp__INTSTAT)
#define USBFS_DP_INTSTAT_REG     (* (reg8 *) USBFS_Dp__INTSTAT)

#if (USBFS_MON_VBUS == 1u)
    #if (USBFS_EXTERN_VBUS == 0u)
        #define USBFS_VBUS_DR_PTR        (  (reg8 *) USBFS_VBUS__DR)
        #define USBFS_VBUS_DR_REG        (* (reg8 *) USBFS_VBUS__DR)
        #define USBFS_VBUS_PS_PTR        (  (reg8 *) USBFS_VBUS__PS)
        #define USBFS_VBUS_PS_REG        (* (reg8 *) USBFS_VBUS__PS)
        #define USBFS_VBUS_MASK          USBFS_VBUS__MASK
    #else
        #define USBFS_VBUS_PS_PTR        (  (reg8 *) USBFS_Vbus_ps_sts_sts_reg__STATUS_REG )
        #define USBFS_VBUS_MASK          (0x01u)
    #endif /*  USBFS_EXTERN_VBUS == 0u */
#endif /*  USBFS_MON_VBUS */

/* Renamed Registers for backward compatibility.
*  Should not be used in new designs.
*/
#define USBFS_ARB_CFG        USBFS_ARB_CFG_PTR

#define USBFS_ARB_EP1_CFG    USBFS_ARB_EP1_CFG_PTR
#define USBFS_ARB_EP1_INT_EN USBFS_ARB_EP1_INT_EN_PTR
#define USBFS_ARB_EP1_SR     USBFS_ARB_EP1_SR_PTR

#define USBFS_ARB_EP2_CFG    USBFS_ARB_EP2_CFG_PTR
#define USBFS_ARB_EP2_INT_EN USBFS_ARB_EP2_INT_EN_PTR
#define USBFS_ARB_EP2_SR     USBFS_ARB_EP2_SR_PTR

#define USBFS_ARB_EP3_CFG    USBFS_ARB_EP3_CFG_PTR
#define USBFS_ARB_EP3_INT_EN USBFS_ARB_EP3_INT_EN_PTR
#define USBFS_ARB_EP3_SR     USBFS_ARB_EP3_SR_PTR

#define USBFS_ARB_EP4_CFG    USBFS_ARB_EP4_CFG_PTR
#define USBFS_ARB_EP4_INT_EN USBFS_ARB_EP4_INT_EN_PTR
#define USBFS_ARB_EP4_SR     USBFS_ARB_EP4_SR_PTR

#define USBFS_ARB_EP5_CFG    USBFS_ARB_EP5_CFG_PTR
#define USBFS_ARB_EP5_INT_EN USBFS_ARB_EP5_INT_EN_PTR
#define USBFS_ARB_EP5_SR     USBFS_ARB_EP5_SR_PTR

#define USBFS_ARB_EP6_CFG    USBFS_ARB_EP6_CFG_PTR
#define USBFS_ARB_EP6_INT_EN USBFS_ARB_EP6_INT_EN_PTR
#define USBFS_ARB_EP6_SR     USBFS_ARB_EP6_SR_PTR

#define USBFS_ARB_EP7_CFG    USBFS_ARB_EP7_CFG_PTR
#define USBFS_ARB_EP7_INT_EN USBFS_ARB_EP7_INT_EN_PTR
#define USBFS_ARB_EP7_SR     USBFS_ARB_EP7_SR_PTR

#define USBFS_ARB_EP8_CFG    USBFS_ARB_EP8_CFG_PTR
#define USBFS_ARB_EP8_INT_EN USBFS_ARB_EP8_INT_EN_PTR
#define USBFS_ARB_EP8_SR     USBFS_ARB_EP8_SR_PTR

#define USBFS_ARB_INT_EN     USBFS_ARB_INT_EN_PTR
#define USBFS_ARB_INT_SR     USBFS_ARB_INT_SR_PTR

#define USBFS_ARB_RW1_DR     USBFS_ARB_RW1_DR_PTR
#define USBFS_ARB_RW1_RA     USBFS_ARB_RW1_RA_PTR
#define USBFS_ARB_RW1_RA_MSB USBFS_ARB_RW1_RA_MSB_PTR
#define USBFS_ARB_RW1_WA     USBFS_ARB_RW1_WA_PTR
#define USBFS_ARB_RW1_WA_MSB USBFS_ARB_RW1_WA_MSB_PTR

#define USBFS_ARB_RW2_DR     USBFS_ARB_RW2_DR_PTR
#define USBFS_ARB_RW2_RA     USBFS_ARB_RW2_RA_PTR
#define USBFS_ARB_RW2_RA_MSB USBFS_ARB_RW2_RA_MSB_PTR
#define USBFS_ARB_RW2_WA     USBFS_ARB_RW2_WA_PTR
#define USBFS_ARB_RW2_WA_MSB USBFS_ARB_RW2_WA_MSB_PTR

#define USBFS_ARB_RW3_DR     USBFS_ARB_RW3_DR_PTR
#define USBFS_ARB_RW3_RA     USBFS_ARB_RW3_RA_PTR
#define USBFS_ARB_RW3_RA_MSB USBFS_ARB_RW3_RA_MSB_PTR
#define USBFS_ARB_RW3_WA     USBFS_ARB_RW3_WA_PTR
#define USBFS_ARB_RW3_WA_MSB USBFS_ARB_RW3_WA_MSB_PTR

#define USBFS_ARB_RW4_DR     USBFS_ARB_RW4_DR_PTR
#define USBFS_ARB_RW4_RA     USBFS_ARB_RW4_RA_PTR
#define USBFS_ARB_RW4_RA_MSB USBFS_ARB_RW4_RA_MSB_PTR
#define USBFS_ARB_RW4_WA     USBFS_ARB_RW4_WA_PTR
#define USBFS_ARB_RW4_WA_MSB USBFS_ARB_RW4_WA_MSB_PTR

#define USBFS_ARB_RW5_DR     USBFS_ARB_RW5_DR_PTR
#define USBFS_ARB_RW5_RA     USBFS_ARB_RW5_RA_PTR
#define USBFS_ARB_RW5_RA_MSB USBFS_ARB_RW5_RA_MSB_PTR
#define USBFS_ARB_RW5_WA     USBFS_ARB_RW5_WA_PTR
#define USBFS_ARB_RW5_WA_MSB USBFS_ARB_RW5_WA_MSB_PTR

#define USBFS_ARB_RW6_DR     USBFS_ARB_RW6_DR_PTR
#define USBFS_ARB_RW6_RA     USBFS_ARB_RW6_RA_PTR
#define USBFS_ARB_RW6_RA_MSB USBFS_ARB_RW6_RA_MSB_PTR
#define USBFS_ARB_RW6_WA     USBFS_ARB_RW6_WA_PTR
#define USBFS_ARB_RW6_WA_MSB USBFS_ARB_RW6_WA_MSB_PTR

#define USBFS_ARB_RW7_DR     USBFS_ARB_RW7_DR_PTR
#define USBFS_ARB_RW7_RA     USBFS_ARB_RW7_RA_PTR
#define USBFS_ARB_RW7_RA_MSB USBFS_ARB_RW7_RA_MSB_PTR
#define USBFS_ARB_RW7_WA     USBFS_ARB_RW7_WA_PTR
#define USBFS_ARB_RW7_WA_MSB USBFS_ARB_RW7_WA_MSB_PTR

#define USBFS_ARB_RW8_DR     USBFS_ARB_RW8_DR_PTR
#define USBFS_ARB_RW8_RA     USBFS_ARB_RW8_RA_PTR
#define USBFS_ARB_RW8_RA_MSB USBFS_ARB_RW8_RA_MSB_PTR
#define USBFS_ARB_RW8_WA     USBFS_ARB_RW8_WA_PTR
#define USBFS_ARB_RW8_WA_MSB USBFS_ARB_RW8_WA_MSB_PTR

#define USBFS_BUF_SIZE       USBFS_BUF_SIZE_PTR
#define USBFS_BUS_RST_CNT    USBFS_BUS_RST_CNT_PTR
#define USBFS_CR0            USBFS_CR0_PTR
#define USBFS_CR1            USBFS_CR1_PTR
#define USBFS_CWA            USBFS_CWA_PTR
#define USBFS_CWA_MSB        USBFS_CWA_MSB_PTR

#define USBFS_DMA_THRES      USBFS_DMA_THRES_PTR
#define USBFS_DMA_THRES_MSB  USBFS_DMA_THRES_MSB_PTR

#define USBFS_EP_ACTIVE      USBFS_EP_ACTIVE_PTR
#define USBFS_EP_TYPE        USBFS_EP_TYPE_PTR

#define USBFS_EP0_CNT        USBFS_EP0_CNT_PTR
#define USBFS_EP0_CR         USBFS_EP0_CR_PTR
#define USBFS_EP0_DR0        USBFS_EP0_DR0_PTR
#define USBFS_EP0_DR1        USBFS_EP0_DR1_PTR
#define USBFS_EP0_DR2        USBFS_EP0_DR2_PTR
#define USBFS_EP0_DR3        USBFS_EP0_DR3_PTR
#define USBFS_EP0_DR4        USBFS_EP0_DR4_PTR
#define USBFS_EP0_DR5        USBFS_EP0_DR5_PTR
#define USBFS_EP0_DR6        USBFS_EP0_DR6_PTR
#define USBFS_EP0_DR7        USBFS_EP0_DR7_PTR

#define USBFS_OSCLK_DR0      USBFS_OSCLK_DR0_PTR
#define USBFS_OSCLK_DR1      USBFS_OSCLK_DR1_PTR

#define USBFS_PM_ACT_CFG     USBFS_PM_ACT_CFG_PTR
#define USBFS_PM_STBY_CFG    USBFS_PM_STBY_CFG_PTR

#define USBFS_SIE_EP_INT_EN  USBFS_SIE_EP_INT_EN_PTR
#define USBFS_SIE_EP_INT_SR  USBFS_SIE_EP_INT_SR_PTR

#define USBFS_SIE_EP1_CNT0   USBFS_SIE_EP1_CNT0_PTR
#define USBFS_SIE_EP1_CNT1   USBFS_SIE_EP1_CNT1_PTR
#define USBFS_SIE_EP1_CR0    USBFS_SIE_EP1_CR0_PTR

#define USBFS_SIE_EP2_CNT0   USBFS_SIE_EP2_CNT0_PTR
#define USBFS_SIE_EP2_CNT1   USBFS_SIE_EP2_CNT1_PTR
#define USBFS_SIE_EP2_CR0    USBFS_SIE_EP2_CR0_PTR

#define USBFS_SIE_EP3_CNT0   USBFS_SIE_EP3_CNT0_PTR
#define USBFS_SIE_EP3_CNT1   USBFS_SIE_EP3_CNT1_PTR
#define USBFS_SIE_EP3_CR0    USBFS_SIE_EP3_CR0_PTR

#define USBFS_SIE_EP4_CNT0   USBFS_SIE_EP4_CNT0_PTR
#define USBFS_SIE_EP4_CNT1   USBFS_SIE_EP4_CNT1_PTR
#define USBFS_SIE_EP4_CR0    USBFS_SIE_EP4_CR0_PTR

#define USBFS_SIE_EP5_CNT0   USBFS_SIE_EP5_CNT0_PTR
#define USBFS_SIE_EP5_CNT1   USBFS_SIE_EP5_CNT1_PTR
#define USBFS_SIE_EP5_CR0    USBFS_SIE_EP5_CR0_PTR

#define USBFS_SIE_EP6_CNT0   USBFS_SIE_EP6_CNT0_PTR
#define USBFS_SIE_EP6_CNT1   USBFS_SIE_EP6_CNT1_PTR
#define USBFS_SIE_EP6_CR0    USBFS_SIE_EP6_CR0_PTR

#define USBFS_SIE_EP7_CNT0   USBFS_SIE_EP7_CNT0_PTR
#define USBFS_SIE_EP7_CNT1   USBFS_SIE_EP7_CNT1_PTR
#define USBFS_SIE_EP7_CR0    USBFS_SIE_EP7_CR0_PTR

#define USBFS_SIE_EP8_CNT0   USBFS_SIE_EP8_CNT0_PTR
#define USBFS_SIE_EP8_CNT1   USBFS_SIE_EP8_CNT1_PTR
#define USBFS_SIE_EP8_CR0    USBFS_SIE_EP8_CR0_PTR

#define USBFS_SOF0           USBFS_SOF0_PTR
#define USBFS_SOF1           USBFS_SOF1_PTR

#define USBFS_USB_CLK_EN     USBFS_USB_CLK_EN_PTR

#define USBFS_USBIO_CR0      USBFS_USBIO_CR0_PTR
#define USBFS_USBIO_CR1      USBFS_USBIO_CR1_PTR
#define USBFS_USBIO_CR2      USBFS_USBIO_CR2_PTR

#define USBFS_USB_MEM        ((reg8 *) CYDEV_USB_MEM_BASE)

#if(CYDEV_CHIP_DIE_EXPECT == CYDEV_CHIP_DIE_LEOPARD)
    /* PSoC3 interrupt registers*/
    #define USBFS_USB_ISR_PRIOR  ((reg8 *) CYDEV_INTC_PRIOR0)
    #define USBFS_USB_ISR_SET_EN ((reg8 *) CYDEV_INTC_SET_EN0)
    #define USBFS_USB_ISR_CLR_EN ((reg8 *) CYDEV_INTC_CLR_EN0)
    #define USBFS_USB_ISR_VECT   ((cyisraddress *) CYDEV_INTC_VECT_MBASE)
#elif(CYDEV_CHIP_DIE_EXPECT == CYDEV_CHIP_DIE_PANTHER)
    /* PSoC5 interrupt registers*/
    #define USBFS_USB_ISR_PRIOR  ((reg8 *) CYDEV_NVIC_PRI_0)
    #define USBFS_USB_ISR_SET_EN ((reg8 *) CYDEV_NVIC_SETENA0)
    #define USBFS_USB_ISR_CLR_EN ((reg8 *) CYDEV_NVIC_CLRENA0)
    #define USBFS_USB_ISR_VECT   ((cyisraddress *) CYDEV_NVIC_VECT_OFFSET)
#endif /*  CYDEV_CHIP_DIE_EXPECT */


/***************************************
* Interrupt vectors, masks and priorities
***************************************/

#define USBFS_BUS_RESET_PRIOR    USBFS_bus_reset__INTC_PRIOR_NUM
#define USBFS_BUS_RESET_MASK     USBFS_bus_reset__INTC_MASK
#define USBFS_BUS_RESET_VECT_NUM USBFS_bus_reset__INTC_NUMBER

#define USBFS_SOF_PRIOR          USBFS_sof_int__INTC_PRIOR_NUM
#define USBFS_SOF_MASK           USBFS_sof_int__INTC_MASK
#define USBFS_SOF_VECT_NUM       USBFS_sof_int__INTC_NUMBER

#define USBFS_EP_0_PRIOR         USBFS_ep_0__INTC_PRIOR_NUM
#define USBFS_EP_0_MASK          USBFS_ep_0__INTC_MASK
#define USBFS_EP_0_VECT_NUM      USBFS_ep_0__INTC_NUMBER

#define USBFS_EP_1_PRIOR         USBFS_ep_1__INTC_PRIOR_NUM
#define USBFS_EP_1_MASK          USBFS_ep_1__INTC_MASK
#define USBFS_EP_1_VECT_NUM      USBFS_ep_1__INTC_NUMBER

#define USBFS_EP_2_PRIOR         USBFS_ep_2__INTC_PRIOR_NUM
#define USBFS_EP_2_MASK          USBFS_ep_2__INTC_MASK
#define USBFS_EP_2_VECT_NUM      USBFS_ep_2__INTC_NUMBER

#define USBFS_EP_3_PRIOR         USBFS_ep_3__INTC_PRIOR_NUM
#define USBFS_EP_3_MASK          USBFS_ep_3__INTC_MASK
#define USBFS_EP_3_VECT_NUM      USBFS_ep_3__INTC_NUMBER

#define USBFS_EP_4_PRIOR         USBFS_ep_4__INTC_PRIOR_NUM
#define USBFS_EP_4_MASK          USBFS_ep_4__INTC_MASK
#define USBFS_EP_4_VECT_NUM      USBFS_ep_4__INTC_NUMBER

#define USBFS_EP_5_PRIOR         USBFS_ep_5__INTC_PRIOR_NUM
#define USBFS_EP_5_MASK          USBFS_ep_5__INTC_MASK
#define USBFS_EP_5_VECT_NUM      USBFS_ep_5__INTC_NUMBER

#define USBFS_EP_6_PRIOR         USBFS_ep_6__INTC_PRIOR_NUM
#define USBFS_EP_6_MASK          USBFS_ep_6__INTC_MASK
#define USBFS_EP_6_VECT_NUM      USBFS_ep_6__INTC_NUMBER

#define USBFS_EP_7_PRIOR         USBFS_ep_7__INTC_PRIOR_NUM
#define USBFS_EP_7_MASK          USBFS_ep_7__INTC_MASK
#define USBFS_EP_7_VECT_NUM      USBFS_ep_7__INTC_NUMBER

#define USBFS_EP_8_PRIOR         USBFS_ep_8__INTC_PRIOR_NUM
#define USBFS_EP_8_MASK          USBFS_ep_8__INTC_MASK
#define USBFS_EP_8_VECT_NUM      USBFS_ep_8__INTC_NUMBER

#define USBFS_DP_INTC_PRIOR      USBFS_dp_int__INTC_PRIOR_NUM
#define USBFS_DP_INTC_MASK       USBFS_dp_int__INTC_MASK
#define USBFS_DP_INTC_VECT_NUM   USBFS_dp_int__INTC_NUMBER

/* ARB ISR should have higher priority from EP_X ISR, therefore it is defined to highest (0) */
#define USBFS_ARB_PRIOR          (0u)
#define USBFS_ARB_MASK           USBFS_arb_int__INTC_MASK
#define USBFS_ARB_VECT_NUM       USBFS_arb_int__INTC_NUMBER

/***************************************
 *  Endpoint 0 offsets (Table 9-2)
 **************************************/

#define USBFS_bmRequestType      USBFS_EP0_DR0_PTR
#define USBFS_bRequest           USBFS_EP0_DR1_PTR
#define USBFS_wValue             USBFS_EP0_DR2_PTR
#define USBFS_wValueHi           USBFS_EP0_DR3_PTR
#define USBFS_wValueLo           USBFS_EP0_DR2_PTR
#define USBFS_wIndex             USBFS_EP0_DR4_PTR
#define USBFS_wIndexHi           USBFS_EP0_DR5_PTR
#define USBFS_wIndexLo           USBFS_EP0_DR4_PTR
#define USBFS_length             USBFS_EP0_DR6_PTR
#define USBFS_lengthHi           USBFS_EP0_DR7_PTR
#define USBFS_lengthLo           USBFS_EP0_DR6_PTR


/***************************************
*       Register Constants
***************************************/
#define USBFS_VDDD_MV                    CYDEV_VDDD_MV
#define USBFS_3500MV                     (3500u)

#define USBFS_CR1_REG_ENABLE             (0x01u)
#define USBFS_CR1_ENABLE_LOCK            (0x02u)
#define USBFS_CR1_BUS_ACTIVITY_SHIFT     (0x02u)
#define USBFS_CR1_BUS_ACTIVITY           ((uint8)(0x01u << USBFS_CR1_BUS_ACTIVITY_SHIFT))
#define USBFS_CR1_TRIM_MSB_EN            (0x08u)

#define USBFS_EP0_CNT_DATA_TOGGLE        (0x80u)
#define USBFS_EPX_CNT_DATA_TOGGLE        (0x80u)
#define USBFS_EPX_CNT0_MASK              (0x0Fu)
#define USBFS_EPX_CNTX_MSB_MASK          (0x07u)
#define USBFS_EPX_CNTX_ADDR_SHIFT        (0x04u)
#define USBFS_EPX_CNTX_ADDR_OFFSET       (0x10u)
#define USBFS_EPX_CNTX_CRC_COUNT         (0x02u)
#define USBFS_EPX_DATA_BUF_MAX           (512u)

#define USBFS_CR0_ENABLE                 (0x80u)

/* A 100 KHz clock is used for BUS reset count. Recommended is to count 10 pulses */
#define USBFS_BUS_RST_COUNT              (0x0au)

#define USBFS_USBIO_CR1_IOMODE           (0x20u)
#define USBFS_USBIO_CR1_USBPUEN          (0x04u)
#define USBFS_USBIO_CR1_DP0              (0x02u)
#define USBFS_USBIO_CR1_DM0              (0x01u)

#define USBFS_USBIO_CR0_TEN              (0x80u)
#define USBFS_USBIO_CR0_TSE0             (0x40u)
#define USBFS_USBIO_CR0_TD               (0x20u)
#define USBFS_USBIO_CR0_RD               (0x01u)

#define USBFS_FASTCLK_IMO_CR_USBCLK_ON   (0x40u)
#define USBFS_FASTCLK_IMO_CR_XCLKEN      (0x20u)
#define USBFS_FASTCLK_IMO_CR_FX2ON       (0x10u)

#define USBFS_ARB_EPX_CFG_RESET          (0x08u)
#define USBFS_ARB_EPX_CFG_CRC_BYPASS     (0x04u)
#define USBFS_ARB_EPX_CFG_DMA_REQ        (0x02u)
#define USBFS_ARB_EPX_CFG_IN_DATA_RDY    (0x01u)
#define USBFS_ARB_EPX_CFG_DEFAULT        (USBFS_ARB_EPX_CFG_RESET | \
                                                     USBFS_ARB_EPX_CFG_CRC_BYPASS)

#define USBFS_ARB_EPX_SR_IN_BUF_FULL     (0x01u)
#define USBFS_ARB_EPX_SR_DMA_GNT         (0x02u)
#define USBFS_ARB_EPX_SR_BUF_OVER        (0x04u)
#define USBFS_ARB_EPX_SR_BUF_UNDER       (0x08u)

#define USBFS_ARB_CFG_AUTO_MEM           (0x10u)
#define USBFS_ARB_CFG_MANUAL_DMA         (0x20u)
#define USBFS_ARB_CFG_AUTO_DMA           (0x40u)
#define USBFS_ARB_CFG_CFG_CPM            (0x80u)

#if(USBFS_EP_MM == USBFS__EP_DMAAUTO)
    #define USBFS_ARB_EPX_INT_MASK           (0x1Du)
#else
    #define USBFS_ARB_EPX_INT_MASK           (0x1Fu)
#endif /*  USBFS_EP_MM == USBFS__EP_DMAAUTO */
#define USBFS_ARB_INT_MASK       (uint8)((USBFS_DMA1_REMOVE ^ 1u) | \
                                            (uint8)((USBFS_DMA2_REMOVE ^ 1u) << 1u) | \
                                            (uint8)((USBFS_DMA3_REMOVE ^ 1u) << 2u) | \
                                            (uint8)((USBFS_DMA4_REMOVE ^ 1u) << 3u) | \
                                            (uint8)((USBFS_DMA5_REMOVE ^ 1u) << 4u) | \
                                            (uint8)((USBFS_DMA6_REMOVE ^ 1u) << 5u) | \
                                            (uint8)((USBFS_DMA7_REMOVE ^ 1u) << 6u) | \
                                            (uint8)((USBFS_DMA8_REMOVE ^ 1u) << 7u) )

#define USBFS_SIE_EP_INT_EP1_MASK        (0x01u)
#define USBFS_SIE_EP_INT_EP2_MASK        (0x02u)
#define USBFS_SIE_EP_INT_EP3_MASK        (0x04u)
#define USBFS_SIE_EP_INT_EP4_MASK        (0x08u)
#define USBFS_SIE_EP_INT_EP5_MASK        (0x10u)
#define USBFS_SIE_EP_INT_EP6_MASK        (0x20u)
#define USBFS_SIE_EP_INT_EP7_MASK        (0x40u)
#define USBFS_SIE_EP_INT_EP8_MASK        (0x80u)

#define USBFS_PM_ACT_EN_FSUSB            USBFS_USB__PM_ACT_MSK
#define USBFS_PM_STBY_EN_FSUSB           USBFS_USB__PM_STBY_MSK
#define USBFS_PM_AVAIL_EN_FSUSBIO        (0x10u)

#define USBFS_PM_USB_CR0_REF_EN          (0x01u)
#define USBFS_PM_USB_CR0_PD_N            (0x02u)
#define USBFS_PM_USB_CR0_PD_PULLUP_N     (0x04u)

#define USBFS_USB_CLK_ENABLE             (0x01u)

#define USBFS_DM_MASK                    USBFS_Dm__0__MASK
#define USBFS_DP_MASK                    USBFS_Dp__0__MASK

#define USBFS_DYN_RECONFIG_ENABLE        (0x01u)
#define USBFS_DYN_RECONFIG_EP_SHIFT      (0x01u)
#define USBFS_DYN_RECONFIG_RDY_STS       (0x10u)


#endif /*  CY_USBFS_USBFS_H */


/* [] END OF FILE */
