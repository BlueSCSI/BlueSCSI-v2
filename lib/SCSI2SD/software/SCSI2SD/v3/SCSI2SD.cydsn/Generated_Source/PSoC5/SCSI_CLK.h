/*******************************************************************************
* File Name: SCSI_CLK.h
* Version 2.10
*
*  Description:
*   Provides the function and constant definitions for the clock component.
*
*  Note:
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_CLOCK_SCSI_CLK_H)
#define CY_CLOCK_SCSI_CLK_H

#include <cytypes.h>
#include <cyfitter.h>


/***************************************
* Conditional Compilation Parameters
***************************************/

/* Check to see if required defines such as CY_PSOC5LP are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5LP)
    #error Component cy_clock_v2_10 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5LP) */


/***************************************
*        Function Prototypes
***************************************/

void SCSI_CLK_Start(void) ;
void SCSI_CLK_Stop(void) ;

#if(CY_PSOC3 || CY_PSOC5LP)
void SCSI_CLK_StopBlock(void) ;
#endif /* (CY_PSOC3 || CY_PSOC5LP) */

void SCSI_CLK_StandbyPower(uint8 state) ;
void SCSI_CLK_SetDividerRegister(uint16 clkDivider, uint8 restart) 
                                ;
uint16 SCSI_CLK_GetDividerRegister(void) ;
void SCSI_CLK_SetModeRegister(uint8 modeBitMask) ;
void SCSI_CLK_ClearModeRegister(uint8 modeBitMask) ;
uint8 SCSI_CLK_GetModeRegister(void) ;
void SCSI_CLK_SetSourceRegister(uint8 clkSource) ;
uint8 SCSI_CLK_GetSourceRegister(void) ;
#if defined(SCSI_CLK__CFG3)
void SCSI_CLK_SetPhaseRegister(uint8 clkPhase) ;
uint8 SCSI_CLK_GetPhaseRegister(void) ;
#endif /* defined(SCSI_CLK__CFG3) */

#define SCSI_CLK_Enable()                       SCSI_CLK_Start()
#define SCSI_CLK_Disable()                      SCSI_CLK_Stop()
#define SCSI_CLK_SetDivider(clkDivider)         SCSI_CLK_SetDividerRegister(clkDivider, 1u)
#define SCSI_CLK_SetDividerValue(clkDivider)    SCSI_CLK_SetDividerRegister((clkDivider) - 1u, 1u)
#define SCSI_CLK_SetMode(clkMode)               SCSI_CLK_SetModeRegister(clkMode)
#define SCSI_CLK_SetSource(clkSource)           SCSI_CLK_SetSourceRegister(clkSource)
#if defined(SCSI_CLK__CFG3)
#define SCSI_CLK_SetPhase(clkPhase)             SCSI_CLK_SetPhaseRegister(clkPhase)
#define SCSI_CLK_SetPhaseValue(clkPhase)        SCSI_CLK_SetPhaseRegister((clkPhase) + 1u)
#endif /* defined(SCSI_CLK__CFG3) */


/***************************************
*             Registers
***************************************/

/* Register to enable or disable the clock */
#define SCSI_CLK_CLKEN              (* (reg8 *) SCSI_CLK__PM_ACT_CFG)
#define SCSI_CLK_CLKEN_PTR          ((reg8 *) SCSI_CLK__PM_ACT_CFG)

/* Register to enable or disable the clock */
#define SCSI_CLK_CLKSTBY            (* (reg8 *) SCSI_CLK__PM_STBY_CFG)
#define SCSI_CLK_CLKSTBY_PTR        ((reg8 *) SCSI_CLK__PM_STBY_CFG)

/* Clock LSB divider configuration register. */
#define SCSI_CLK_DIV_LSB            (* (reg8 *) SCSI_CLK__CFG0)
#define SCSI_CLK_DIV_LSB_PTR        ((reg8 *) SCSI_CLK__CFG0)
#define SCSI_CLK_DIV_PTR            ((reg16 *) SCSI_CLK__CFG0)

/* Clock MSB divider configuration register. */
#define SCSI_CLK_DIV_MSB            (* (reg8 *) SCSI_CLK__CFG1)
#define SCSI_CLK_DIV_MSB_PTR        ((reg8 *) SCSI_CLK__CFG1)

/* Mode and source configuration register */
#define SCSI_CLK_MOD_SRC            (* (reg8 *) SCSI_CLK__CFG2)
#define SCSI_CLK_MOD_SRC_PTR        ((reg8 *) SCSI_CLK__CFG2)

#if defined(SCSI_CLK__CFG3)
/* Analog clock phase configuration register */
#define SCSI_CLK_PHASE              (* (reg8 *) SCSI_CLK__CFG3)
#define SCSI_CLK_PHASE_PTR          ((reg8 *) SCSI_CLK__CFG3)
#endif /* defined(SCSI_CLK__CFG3) */


/**************************************
*       Register Constants
**************************************/

/* Power manager register masks */
#define SCSI_CLK_CLKEN_MASK         SCSI_CLK__PM_ACT_MSK
#define SCSI_CLK_CLKSTBY_MASK       SCSI_CLK__PM_STBY_MSK

/* CFG2 field masks */
#define SCSI_CLK_SRC_SEL_MSK        SCSI_CLK__CFG2_SRC_SEL_MASK
#define SCSI_CLK_MODE_MASK          (~(SCSI_CLK_SRC_SEL_MSK))

#if defined(SCSI_CLK__CFG3)
/* CFG3 phase mask */
#define SCSI_CLK_PHASE_MASK         SCSI_CLK__CFG3_PHASE_DLY_MASK
#endif /* defined(SCSI_CLK__CFG3) */

#endif /* CY_CLOCK_SCSI_CLK_H */


/* [] END OF FILE */
