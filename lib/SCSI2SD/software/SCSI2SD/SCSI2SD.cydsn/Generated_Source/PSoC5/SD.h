/*******************************************************************************
* File Name: SD.h
* Version 2.40
*
* Description:
*  Contains the function prototypes, constants and register definition
*  of the SPI Master Component.
*
* Note:
*  None
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_SPIM_SD_H)
#define CY_SPIM_SD_H

#include "cytypes.h"
#include "cyfitter.h"
#include "CyLib.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component SPI_Master_v2_40 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */


/***************************************
*   Conditional Compilation Parameters
***************************************/

#define SD_INTERNAL_CLOCK             (0u)

#if(0u != SD_INTERNAL_CLOCK)
    #include "SD_IntClock.h"
#endif /* (0u != SD_INTERNAL_CLOCK) */

#define SD_MODE                       (1u)
#define SD_DATA_WIDTH                 (8u)
#define SD_MODE_USE_ZERO              (1u)
#define SD_BIDIRECTIONAL_MODE         (0u)

/* Internal interrupt handling */
#define SD_TX_BUFFER_SIZE             (4u)
#define SD_RX_BUFFER_SIZE             (4u)
#define SD_INTERNAL_TX_INT_ENABLED    (0u)
#define SD_INTERNAL_RX_INT_ENABLED    (0u)

#define SD_SINGLE_REG_SIZE            (8u)
#define SD_USE_SECOND_DATAPATH        (SD_DATA_WIDTH > SD_SINGLE_REG_SIZE)

#define SD_FIFO_SIZE                  (4u)
#define SD_TX_SOFTWARE_BUF_ENABLED    ((0u != SD_INTERNAL_TX_INT_ENABLED) && \
                                                     (SD_TX_BUFFER_SIZE > SD_FIFO_SIZE))

#define SD_RX_SOFTWARE_BUF_ENABLED    ((0u != SD_INTERNAL_RX_INT_ENABLED) && \
                                                     (SD_RX_BUFFER_SIZE > SD_FIFO_SIZE))


/***************************************
*        Data Struct Definition
***************************************/

/* Sleep Mode API Support */
typedef struct
{
    uint8 enableState;
    uint8 cntrPeriod;
    #if(CY_UDB_V0)
        uint8 saveSrTxIntMask;
        uint8 saveSrRxIntMask;
    #endif /* (CY_UDB_V0) */

} SD_BACKUP_STRUCT;


/***************************************
*        Function Prototypes
***************************************/

void  SD_Init(void)                           ;
void  SD_Enable(void)                         ;
void  SD_Start(void)                          ;
void  SD_Stop(void)                           ;

void  SD_EnableTxInt(void)                    ;
void  SD_EnableRxInt(void)                    ;
void  SD_DisableTxInt(void)                   ;
void  SD_DisableRxInt(void)                   ;

void  SD_Sleep(void)                          ;
void  SD_Wakeup(void)                         ;
void  SD_SaveConfig(void)                     ;
void  SD_RestoreConfig(void)                  ;

void  SD_SetTxInterruptMode(uint8 intSrc)     ;
void  SD_SetRxInterruptMode(uint8 intSrc)     ;
uint8 SD_ReadTxStatus(void)                   ;
uint8 SD_ReadRxStatus(void)                   ;
void  SD_WriteTxData(uint8 txData)  \
                                                            ;
uint8 SD_ReadRxData(void) \
                                                            ;
uint8 SD_GetRxBufferSize(void)                ;
uint8 SD_GetTxBufferSize(void)                ;
void  SD_ClearRxBuffer(void)                  ;
void  SD_ClearTxBuffer(void)                  ;
void  SD_ClearFIFO(void)                              ;
void  SD_PutArray(const uint8 buffer[], uint8 byteCount) \
                                                            ;

#if(0u != SD_BIDIRECTIONAL_MODE)
    void  SD_TxEnable(void)                   ;
    void  SD_TxDisable(void)                  ;
#endif /* (0u != SD_BIDIRECTIONAL_MODE) */

CY_ISR_PROTO(SD_TX_ISR);
CY_ISR_PROTO(SD_RX_ISR);


/**********************************
*   Variable with external linkage
**********************************/

extern uint8 SD_initVar;


/***************************************
*           API Constants
***************************************/

#define SD_TX_ISR_NUMBER     ((uint8) (SD_TxInternalInterrupt__INTC_NUMBER))
#define SD_RX_ISR_NUMBER     ((uint8) (SD_RxInternalInterrupt__INTC_NUMBER))

#define SD_TX_ISR_PRIORITY   ((uint8) (SD_TxInternalInterrupt__INTC_PRIOR_NUM))
#define SD_RX_ISR_PRIORITY   ((uint8) (SD_RxInternalInterrupt__INTC_PRIOR_NUM))


/***************************************
*    Initial Parameter Constants
***************************************/

#define SD_INT_ON_SPI_DONE    ((uint8) (0u   << SD_STS_SPI_DONE_SHIFT))
#define SD_INT_ON_TX_EMPTY    ((uint8) (0u   << SD_STS_TX_FIFO_EMPTY_SHIFT))
#define SD_INT_ON_TX_NOT_FULL ((uint8) (0u << \
                                                                           SD_STS_TX_FIFO_NOT_FULL_SHIFT))
#define SD_INT_ON_BYTE_COMP   ((uint8) (0u  << SD_STS_BYTE_COMPLETE_SHIFT))
#define SD_INT_ON_SPI_IDLE    ((uint8) (0u   << SD_STS_SPI_IDLE_SHIFT))

/* Disable TX_NOT_FULL if software buffer is used */
#define SD_INT_ON_TX_NOT_FULL_DEF ((SD_TX_SOFTWARE_BUF_ENABLED) ? \
                                                                        (0u) : (SD_INT_ON_TX_NOT_FULL))

/* TX interrupt mask */
#define SD_TX_INIT_INTERRUPTS_MASK    (SD_INT_ON_SPI_DONE  | \
                                                     SD_INT_ON_TX_EMPTY  | \
                                                     SD_INT_ON_TX_NOT_FULL_DEF | \
                                                     SD_INT_ON_BYTE_COMP | \
                                                     SD_INT_ON_SPI_IDLE)

#define SD_INT_ON_RX_FULL         ((uint8) (0u << \
                                                                          SD_STS_RX_FIFO_FULL_SHIFT))
#define SD_INT_ON_RX_NOT_EMPTY    ((uint8) (0u << \
                                                                          SD_STS_RX_FIFO_NOT_EMPTY_SHIFT))
#define SD_INT_ON_RX_OVER         ((uint8) (0u << \
                                                                          SD_STS_RX_FIFO_OVERRUN_SHIFT))

/* RX interrupt mask */
#define SD_RX_INIT_INTERRUPTS_MASK    (SD_INT_ON_RX_FULL      | \
                                                     SD_INT_ON_RX_NOT_EMPTY | \
                                                     SD_INT_ON_RX_OVER)
/* Nubmer of bits to receive/transmit */
#define SD_BITCTR_INIT            (((uint8) (SD_DATA_WIDTH << 1u)) - 1u)


/***************************************
*             Registers
***************************************/

#if(CY_PSOC3 || CY_PSOC5)
    #define SD_TXDATA_REG (* (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F0_REG)
    #define SD_TXDATA_PTR (  (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F0_REG)
    #define SD_RXDATA_REG (* (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F1_REG)
    #define SD_RXDATA_PTR (  (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F1_REG)
#else   /* PSOC4 */
    #if(SD_USE_SECOND_DATAPATH)
        #define SD_TXDATA_REG (* (reg16 *) \
                                          SD_BSPIM_sR8_Dp_u0__16BIT_F0_REG)
        #define SD_TXDATA_PTR (  (reg16 *) \
                                          SD_BSPIM_sR8_Dp_u0__16BIT_F0_REG)
        #define SD_RXDATA_REG (* (reg16 *) \
                                          SD_BSPIM_sR8_Dp_u0__16BIT_F1_REG)
        #define SD_RXDATA_PTR         (  (reg16 *) \
                                          SD_BSPIM_sR8_Dp_u0__16BIT_F1_REG)
    #else
        #define SD_TXDATA_REG (* (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F0_REG)
        #define SD_TXDATA_PTR (  (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F0_REG)
        #define SD_RXDATA_REG (* (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F1_REG)
        #define SD_RXDATA_PTR (  (reg8 *) \
                                                SD_BSPIM_sR8_Dp_u0__F1_REG)
    #endif /* (SD_USE_SECOND_DATAPATH) */
#endif     /* (CY_PSOC3 || CY_PSOC5) */

#define SD_AUX_CONTROL_DP0_REG (* (reg8 *) \
                                        SD_BSPIM_sR8_Dp_u0__DP_AUX_CTL_REG)
#define SD_AUX_CONTROL_DP0_PTR (  (reg8 *) \
                                        SD_BSPIM_sR8_Dp_u0__DP_AUX_CTL_REG)

#if(SD_USE_SECOND_DATAPATH)
    #define SD_AUX_CONTROL_DP1_REG  (* (reg8 *) \
                                        SD_BSPIM_sR8_Dp_u1__DP_AUX_CTL_REG)
    #define SD_AUX_CONTROL_DP1_PTR  (  (reg8 *) \
                                        SD_BSPIM_sR8_Dp_u1__DP_AUX_CTL_REG)
#endif /* (SD_USE_SECOND_DATAPATH) */

#define SD_COUNTER_PERIOD_REG     (* (reg8 *) SD_BSPIM_BitCounter__PERIOD_REG)
#define SD_COUNTER_PERIOD_PTR     (  (reg8 *) SD_BSPIM_BitCounter__PERIOD_REG)
#define SD_COUNTER_CONTROL_REG    (* (reg8 *) SD_BSPIM_BitCounter__CONTROL_AUX_CTL_REG)
#define SD_COUNTER_CONTROL_PTR    (  (reg8 *) SD_BSPIM_BitCounter__CONTROL_AUX_CTL_REG)

#define SD_TX_STATUS_REG          (* (reg8 *) SD_BSPIM_TxStsReg__STATUS_REG)
#define SD_TX_STATUS_PTR          (  (reg8 *) SD_BSPIM_TxStsReg__STATUS_REG)
#define SD_RX_STATUS_REG          (* (reg8 *) SD_BSPIM_RxStsReg__STATUS_REG)
#define SD_RX_STATUS_PTR          (  (reg8 *) SD_BSPIM_RxStsReg__STATUS_REG)

#define SD_CONTROL_REG            (* (reg8 *) \
                                      SD_BSPIM_BidirMode_SyncCtl_CtrlReg__CONTROL_REG)
#define SD_CONTROL_PTR            (  (reg8 *) \
                                      SD_BSPIM_BidirMode_SyncCtl_CtrlReg__CONTROL_REG)

#define SD_TX_STATUS_MASK_REG     (* (reg8 *) SD_BSPIM_TxStsReg__MASK_REG)
#define SD_TX_STATUS_MASK_PTR     (  (reg8 *) SD_BSPIM_TxStsReg__MASK_REG)
#define SD_RX_STATUS_MASK_REG     (* (reg8 *) SD_BSPIM_RxStsReg__MASK_REG)
#define SD_RX_STATUS_MASK_PTR     (  (reg8 *) SD_BSPIM_RxStsReg__MASK_REG)

#define SD_TX_STATUS_ACTL_REG     (* (reg8 *) SD_BSPIM_TxStsReg__STATUS_AUX_CTL_REG)
#define SD_TX_STATUS_ACTL_PTR     (  (reg8 *) SD_BSPIM_TxStsReg__STATUS_AUX_CTL_REG)
#define SD_RX_STATUS_ACTL_REG     (* (reg8 *) SD_BSPIM_RxStsReg__STATUS_AUX_CTL_REG)
#define SD_RX_STATUS_ACTL_PTR     (  (reg8 *) SD_BSPIM_RxStsReg__STATUS_AUX_CTL_REG)

#if(SD_USE_SECOND_DATAPATH)
    #define SD_AUX_CONTROLDP1     (SD_AUX_CONTROL_DP1_REG)
#endif /* (SD_USE_SECOND_DATAPATH) */


/***************************************
*       Register Constants
***************************************/

/* Status Register Definitions */
#define SD_STS_SPI_DONE_SHIFT             (0x00u)
#define SD_STS_TX_FIFO_EMPTY_SHIFT        (0x01u)
#define SD_STS_TX_FIFO_NOT_FULL_SHIFT     (0x02u)
#define SD_STS_BYTE_COMPLETE_SHIFT        (0x03u)
#define SD_STS_SPI_IDLE_SHIFT             (0x04u)
#define SD_STS_RX_FIFO_FULL_SHIFT         (0x04u)
#define SD_STS_RX_FIFO_NOT_EMPTY_SHIFT    (0x05u)
#define SD_STS_RX_FIFO_OVERRUN_SHIFT      (0x06u)

#define SD_STS_SPI_DONE           ((uint8) (0x01u << SD_STS_SPI_DONE_SHIFT))
#define SD_STS_TX_FIFO_EMPTY      ((uint8) (0x01u << SD_STS_TX_FIFO_EMPTY_SHIFT))
#define SD_STS_TX_FIFO_NOT_FULL   ((uint8) (0x01u << SD_STS_TX_FIFO_NOT_FULL_SHIFT))
#define SD_STS_BYTE_COMPLETE      ((uint8) (0x01u << SD_STS_BYTE_COMPLETE_SHIFT))
#define SD_STS_SPI_IDLE           ((uint8) (0x01u << SD_STS_SPI_IDLE_SHIFT))
#define SD_STS_RX_FIFO_FULL       ((uint8) (0x01u << SD_STS_RX_FIFO_FULL_SHIFT))
#define SD_STS_RX_FIFO_NOT_EMPTY  ((uint8) (0x01u << SD_STS_RX_FIFO_NOT_EMPTY_SHIFT))
#define SD_STS_RX_FIFO_OVERRUN    ((uint8) (0x01u << SD_STS_RX_FIFO_OVERRUN_SHIFT))

/* TX and RX masks for clear on read bits */
#define SD_TX_STS_CLR_ON_RD_BYTES_MASK    (0x09u)
#define SD_RX_STS_CLR_ON_RD_BYTES_MASK    (0x40u)

/* StatusI Register Interrupt Enable Control Bits */
/* As defined by the Register map for the AUX Control Register */
#define SD_INT_ENABLE     (0x10u) /* Enable interrupt from statusi */
#define SD_TX_FIFO_CLR    (0x01u) /* F0 - TX FIFO */
#define SD_RX_FIFO_CLR    (0x02u) /* F1 - RX FIFO */
#define SD_FIFO_CLR       (SD_TX_FIFO_CLR | SD_RX_FIFO_CLR)

/* Bit Counter (7-bit) Control Register Bit Definitions */
/* As defined by the Register map for the AUX Control Register */
#define SD_CNTR_ENABLE    (0x20u) /* Enable CNT7 */

/* Bi-Directional mode control bit */
#define SD_CTRL_TX_SIGNAL_EN  (0x01u)

/* Datapath Auxillary Control Register definitions */
#define SD_AUX_CTRL_FIFO0_CLR         (0x01u)
#define SD_AUX_CTRL_FIFO1_CLR         (0x02u)
#define SD_AUX_CTRL_FIFO0_LVL         (0x04u)
#define SD_AUX_CTRL_FIFO1_LVL         (0x08u)
#define SD_STATUS_ACTL_INT_EN_MASK    (0x10u)

/* Component disabled */
#define SD_DISABLED   (0u)


/***************************************
*       Macros
***************************************/

/* Returns true if componentn enabled */
#define SD_IS_ENABLED (0u != (SD_TX_STATUS_ACTL_REG & SD_INT_ENABLE))

/* Retuns TX status register */
#define SD_GET_STATUS_TX(swTxSts) ( (uint8)(SD_TX_STATUS_REG | \
                                                          ((swTxSts) & SD_TX_STS_CLR_ON_RD_BYTES_MASK)) )
/* Retuns RX status register */
#define SD_GET_STATUS_RX(swRxSts) ( (uint8)(SD_RX_STATUS_REG | \
                                                          ((swRxSts) & SD_RX_STS_CLR_ON_RD_BYTES_MASK)) )


/***************************************
*       Obsolete definitions
***************************************/

/* Following definitions are for version compatibility.
*  They are obsolete in SPIM v2_30.
*  Please do not use it in new projects
*/

#define SD_WriteByte   SD_WriteTxData
#define SD_ReadByte    SD_ReadRxData
void  SD_SetInterruptMode(uint8 intSrc)       ;
uint8 SD_ReadStatus(void)                     ;
void  SD_EnableInt(void)                      ;
void  SD_DisableInt(void)                     ;

/* Obsolete register names. Not to be used in new designs */
#define SD_TXDATA                 (SD_TXDATA_REG)
#define SD_RXDATA                 (SD_RXDATA_REG)
#define SD_AUX_CONTROLDP0         (SD_AUX_CONTROL_DP0_REG)
#define SD_TXBUFFERREAD           (SD_txBufferRead)
#define SD_TXBUFFERWRITE          (SD_txBufferWrite)
#define SD_RXBUFFERREAD           (SD_rxBufferRead)
#define SD_RXBUFFERWRITE          (SD_rxBufferWrite)

#define SD_COUNTER_PERIOD         (SD_COUNTER_PERIOD_REG)
#define SD_COUNTER_CONTROL        (SD_COUNTER_CONTROL_REG)
#define SD_STATUS                 (SD_TX_STATUS_REG)
#define SD_CONTROL                (SD_CONTROL_REG)
#define SD_STATUS_MASK            (SD_TX_STATUS_MASK_REG)
#define SD_STATUS_ACTL            (SD_TX_STATUS_ACTL_REG)

#define SD_INIT_INTERRUPTS_MASK  (SD_INT_ON_SPI_DONE     | \
                                                SD_INT_ON_TX_EMPTY     | \
                                                SD_INT_ON_TX_NOT_FULL_DEF  | \
                                                SD_INT_ON_RX_FULL      | \
                                                SD_INT_ON_RX_NOT_EMPTY | \
                                                SD_INT_ON_RX_OVER      | \
                                                SD_INT_ON_BYTE_COMP)
                                                
/* Following definitions are for version Compatibility.
*  They are obsolete in SPIM v2_40.
*  Please do not use it in new projects
*/

#define SD_DataWidth                  (SD_DATA_WIDTH)
#define SD_InternalClockUsed          (SD_INTERNAL_CLOCK)
#define SD_InternalTxInterruptEnabled (SD_INTERNAL_TX_INT_ENABLED)
#define SD_InternalRxInterruptEnabled (SD_INTERNAL_RX_INT_ENABLED)
#define SD_ModeUseZero                (SD_MODE_USE_ZERO)
#define SD_BidirectionalMode          (SD_BIDIRECTIONAL_MODE)
#define SD_Mode                       (SD_MODE)
#define SD_DATAWIDHT                  (SD_DATA_WIDTH)
#define SD_InternalInterruptEnabled   (0u)

#define SD_TXBUFFERSIZE   (SD_TX_BUFFER_SIZE)
#define SD_RXBUFFERSIZE   (SD_RX_BUFFER_SIZE)

#define SD_TXBUFFER       SD_txBuffer
#define SD_RXBUFFER       SD_rxBuffer

#endif /* (CY_SPIM_SD_H) */


/* [] END OF FILE */
