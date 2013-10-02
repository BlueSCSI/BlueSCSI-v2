/*******************************************************************************
* File Name: SD_Init_Clk.h
* Version 2.0
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

#if !defined(CY_CLOCK_SD_Init_Clk_H)
#define CY_CLOCK_SD_Init_Clk_H

#include <cytypes.h>
#include <cyfitter.h>


/***************************************
* Conditional Compilation Parameters
***************************************/

/* Check to see if required defines such as CY_PSOC5LP are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5LP)
    #error Component cy_clock_v2_0 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5LP) */


/***************************************
*        Function Prototypes
***************************************/

void SD_Init_Clk_Start(void) ;
void SD_Init_Clk_Stop(void) ;

#if(CY_PSOC3 || CY_PSOC5LP)
void SD_Init_Clk_StopBlock(void) ;
#endif /* (CY_PSOC3 || CY_PSOC5LP) */

void SD_Init_Clk_StandbyPower(uint8 state) ;
void SD_Init_Clk_SetDividerRegister(uint16 clkDivider, uint8 restart) 
                                ;
uint16 SD_Init_Clk_GetDividerRegister(void) ;
void SD_Init_Clk_SetModeRegister(uint8 modeBitMask) ;
void SD_Init_Clk_ClearModeRegister(uint8 modeBitMask) ;
uint8 SD_Init_Clk_GetModeRegister(void) ;
void SD_Init_Clk_SetSourceRegister(uint8 clkSource) ;
uint8 SD_Init_Clk_GetSourceRegister(void) ;
#if defined(SD_Init_Clk__CFG3)
void SD_Init_Clk_SetPhaseRegister(uint8 clkPhase) ;
uint8 SD_Init_Clk_GetPhaseRegister(void) ;
#endif /* defined(SD_Init_Clk__CFG3) */

#define SD_Init_Clk_Enable()                       SD_Init_Clk_Start()
#define SD_Init_Clk_Disable()                      SD_Init_Clk_Stop()
#define SD_Init_Clk_SetDivider(clkDivider)         SD_Init_Clk_SetDividerRegister(clkDivider, 1)
#define SD_Init_Clk_SetDividerValue(clkDivider)    SD_Init_Clk_SetDividerRegister((clkDivider) - 1, 1)
#define SD_Init_Clk_SetMode(clkMode)               SD_Init_Clk_SetModeRegister(clkMode)
#define SD_Init_Clk_SetSource(clkSource)           SD_Init_Clk_SetSourceRegister(clkSource)
#if defined(SD_Init_Clk__CFG3)
#define SD_Init_Clk_SetPhase(clkPhase)             SD_Init_Clk_SetPhaseRegister(clkPhase)
#define SD_Init_Clk_SetPhaseValue(clkPhase)        SD_Init_Clk_SetPhaseRegister((clkPhase) + 1)
#endif /* defined(SD_Init_Clk__CFG3) */


/***************************************
*             Registers
***************************************/

/* Register to enable or disable the clock */
#define SD_Init_Clk_CLKEN              (* (reg8 *) SD_Init_Clk__PM_ACT_CFG)
#define SD_Init_Clk_CLKEN_PTR          ((reg8 *) SD_Init_Clk__PM_ACT_CFG)

/* Register to enable or disable the clock */
#define SD_Init_Clk_CLKSTBY            (* (reg8 *) SD_Init_Clk__PM_STBY_CFG)
#define SD_Init_Clk_CLKSTBY_PTR        ((reg8 *) SD_Init_Clk__PM_STBY_CFG)

/* Clock LSB divider configuration register. */
#define SD_Init_Clk_DIV_LSB            (* (reg8 *) SD_Init_Clk__CFG0)
#define SD_Init_Clk_DIV_LSB_PTR        ((reg8 *) SD_Init_Clk__CFG0)
#define SD_Init_Clk_DIV_PTR            ((reg16 *) SD_Init_Clk__CFG0)

/* Clock MSB divider configuration register. */
#define SD_Init_Clk_DIV_MSB            (* (reg8 *) SD_Init_Clk__CFG1)
#define SD_Init_Clk_DIV_MSB_PTR        ((reg8 *) SD_Init_Clk__CFG1)

/* Mode and source configuration register */
#define SD_Init_Clk_MOD_SRC            (* (reg8 *) SD_Init_Clk__CFG2)
#define SD_Init_Clk_MOD_SRC_PTR        ((reg8 *) SD_Init_Clk__CFG2)

#if defined(SD_Init_Clk__CFG3)
/* Analog clock phase configuration register */
#define SD_Init_Clk_PHASE              (* (reg8 *) SD_Init_Clk__CFG3)
#define SD_Init_Clk_PHASE_PTR          ((reg8 *) SD_Init_Clk__CFG3)
#endif /* defined(SD_Init_Clk__CFG3) */


/**************************************
*       Register Constants
**************************************/

/* Power manager register masks */
#define SD_Init_Clk_CLKEN_MASK         SD_Init_Clk__PM_ACT_MSK
#define SD_Init_Clk_CLKSTBY_MASK       SD_Init_Clk__PM_STBY_MSK

/* CFG2 field masks */
#define SD_Init_Clk_SRC_SEL_MSK        SD_Init_Clk__CFG2_SRC_SEL_MASK
#define SD_Init_Clk_MODE_MASK          (~(SD_Init_Clk_SRC_SEL_MSK))

#if defined(SD_Init_Clk__CFG3)
/* CFG3 phase mask */
#define SD_Init_Clk_PHASE_MASK         SD_Init_Clk__CFG3_PHASE_DLY_MASK
#endif /* defined(SD_Init_Clk__CFG3) */

#endif /* CY_CLOCK_SD_Init_Clk_H */


/* [] END OF FILE */
