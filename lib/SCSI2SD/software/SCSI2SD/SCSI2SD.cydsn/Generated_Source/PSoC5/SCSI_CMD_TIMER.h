/*******************************************************************************
* File Name: SCSI_CMD_TIMER.h
* Version 2.50
*
*  Description:
*     Contains the function prototypes and constants available to the timer
*     user module.
*
*   Note:
*     None
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#if !defined(CY_Timer_v2_30_SCSI_CMD_TIMER_H)
#define CY_Timer_v2_30_SCSI_CMD_TIMER_H

#include "cytypes.h"
#include "cyfitter.h"
#include "CyLib.h" /* For CyEnterCriticalSection() and CyExitCriticalSection() functions */

extern uint8 SCSI_CMD_TIMER_initVar;

/* Check to see if required defines such as CY_PSOC5LP are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5LP)
    #error Component Timer_v2_50 requires cy_boot v3.0 or later
#endif /* (CY_ PSOC5LP) */


/**************************************
*           Parameter Defaults
**************************************/

#define SCSI_CMD_TIMER_Resolution                 16u
#define SCSI_CMD_TIMER_UsingFixedFunction         1u
#define SCSI_CMD_TIMER_UsingHWCaptureCounter      0u
#define SCSI_CMD_TIMER_SoftwareCaptureMode        0u
#define SCSI_CMD_TIMER_SoftwareTriggerMode        0u
#define SCSI_CMD_TIMER_UsingHWEnable              0u
#define SCSI_CMD_TIMER_EnableTriggerMode          0u
#define SCSI_CMD_TIMER_InterruptOnCaptureCount    0u
#define SCSI_CMD_TIMER_RunModeUsed                1u
#define SCSI_CMD_TIMER_ControlRegRemoved          0u


/***************************************
*       Type defines
***************************************/


/**************************************************************************
 * Sleep Wakeup Backup structure for Timer Component
 *************************************************************************/
typedef struct
{
    uint8 TimerEnableState;
    #if(!SCSI_CMD_TIMER_UsingFixedFunction)
        #if (CY_UDB_V0)
            uint16 TimerUdb;                 /* Timer internal counter value */
            uint16 TimerPeriod;              /* Timer Period value       */
            uint8 InterruptMaskValue;       /* Timer Compare Value */
            #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
                uint8 TimerCaptureCounter;  /* Timer Capture Counter Value */
            #endif /* variable declaration for backing up Capture Counter value*/
        #endif /* variables for non retention registers in CY_UDB_V0 */

        #if (CY_UDB_V1)
            uint16 TimerUdb;
            uint8 InterruptMaskValue;
            #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
                uint8 TimerCaptureCounter;
            #endif /* variable declarations for backing up non retention registers in CY_UDB_V1 */
        #endif /* (CY_UDB_V1) */

        #if (!SCSI_CMD_TIMER_ControlRegRemoved)
            uint8 TimerControlRegister;
        #endif /* variable declaration for backing up enable state of the Timer */
    #endif /* define backup variables only for UDB implementation. Fixed function registers are all retention */
}SCSI_CMD_TIMER_backupStruct;


/***************************************
*       Function Prototypes
***************************************/

void    SCSI_CMD_TIMER_Start(void) ;
void    SCSI_CMD_TIMER_Stop(void) ;

void    SCSI_CMD_TIMER_SetInterruptMode(uint8 interruptMode) ;
uint8   SCSI_CMD_TIMER_ReadStatusRegister(void) ;
/* Deprecated function. Do not use this in future. Retained for backward compatibility */
#define SCSI_CMD_TIMER_GetInterruptSource() SCSI_CMD_TIMER_ReadStatusRegister()

#if(!SCSI_CMD_TIMER_ControlRegRemoved)
    uint8   SCSI_CMD_TIMER_ReadControlRegister(void) ;
    void    SCSI_CMD_TIMER_WriteControlRegister(uint8 control) \
        ;
#endif /* (!SCSI_CMD_TIMER_ControlRegRemoved) */

uint16  SCSI_CMD_TIMER_ReadPeriod(void) ;
void    SCSI_CMD_TIMER_WritePeriod(uint16 period) \
    ;
uint16  SCSI_CMD_TIMER_ReadCounter(void) ;
void    SCSI_CMD_TIMER_WriteCounter(uint16 counter) \
    ;
uint16  SCSI_CMD_TIMER_ReadCapture(void) ;
void    SCSI_CMD_TIMER_SoftwareCapture(void) ;


#if(!SCSI_CMD_TIMER_UsingFixedFunction) /* UDB Prototypes */
    #if (SCSI_CMD_TIMER_SoftwareCaptureMode)
        void    SCSI_CMD_TIMER_SetCaptureMode(uint8 captureMode) ;
    #endif /* (!SCSI_CMD_TIMER_UsingFixedFunction) */

    #if (SCSI_CMD_TIMER_SoftwareTriggerMode)
        void    SCSI_CMD_TIMER_SetTriggerMode(uint8 triggerMode) ;
    #endif /* (SCSI_CMD_TIMER_SoftwareTriggerMode) */
    #if (SCSI_CMD_TIMER_EnableTriggerMode)
        void    SCSI_CMD_TIMER_EnableTrigger(void) ;
        void    SCSI_CMD_TIMER_DisableTrigger(void) ;
    #endif /* (SCSI_CMD_TIMER_EnableTriggerMode) */

    #if(SCSI_CMD_TIMER_InterruptOnCaptureCount)
        #if(!SCSI_CMD_TIMER_ControlRegRemoved)
            void    SCSI_CMD_TIMER_SetInterruptCount(uint8 interruptCount) \
                ;
        #endif /* (!SCSI_CMD_TIMER_ControlRegRemoved) */
    #endif /* (SCSI_CMD_TIMER_InterruptOnCaptureCount) */

    #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
        void    SCSI_CMD_TIMER_SetCaptureCount(uint8 captureCount) \
            ;
        uint8   SCSI_CMD_TIMER_ReadCaptureCount(void) ;
    #endif /* (SCSI_CMD_TIMER_UsingHWCaptureCounter) */

    void SCSI_CMD_TIMER_ClearFIFO(void) ;
#endif /* UDB Prototypes */

/* Sleep Retention APIs */
void SCSI_CMD_TIMER_Init(void)          ;
void SCSI_CMD_TIMER_Enable(void)        ;
void SCSI_CMD_TIMER_SaveConfig(void)    ;
void SCSI_CMD_TIMER_RestoreConfig(void) ;
void SCSI_CMD_TIMER_Sleep(void)         ;
void SCSI_CMD_TIMER_Wakeup(void)        ;


/***************************************
*   Enumerated Types and Parameters
***************************************/

/* Enumerated Type B_Timer__CaptureModes, Used in Capture Mode */
#define SCSI_CMD_TIMER__B_TIMER__CM_NONE 0
#define SCSI_CMD_TIMER__B_TIMER__CM_RISINGEDGE 1
#define SCSI_CMD_TIMER__B_TIMER__CM_FALLINGEDGE 2
#define SCSI_CMD_TIMER__B_TIMER__CM_EITHEREDGE 3
#define SCSI_CMD_TIMER__B_TIMER__CM_SOFTWARE 4



/* Enumerated Type B_Timer__TriggerModes, Used in Trigger Mode */
#define SCSI_CMD_TIMER__B_TIMER__TM_NONE 0x00u
#define SCSI_CMD_TIMER__B_TIMER__TM_RISINGEDGE 0x04u
#define SCSI_CMD_TIMER__B_TIMER__TM_FALLINGEDGE 0x08u
#define SCSI_CMD_TIMER__B_TIMER__TM_EITHEREDGE 0x0Cu
#define SCSI_CMD_TIMER__B_TIMER__TM_SOFTWARE 0x10u


/***************************************
*    Initialial Parameter Constants
***************************************/

#define SCSI_CMD_TIMER_INIT_PERIOD             1199u
#define SCSI_CMD_TIMER_INIT_CAPTURE_MODE       ((uint8)((uint8)0u << SCSI_CMD_TIMER_CTRL_CAP_MODE_SHIFT))
#define SCSI_CMD_TIMER_INIT_TRIGGER_MODE       ((uint8)((uint8)0u << SCSI_CMD_TIMER_CTRL_TRIG_MODE_SHIFT))
#if (SCSI_CMD_TIMER_UsingFixedFunction)
    #define SCSI_CMD_TIMER_INIT_INTERRUPT_MODE (((uint8)((uint8)0u << SCSI_CMD_TIMER_STATUS_TC_INT_MASK_SHIFT)) | \
                                                  ((uint8)((uint8)0 << SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK_SHIFT)))
#else
    #define SCSI_CMD_TIMER_INIT_INTERRUPT_MODE (((uint8)((uint8)0u << SCSI_CMD_TIMER_STATUS_TC_INT_MASK_SHIFT)) | \
                                                 ((uint8)((uint8)0 << SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK_SHIFT)) | \
                                                 ((uint8)((uint8)0 << SCSI_CMD_TIMER_STATUS_FIFOFULL_INT_MASK_SHIFT)))
#endif /* (SCSI_CMD_TIMER_UsingFixedFunction) */
#define SCSI_CMD_TIMER_INIT_CAPTURE_COUNT      (2u)
#define SCSI_CMD_TIMER_INIT_INT_CAPTURE_COUNT  ((uint8)((uint8)(1u - 1u) << SCSI_CMD_TIMER_CTRL_INTCNT_SHIFT))


/***************************************
*           Registers
***************************************/

#if (SCSI_CMD_TIMER_UsingFixedFunction) /* Implementation Specific Registers and Register Constants */


    /***************************************
    *    Fixed Function Registers
    ***************************************/

    #define SCSI_CMD_TIMER_STATUS         (*(reg8 *) SCSI_CMD_TIMER_TimerHW__SR0 )
    /* In Fixed Function Block Status and Mask are the same register */
    #define SCSI_CMD_TIMER_STATUS_MASK    (*(reg8 *) SCSI_CMD_TIMER_TimerHW__SR0 )
    #define SCSI_CMD_TIMER_CONTROL        (*(reg8 *) SCSI_CMD_TIMER_TimerHW__CFG0)
    #define SCSI_CMD_TIMER_CONTROL2       (*(reg8 *) SCSI_CMD_TIMER_TimerHW__CFG1)
    #define SCSI_CMD_TIMER_CONTROL2_PTR   ( (reg8 *) SCSI_CMD_TIMER_TimerHW__CFG1)
    #define SCSI_CMD_TIMER_RT1            (*(reg8 *) SCSI_CMD_TIMER_TimerHW__RT1)
    #define SCSI_CMD_TIMER_RT1_PTR        ( (reg8 *) SCSI_CMD_TIMER_TimerHW__RT1)

    #if (CY_PSOC3 || CY_PSOC5LP)
        #define SCSI_CMD_TIMER_CONTROL3       (*(reg8 *) SCSI_CMD_TIMER_TimerHW__CFG2)
        #define SCSI_CMD_TIMER_CONTROL3_PTR   ( (reg8 *) SCSI_CMD_TIMER_TimerHW__CFG2)
    #endif /* (CY_PSOC3 || CY_PSOC5LP) */
    #define SCSI_CMD_TIMER_GLOBAL_ENABLE  (*(reg8 *) SCSI_CMD_TIMER_TimerHW__PM_ACT_CFG)
    #define SCSI_CMD_TIMER_GLOBAL_STBY_ENABLE  (*(reg8 *) SCSI_CMD_TIMER_TimerHW__PM_STBY_CFG)

    #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg16 *) SCSI_CMD_TIMER_TimerHW__CAP0 )
    #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg16 *) SCSI_CMD_TIMER_TimerHW__CAP0 )
    #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg16 *) SCSI_CMD_TIMER_TimerHW__PER0 )
    #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg16 *) SCSI_CMD_TIMER_TimerHW__PER0 )
    #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg16 *) SCSI_CMD_TIMER_TimerHW__CNT_CMP0 )
    #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg16 *) SCSI_CMD_TIMER_TimerHW__CNT_CMP0 )


    /***************************************
    *    Register Constants
    ***************************************/

    /* Fixed Function Block Chosen */
    #define SCSI_CMD_TIMER_BLOCK_EN_MASK                     SCSI_CMD_TIMER_TimerHW__PM_ACT_MSK
    #define SCSI_CMD_TIMER_BLOCK_STBY_EN_MASK                SCSI_CMD_TIMER_TimerHW__PM_STBY_MSK

    /* Control Register Bit Locations */
    /* Interrupt Count - Not valid for Fixed Function Block */
    #define SCSI_CMD_TIMER_CTRL_INTCNT_SHIFT                  0x00u
    /* Trigger Polarity - Not valid for Fixed Function Block */
    #define SCSI_CMD_TIMER_CTRL_TRIG_MODE_SHIFT               0x00u
    /* Trigger Enable - Not valid for Fixed Function Block */
    #define SCSI_CMD_TIMER_CTRL_TRIG_EN_SHIFT                 0x00u
    /* Capture Polarity - Not valid for Fixed Function Block */
    #define SCSI_CMD_TIMER_CTRL_CAP_MODE_SHIFT                0x00u
    /* Timer Enable - As defined in Register Map, part of TMRX_CFG0 register */
    #define SCSI_CMD_TIMER_CTRL_ENABLE_SHIFT                  0x00u

    /* Control Register Bit Masks */
    #define SCSI_CMD_TIMER_CTRL_ENABLE                        ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_CTRL_ENABLE_SHIFT))

    /* Control2 Register Bit Masks */
    /* As defined in Register Map, Part of the TMRX_CFG1 register */
    #define SCSI_CMD_TIMER_CTRL2_IRQ_SEL_SHIFT                 0x00u
    #define SCSI_CMD_TIMER_CTRL2_IRQ_SEL                      ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_CTRL2_IRQ_SEL_SHIFT))

    #if (CY_PSOC5A)
        /* Use CFG1 Mode bits to set run mode */
        /* As defined by Verilog Implementation */
        #define SCSI_CMD_TIMER_CTRL_MODE_SHIFT                     0x01u
        #define SCSI_CMD_TIMER_CTRL_MODE_MASK                     ((uint8)((uint8)0x07u << SCSI_CMD_TIMER_CTRL_MODE_SHIFT))
    #endif /* (CY_PSOC5A) */
    #if (CY_PSOC3 || CY_PSOC5LP)
        /* Control3 Register Bit Locations */
        #define SCSI_CMD_TIMER_CTRL_RCOD_SHIFT        0x02u
        #define SCSI_CMD_TIMER_CTRL_ENBL_SHIFT        0x00u
        #define SCSI_CMD_TIMER_CTRL_MODE_SHIFT        0x00u

        /* Control3 Register Bit Masks */
        #define SCSI_CMD_TIMER_CTRL_RCOD_MASK  ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_CTRL_RCOD_SHIFT)) /* ROD and COD bit masks */
        #define SCSI_CMD_TIMER_CTRL_ENBL_MASK  ((uint8)((uint8)0x80u << SCSI_CMD_TIMER_CTRL_ENBL_SHIFT)) /* HW_EN bit mask */
        #define SCSI_CMD_TIMER_CTRL_MODE_MASK  ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_CTRL_MODE_SHIFT)) /* Run mode bit mask */

        #define SCSI_CMD_TIMER_CTRL_RCOD       ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_CTRL_RCOD_SHIFT))
        #define SCSI_CMD_TIMER_CTRL_ENBL       ((uint8)((uint8)0x80u << SCSI_CMD_TIMER_CTRL_ENBL_SHIFT))
    #endif /* (CY_PSOC3 || CY_PSOC5LP) */

    /*RT1 Synch Constants: Applicable for PSoC3 and PSoC5LP */
    #define SCSI_CMD_TIMER_RT1_SHIFT                       0x04u
    /* Sync TC and CMP bit masks */
    #define SCSI_CMD_TIMER_RT1_MASK                        ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_RT1_SHIFT))
    #define SCSI_CMD_TIMER_SYNC                            ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_RT1_SHIFT))
    #define SCSI_CMD_TIMER_SYNCDSI_SHIFT                   0x00u
    /* Sync all DSI inputs with Mask  */
    #define SCSI_CMD_TIMER_SYNCDSI_MASK                    ((uint8)((uint8)0x0Fu << SCSI_CMD_TIMER_SYNCDSI_SHIFT))
    /* Sync all DSI inputs */
    #define SCSI_CMD_TIMER_SYNCDSI_EN                      ((uint8)((uint8)0x0Fu << SCSI_CMD_TIMER_SYNCDSI_SHIFT))

    #define SCSI_CMD_TIMER_CTRL_MODE_PULSEWIDTH            ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_CTRL_MODE_SHIFT))
    #define SCSI_CMD_TIMER_CTRL_MODE_PERIOD                ((uint8)((uint8)0x02u << SCSI_CMD_TIMER_CTRL_MODE_SHIFT))
    #define SCSI_CMD_TIMER_CTRL_MODE_CONTINUOUS            ((uint8)((uint8)0x00u << SCSI_CMD_TIMER_CTRL_MODE_SHIFT))

    /* Status Register Bit Locations */
    /* As defined in Register Map, part of TMRX_SR0 register */
    #define SCSI_CMD_TIMER_STATUS_TC_SHIFT                 0x07u
    /* As defined in Register Map, part of TMRX_SR0 register, Shared with Compare Status */
    #define SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT            0x06u
    /* As defined in Register Map, part of TMRX_SR0 register */
    #define SCSI_CMD_TIMER_STATUS_TC_INT_MASK_SHIFT        (SCSI_CMD_TIMER_STATUS_TC_SHIFT - 0x04u)
    /* As defined in Register Map, part of TMRX_SR0 register, Shared with Compare Status */
    #define SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK_SHIFT   (SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT - 0x04u)

    /* Status Register Bit Masks */
    #define SCSI_CMD_TIMER_STATUS_TC                       ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_TC_SHIFT))
    #define SCSI_CMD_TIMER_STATUS_CAPTURE                  ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT))
    /* Interrupt Enable Bit-Mask for interrupt on TC */
    #define SCSI_CMD_TIMER_STATUS_TC_INT_MASK              ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_TC_INT_MASK_SHIFT))
    /* Interrupt Enable Bit-Mask for interrupt on Capture */
    #define SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK         ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK_SHIFT))

#else   /* UDB Registers and Register Constants */


    /***************************************
    *           UDB Registers
    ***************************************/

    #define SCSI_CMD_TIMER_STATUS              (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_rstSts_stsreg__STATUS_REG )
    #define SCSI_CMD_TIMER_STATUS_MASK         (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_rstSts_stsreg__MASK_REG)
    #define SCSI_CMD_TIMER_STATUS_AUX_CTRL     (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_rstSts_stsreg__STATUS_AUX_CTL_REG)
    #define SCSI_CMD_TIMER_CONTROL             (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_sCTRLReg_SyncCtl_ctrlreg__CONTROL_REG )
    
    #if(SCSI_CMD_TIMER_Resolution <= 8u) /* 8-bit Timer */
        #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
        #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg8 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
        #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
        #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg8 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
        #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg8 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
        #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg8 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
    #elif(SCSI_CMD_TIMER_Resolution <= 16u) /* 8-bit Timer */
        #if(CY_PSOC3) /* 8-bit addres space */
            #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
            #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
        #else /* 16-bit address space */
            #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__16BIT_F0_REG )
            #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__16BIT_F0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__16BIT_D0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__16BIT_D0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__16BIT_A0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg16 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__16BIT_A0_REG )
        #endif /* CY_PSOC3 */
    #elif(SCSI_CMD_TIMER_Resolution <= 24u)/* 24-bit Timer */
        #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
        #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
        #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
        #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
        #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
        #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
    #else /* 32-bit Timer */
        #if(CY_PSOC3 || CY_PSOC5) /* 8-bit address space */
            #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
            #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__F0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__D0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__A0_REG )
        #else /* 32-bit address space */
            #define SCSI_CMD_TIMER_CAPTURE_LSB         (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__32BIT_F0_REG )
            #define SCSI_CMD_TIMER_CAPTURE_LSB_PTR       ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__32BIT_F0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB          (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__32BIT_D0_REG )
            #define SCSI_CMD_TIMER_PERIOD_LSB_PTR        ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__32BIT_D0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB         (* (reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__32BIT_A0_REG )
            #define SCSI_CMD_TIMER_COUNTER_LSB_PTR       ((reg32 *) SCSI_CMD_TIMER_TimerUDB_sT16_timerdp_u0__32BIT_A0_REG )
        #endif /* CY_PSOC3 || CY_PSOC5 */ 
    #endif

    #if (SCSI_CMD_TIMER_UsingHWCaptureCounter)
        #define SCSI_CMD_TIMER_CAP_COUNT              (*(reg8 *) SCSI_CMD_TIMER_TimerUDB_sCapCount_counter__PERIOD_REG )
        #define SCSI_CMD_TIMER_CAP_COUNT_PTR          ( (reg8 *) SCSI_CMD_TIMER_TimerUDB_sCapCount_counter__PERIOD_REG )
        #define SCSI_CMD_TIMER_CAPTURE_COUNT_CTRL     (*(reg8 *) SCSI_CMD_TIMER_TimerUDB_sCapCount_counter__CONTROL_AUX_CTL_REG )
        #define SCSI_CMD_TIMER_CAPTURE_COUNT_CTRL_PTR ( (reg8 *) SCSI_CMD_TIMER_TimerUDB_sCapCount_counter__CONTROL_AUX_CTL_REG )
    #endif /* (SCSI_CMD_TIMER_UsingHWCaptureCounter) */


    /***************************************
    *       Register Constants
    ***************************************/

    /* Control Register Bit Locations */
    #define SCSI_CMD_TIMER_CTRL_INTCNT_SHIFT              0x00u       /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_CTRL_TRIG_MODE_SHIFT           0x02u       /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_CTRL_TRIG_EN_SHIFT             0x04u       /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_CTRL_CAP_MODE_SHIFT            0x05u       /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_CTRL_ENABLE_SHIFT              0x07u       /* As defined by Verilog Implementation */

    /* Control Register Bit Masks */
    #define SCSI_CMD_TIMER_CTRL_INTCNT_MASK               ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_CTRL_INTCNT_SHIFT))
    #define SCSI_CMD_TIMER_CTRL_TRIG_MODE_MASK            ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_CTRL_TRIG_MODE_SHIFT))
    #define SCSI_CMD_TIMER_CTRL_TRIG_EN                   ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_CTRL_TRIG_EN_SHIFT))
    #define SCSI_CMD_TIMER_CTRL_CAP_MODE_MASK             ((uint8)((uint8)0x03u << SCSI_CMD_TIMER_CTRL_CAP_MODE_SHIFT))
    #define SCSI_CMD_TIMER_CTRL_ENABLE                    ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_CTRL_ENABLE_SHIFT))

    /* Bit Counter (7-bit) Control Register Bit Definitions */
    /* As defined by the Register map for the AUX Control Register */
    #define SCSI_CMD_TIMER_CNTR_ENABLE                    0x20u

    /* Status Register Bit Locations */
    #define SCSI_CMD_TIMER_STATUS_TC_SHIFT                0x00u  /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT           0x01u  /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_STATUS_TC_INT_MASK_SHIFT       SCSI_CMD_TIMER_STATUS_TC_SHIFT
    #define SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK_SHIFT  SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT
    #define SCSI_CMD_TIMER_STATUS_FIFOFULL_SHIFT          0x02u  /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_STATUS_FIFONEMP_SHIFT          0x03u  /* As defined by Verilog Implementation */
    #define SCSI_CMD_TIMER_STATUS_FIFOFULL_INT_MASK_SHIFT SCSI_CMD_TIMER_STATUS_FIFOFULL_SHIFT

    /* Status Register Bit Masks */
    /* Sticky TC Event Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_TC                      ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_TC_SHIFT))
    /* Sticky Capture Event Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_CAPTURE                 ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT))
    /* Interrupt Enable Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_TC_INT_MASK             ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_TC_SHIFT))
    /* Interrupt Enable Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_CAPTURE_INT_MASK        ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_CAPTURE_SHIFT))
    /* NOT-Sticky FIFO Full Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_FIFOFULL                ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_FIFOFULL_SHIFT))
    /* NOT-Sticky FIFO Not Empty Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_FIFONEMP                ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_FIFONEMP_SHIFT))
    /* Interrupt Enable Bit-Mask */
    #define SCSI_CMD_TIMER_STATUS_FIFOFULL_INT_MASK       ((uint8)((uint8)0x01u << SCSI_CMD_TIMER_STATUS_FIFOFULL_SHIFT))

    #define SCSI_CMD_TIMER_STATUS_ACTL_INT_EN             0x10u   /* As defined for the ACTL Register */

    /* Datapath Auxillary Control Register definitions */
    #define SCSI_CMD_TIMER_AUX_CTRL_FIFO0_CLR             0x01u   /* As defined by Register map */
    #define SCSI_CMD_TIMER_AUX_CTRL_FIFO1_CLR             0x02u   /* As defined by Register map */
    #define SCSI_CMD_TIMER_AUX_CTRL_FIFO0_LVL             0x04u   /* As defined by Register map */
    #define SCSI_CMD_TIMER_AUX_CTRL_FIFO1_LVL             0x08u   /* As defined by Register map */
    #define SCSI_CMD_TIMER_STATUS_ACTL_INT_EN_MASK        0x10u   /* As defined for the ACTL Register */

#endif /* Implementation Specific Registers and Register Constants */

#endif  /* CY_Timer_v2_30_SCSI_CMD_TIMER_H */


/* [] END OF FILE */
