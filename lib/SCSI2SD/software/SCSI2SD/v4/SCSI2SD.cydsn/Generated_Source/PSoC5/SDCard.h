/*******************************************************************************
* File Name: SDCard.h
* Version 2.50
*
* Description:
*  Contains the function prototypes, constants and register definition
*  of the SPI Master Component.
*
* Note:
*  None
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_SPIM_SDCard_H)
#define CY_SPIM_SDCard_H

#include "cytypes.h"
#include "cyfitter.h"
#include "CyLib.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component SPI_Master_v2_50 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */


/***************************************
*   Conditional Compilation Parameters
***************************************/

#define SDCard_INTERNAL_CLOCK             (0u)

#if(0u != SDCard_INTERNAL_CLOCK)
    #include "SDCard_IntClock.h"
#endif /* (0u != SDCard_INTERNAL_CLOCK) */

#define SDCard_MODE                       (1u)
#define SDCard_DATA_WIDTH                 (8u)
#define SDCard_MODE_USE_ZERO              (1u)
#define SDCard_BIDIRECTIONAL_MODE         (0u)

/* Internal interrupt handling */
#define SDCard_TX_BUFFER_SIZE             (4u)
#define SDCard_RX_BUFFER_SIZE             (4u)
#define SDCard_INTERNAL_TX_INT_ENABLED    (0u)
#define SDCard_INTERNAL_RX_INT_ENABLED    (0u)

#define SDCard_SINGLE_REG_SIZE            (8u)
#define SDCard_USE_SECOND_DATAPATH        (SDCard_DATA_WIDTH > SDCard_SINGLE_REG_SIZE)

#define SDCard_FIFO_SIZE                  (4u)
#define SDCard_TX_SOFTWARE_BUF_ENABLED    ((0u != SDCard_INTERNAL_TX_INT_ENABLED) && \
                                                     (SDCard_TX_BUFFER_SIZE > SDCard_FIFO_SIZE))

#define SDCard_RX_SOFTWARE_BUF_ENABLED    ((0u != SDCard_INTERNAL_RX_INT_ENABLED) && \
                                                     (SDCard_RX_BUFFER_SIZE > SDCard_FIFO_SIZE))


/***************************************
*        Data Struct Definition
***************************************/

/* Sleep Mode API Support */
typedef struct
{
    uint8 enableState;
    uint8 cntrPeriod;
} SDCard_BACKUP_STRUCT;


/***************************************
*        Function Prototypes
***************************************/

void  SDCard_Init(void)                           ;
void  SDCard_Enable(void)                         ;
void  SDCard_Start(void)                          ;
void  SDCard_Stop(void)                           ;

void  SDCard_EnableTxInt(void)                    ;
void  SDCard_EnableRxInt(void)                    ;
void  SDCard_DisableTxInt(void)                   ;
void  SDCard_DisableRxInt(void)                   ;

void  SDCard_Sleep(void)                          ;
void  SDCard_Wakeup(void)                         ;
void  SDCard_SaveConfig(void)                     ;
void  SDCard_RestoreConfig(void)                  ;

void  SDCard_SetTxInterruptMode(uint8 intSrc)     ;
void  SDCard_SetRxInterruptMode(uint8 intSrc)     ;
uint8 SDCard_ReadTxStatus(void)                   ;
uint8 SDCard_ReadRxStatus(void)                   ;
void  SDCard_WriteTxData(uint8 txData)  \
                                                            ;
uint8 SDCard_ReadRxData(void) \
                                                            ;
uint8 SDCard_GetRxBufferSize(void)                ;
uint8 SDCard_GetTxBufferSize(void)                ;
void  SDCard_ClearRxBuffer(void)                  ;
void  SDCard_ClearTxBuffer(void)                  ;
void  SDCard_ClearFIFO(void)                              ;
void  SDCard_PutArray(const uint8 buffer[], uint8 byteCount) \
                                                            ;

#if(0u != SDCard_BIDIRECTIONAL_MODE)
    void  SDCard_TxEnable(void)                   ;
    void  SDCard_TxDisable(void)                  ;
#endif /* (0u != SDCard_BIDIRECTIONAL_MODE) */

CY_ISR_PROTO(SDCard_TX_ISR);
CY_ISR_PROTO(SDCard_RX_ISR);


/***************************************
*   Variable with external linkage
***************************************/

extern uint8 SDCard_initVar;


/***************************************
*           API Constants
***************************************/

#define SDCard_TX_ISR_NUMBER     ((uint8) (SDCard_TxInternalInterrupt__INTC_NUMBER))
#define SDCard_RX_ISR_NUMBER     ((uint8) (SDCard_RxInternalInterrupt__INTC_NUMBER))

#define SDCard_TX_ISR_PRIORITY   ((uint8) (SDCard_TxInternalInterrupt__INTC_PRIOR_NUM))
#define SDCard_RX_ISR_PRIORITY   ((uint8) (SDCard_RxInternalInterrupt__INTC_PRIOR_NUM))


/***************************************
*    Initial Parameter Constants
***************************************/

#define SDCard_INT_ON_SPI_DONE    ((uint8) (0u   << SDCard_STS_SPI_DONE_SHIFT))
#define SDCard_INT_ON_TX_EMPTY    ((uint8) (1u   << SDCard_STS_TX_FIFO_EMPTY_SHIFT))
#define SDCard_INT_ON_TX_NOT_FULL ((uint8) (0u << \
                                                                           SDCard_STS_TX_FIFO_NOT_FULL_SHIFT))
#define SDCard_INT_ON_BYTE_COMP   ((uint8) (0u  << SDCard_STS_BYTE_COMPLETE_SHIFT))
#define SDCard_INT_ON_SPI_IDLE    ((uint8) (0u   << SDCard_STS_SPI_IDLE_SHIFT))

/* Disable TX_NOT_FULL if software buffer is used */
#define SDCard_INT_ON_TX_NOT_FULL_DEF ((SDCard_TX_SOFTWARE_BUF_ENABLED) ? \
                                                                        (0u) : (SDCard_INT_ON_TX_NOT_FULL))

/* TX interrupt mask */
#define SDCard_TX_INIT_INTERRUPTS_MASK    (SDCard_INT_ON_SPI_DONE  | \
                                                     SDCard_INT_ON_TX_EMPTY  | \
                                                     SDCard_INT_ON_TX_NOT_FULL_DEF | \
                                                     SDCard_INT_ON_BYTE_COMP | \
                                                     SDCard_INT_ON_SPI_IDLE)

#define SDCard_INT_ON_RX_FULL         ((uint8) (0u << \
                                                                          SDCard_STS_RX_FIFO_FULL_SHIFT))
#define SDCard_INT_ON_RX_NOT_EMPTY    ((uint8) (1u << \
                                                                          SDCard_STS_RX_FIFO_NOT_EMPTY_SHIFT))
#define SDCard_INT_ON_RX_OVER         ((uint8) (0u << \
                                                                          SDCard_STS_RX_FIFO_OVERRUN_SHIFT))

/* RX interrupt mask */
#define SDCard_RX_INIT_INTERRUPTS_MASK    (SDCard_INT_ON_RX_FULL      | \
                                                     SDCard_INT_ON_RX_NOT_EMPTY | \
                                                     SDCard_INT_ON_RX_OVER)
/* Nubmer of bits to receive/transmit */
#define SDCard_BITCTR_INIT            (((uint8) (SDCard_DATA_WIDTH << 1u)) - 1u)


/***************************************
*             Registers
***************************************/
#if(CY_PSOC3 || CY_PSOC5)
    #define SDCard_TXDATA_REG (* (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F0_REG)
    #define SDCard_TXDATA_PTR (  (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F0_REG)
    #define SDCard_RXDATA_REG (* (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F1_REG)
    #define SDCard_RXDATA_PTR (  (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F1_REG)
#else   /* PSOC4 */
    #if(SDCard_USE_SECOND_DATAPATH)
        #define SDCard_TXDATA_REG (* (reg16 *) \
                                          SDCard_BSPIM_sR8_Dp_u0__16BIT_F0_REG)
        #define SDCard_TXDATA_PTR (  (reg16 *) \
                                          SDCard_BSPIM_sR8_Dp_u0__16BIT_F0_REG)
        #define SDCard_RXDATA_REG (* (reg16 *) \
                                          SDCard_BSPIM_sR8_Dp_u0__16BIT_F1_REG)
        #define SDCard_RXDATA_PTR (  (reg16 *) \
                                          SDCard_BSPIM_sR8_Dp_u0__16BIT_F1_REG)
    #else
        #define SDCard_TXDATA_REG (* (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F0_REG)
        #define SDCard_TXDATA_PTR (  (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F0_REG)
        #define SDCard_RXDATA_REG (* (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F1_REG)
        #define SDCard_RXDATA_PTR (  (reg8 *) \
                                                SDCard_BSPIM_sR8_Dp_u0__F1_REG)
    #endif /* (SDCard_USE_SECOND_DATAPATH) */
#endif     /* (CY_PSOC3 || CY_PSOC5) */

#define SDCard_AUX_CONTROL_DP0_REG (* (reg8 *) \
                                        SDCard_BSPIM_sR8_Dp_u0__DP_AUX_CTL_REG)
#define SDCard_AUX_CONTROL_DP0_PTR (  (reg8 *) \
                                        SDCard_BSPIM_sR8_Dp_u0__DP_AUX_CTL_REG)

#if(SDCard_USE_SECOND_DATAPATH)
    #define SDCard_AUX_CONTROL_DP1_REG  (* (reg8 *) \
                                        SDCard_BSPIM_sR8_Dp_u1__DP_AUX_CTL_REG)
    #define SDCard_AUX_CONTROL_DP1_PTR  (  (reg8 *) \
                                        SDCard_BSPIM_sR8_Dp_u1__DP_AUX_CTL_REG)
#endif /* (SDCard_USE_SECOND_DATAPATH) */

#define SDCard_COUNTER_PERIOD_REG     (* (reg8 *) SDCard_BSPIM_BitCounter__PERIOD_REG)
#define SDCard_COUNTER_PERIOD_PTR     (  (reg8 *) SDCard_BSPIM_BitCounter__PERIOD_REG)
#define SDCard_COUNTER_CONTROL_REG    (* (reg8 *) SDCard_BSPIM_BitCounter__CONTROL_AUX_CTL_REG)
#define SDCard_COUNTER_CONTROL_PTR    (  (reg8 *) SDCard_BSPIM_BitCounter__CONTROL_AUX_CTL_REG)

#define SDCard_TX_STATUS_REG          (* (reg8 *) SDCard_BSPIM_TxStsReg__STATUS_REG)
#define SDCard_TX_STATUS_PTR          (  (reg8 *) SDCard_BSPIM_TxStsReg__STATUS_REG)
#define SDCard_RX_STATUS_REG          (* (reg8 *) SDCard_BSPIM_RxStsReg__STATUS_REG)
#define SDCard_RX_STATUS_PTR          (  (reg8 *) SDCard_BSPIM_RxStsReg__STATUS_REG)

#define SDCard_CONTROL_REG            (* (reg8 *) \
                                      SDCard_BSPIM_BidirMode_CtrlReg__CONTROL_REG)
#define SDCard_CONTROL_PTR            (  (reg8 *) \
                                      SDCard_BSPIM_BidirMode_CtrlReg__CONTROL_REG)

#define SDCard_TX_STATUS_MASK_REG     (* (reg8 *) SDCard_BSPIM_TxStsReg__MASK_REG)
#define SDCard_TX_STATUS_MASK_PTR     (  (reg8 *) SDCard_BSPIM_TxStsReg__MASK_REG)
#define SDCard_RX_STATUS_MASK_REG     (* (reg8 *) SDCard_BSPIM_RxStsReg__MASK_REG)
#define SDCard_RX_STATUS_MASK_PTR     (  (reg8 *) SDCard_BSPIM_RxStsReg__MASK_REG)

#define SDCard_TX_STATUS_ACTL_REG     (* (reg8 *) SDCard_BSPIM_TxStsReg__STATUS_AUX_CTL_REG)
#define SDCard_TX_STATUS_ACTL_PTR     (  (reg8 *) SDCard_BSPIM_TxStsReg__STATUS_AUX_CTL_REG)
#define SDCard_RX_STATUS_ACTL_REG     (* (reg8 *) SDCard_BSPIM_RxStsReg__STATUS_AUX_CTL_REG)
#define SDCard_RX_STATUS_ACTL_PTR     (  (reg8 *) SDCard_BSPIM_RxStsReg__STATUS_AUX_CTL_REG)

#if(SDCard_USE_SECOND_DATAPATH)
    #define SDCard_AUX_CONTROLDP1     (SDCard_AUX_CONTROL_DP1_REG)
#endif /* (SDCard_USE_SECOND_DATAPATH) */


/***************************************
*       Register Constants
***************************************/

/* Status Register Definitions */
#define SDCard_STS_SPI_DONE_SHIFT             (0x00u)
#define SDCard_STS_TX_FIFO_EMPTY_SHIFT        (0x01u)
#define SDCard_STS_TX_FIFO_NOT_FULL_SHIFT     (0x02u)
#define SDCard_STS_BYTE_COMPLETE_SHIFT        (0x03u)
#define SDCard_STS_SPI_IDLE_SHIFT             (0x04u)
#define SDCard_STS_RX_FIFO_FULL_SHIFT         (0x04u)
#define SDCard_STS_RX_FIFO_NOT_EMPTY_SHIFT    (0x05u)
#define SDCard_STS_RX_FIFO_OVERRUN_SHIFT      (0x06u)

#define SDCard_STS_SPI_DONE           ((uint8) (0x01u << SDCard_STS_SPI_DONE_SHIFT))
#define SDCard_STS_TX_FIFO_EMPTY      ((uint8) (0x01u << SDCard_STS_TX_FIFO_EMPTY_SHIFT))
#define SDCard_STS_TX_FIFO_NOT_FULL   ((uint8) (0x01u << SDCard_STS_TX_FIFO_NOT_FULL_SHIFT))
#define SDCard_STS_BYTE_COMPLETE      ((uint8) (0x01u << SDCard_STS_BYTE_COMPLETE_SHIFT))
#define SDCard_STS_SPI_IDLE           ((uint8) (0x01u << SDCard_STS_SPI_IDLE_SHIFT))
#define SDCard_STS_RX_FIFO_FULL       ((uint8) (0x01u << SDCard_STS_RX_FIFO_FULL_SHIFT))
#define SDCard_STS_RX_FIFO_NOT_EMPTY  ((uint8) (0x01u << SDCard_STS_RX_FIFO_NOT_EMPTY_SHIFT))
#define SDCard_STS_RX_FIFO_OVERRUN    ((uint8) (0x01u << SDCard_STS_RX_FIFO_OVERRUN_SHIFT))

/* TX and RX masks for clear on read bits */
#define SDCard_TX_STS_CLR_ON_RD_BYTES_MASK    (0x09u)
#define SDCard_RX_STS_CLR_ON_RD_BYTES_MASK    (0x40u)

/* StatusI Register Interrupt Enable Control Bits */
/* As defined by the Register map for the AUX Control Register */
#define SDCard_INT_ENABLE     (0x10u) /* Enable interrupt from statusi */
#define SDCard_TX_FIFO_CLR    (0x01u) /* F0 - TX FIFO */
#define SDCard_RX_FIFO_CLR    (0x02u) /* F1 - RX FIFO */
#define SDCard_FIFO_CLR       (SDCard_TX_FIFO_CLR | SDCard_RX_FIFO_CLR)

/* Bit Counter (7-bit) Control Register Bit Definitions */
/* As defined by the Register map for the AUX Control Register */
#define SDCard_CNTR_ENABLE    (0x20u) /* Enable CNT7 */

/* Bi-Directional mode control bit */
#define SDCard_CTRL_TX_SIGNAL_EN  (0x01u)

/* Datapath Auxillary Control Register definitions */
#define SDCard_AUX_CTRL_FIFO0_CLR         (0x01u)
#define SDCard_AUX_CTRL_FIFO1_CLR         (0x02u)
#define SDCard_AUX_CTRL_FIFO0_LVL         (0x04u)
#define SDCard_AUX_CTRL_FIFO1_LVL         (0x08u)
#define SDCard_STATUS_ACTL_INT_EN_MASK    (0x10u)

/* Component disabled */
#define SDCard_DISABLED   (0u)


/***************************************
*       Macros
***************************************/

/* Returns true if componentn enabled */
#define SDCard_IS_ENABLED (0u != (SDCard_TX_STATUS_ACTL_REG & SDCard_INT_ENABLE))

/* Retuns TX status register */
#define SDCard_GET_STATUS_TX(swTxSts) ( (uint8)(SDCard_TX_STATUS_REG | \
                                                          ((swTxSts) & SDCard_TX_STS_CLR_ON_RD_BYTES_MASK)) )
/* Retuns RX status register */
#define SDCard_GET_STATUS_RX(swRxSts) ( (uint8)(SDCard_RX_STATUS_REG | \
                                                          ((swRxSts) & SDCard_RX_STS_CLR_ON_RD_BYTES_MASK)) )


/***************************************
* The following code is DEPRECATED and 
* should not be used in new projects.
***************************************/

#define SDCard_WriteByte   SDCard_WriteTxData
#define SDCard_ReadByte    SDCard_ReadRxData
void  SDCard_SetInterruptMode(uint8 intSrc)       ;
uint8 SDCard_ReadStatus(void)                     ;
void  SDCard_EnableInt(void)                      ;
void  SDCard_DisableInt(void)                     ;

#define SDCard_TXDATA                 (SDCard_TXDATA_REG)
#define SDCard_RXDATA                 (SDCard_RXDATA_REG)
#define SDCard_AUX_CONTROLDP0         (SDCard_AUX_CONTROL_DP0_REG)
#define SDCard_TXBUFFERREAD           (SDCard_txBufferRead)
#define SDCard_TXBUFFERWRITE          (SDCard_txBufferWrite)
#define SDCard_RXBUFFERREAD           (SDCard_rxBufferRead)
#define SDCard_RXBUFFERWRITE          (SDCard_rxBufferWrite)

#define SDCard_COUNTER_PERIOD         (SDCard_COUNTER_PERIOD_REG)
#define SDCard_COUNTER_CONTROL        (SDCard_COUNTER_CONTROL_REG)
#define SDCard_STATUS                 (SDCard_TX_STATUS_REG)
#define SDCard_CONTROL                (SDCard_CONTROL_REG)
#define SDCard_STATUS_MASK            (SDCard_TX_STATUS_MASK_REG)
#define SDCard_STATUS_ACTL            (SDCard_TX_STATUS_ACTL_REG)

#define SDCard_INIT_INTERRUPTS_MASK  (SDCard_INT_ON_SPI_DONE     | \
                                                SDCard_INT_ON_TX_EMPTY     | \
                                                SDCard_INT_ON_TX_NOT_FULL_DEF  | \
                                                SDCard_INT_ON_RX_FULL      | \
                                                SDCard_INT_ON_RX_NOT_EMPTY | \
                                                SDCard_INT_ON_RX_OVER      | \
                                                SDCard_INT_ON_BYTE_COMP)
                                                
#define SDCard_DataWidth                  (SDCard_DATA_WIDTH)
#define SDCard_InternalClockUsed          (SDCard_INTERNAL_CLOCK)
#define SDCard_InternalTxInterruptEnabled (SDCard_INTERNAL_TX_INT_ENABLED)
#define SDCard_InternalRxInterruptEnabled (SDCard_INTERNAL_RX_INT_ENABLED)
#define SDCard_ModeUseZero                (SDCard_MODE_USE_ZERO)
#define SDCard_BidirectionalMode          (SDCard_BIDIRECTIONAL_MODE)
#define SDCard_Mode                       (SDCard_MODE)
#define SDCard_DATAWIDHT                  (SDCard_DATA_WIDTH)
#define SDCard_InternalInterruptEnabled   (0u)

#define SDCard_TXBUFFERSIZE   (SDCard_TX_BUFFER_SIZE)
#define SDCard_RXBUFFERSIZE   (SDCard_RX_BUFFER_SIZE)

#define SDCard_TXBUFFER       SDCard_txBuffer
#define SDCard_RXBUFFER       SDCard_rxBuffer

#endif /* (CY_SPIM_SDCard_H) */


/* [] END OF FILE */
