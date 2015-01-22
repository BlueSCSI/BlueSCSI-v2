/*******************************************************************************
* File Name: LED1.h  
* Version 2.10
*
* Description:
*  This file containts Control Register function prototypes and register defines
*
* Note:
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_PINS_LED1_H) /* Pins LED1_H */
#define CY_PINS_LED1_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "LED1_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v2_10 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 LED1__PORT == 15 && ((LED1__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    LED1_Write(uint8 value) ;
void    LED1_SetDriveMode(uint8 mode) ;
uint8   LED1_ReadDataReg(void) ;
uint8   LED1_Read(void) ;
uint8   LED1_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define LED1_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define LED1_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define LED1_DM_RES_UP          PIN_DM_RES_UP
#define LED1_DM_RES_DWN         PIN_DM_RES_DWN
#define LED1_DM_OD_LO           PIN_DM_OD_LO
#define LED1_DM_OD_HI           PIN_DM_OD_HI
#define LED1_DM_STRONG          PIN_DM_STRONG
#define LED1_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define LED1_MASK               LED1__MASK
#define LED1_SHIFT              LED1__SHIFT
#define LED1_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define LED1_PS                     (* (reg8 *) LED1__PS)
/* Data Register */
#define LED1_DR                     (* (reg8 *) LED1__DR)
/* Port Number */
#define LED1_PRT_NUM                (* (reg8 *) LED1__PRT) 
/* Connect to Analog Globals */                                                  
#define LED1_AG                     (* (reg8 *) LED1__AG)                       
/* Analog MUX bux enable */
#define LED1_AMUX                   (* (reg8 *) LED1__AMUX) 
/* Bidirectional Enable */                                                        
#define LED1_BIE                    (* (reg8 *) LED1__BIE)
/* Bit-mask for Aliased Register Access */
#define LED1_BIT_MASK               (* (reg8 *) LED1__BIT_MASK)
/* Bypass Enable */
#define LED1_BYP                    (* (reg8 *) LED1__BYP)
/* Port wide control signals */                                                   
#define LED1_CTL                    (* (reg8 *) LED1__CTL)
/* Drive Modes */
#define LED1_DM0                    (* (reg8 *) LED1__DM0) 
#define LED1_DM1                    (* (reg8 *) LED1__DM1)
#define LED1_DM2                    (* (reg8 *) LED1__DM2) 
/* Input Buffer Disable Override */
#define LED1_INP_DIS                (* (reg8 *) LED1__INP_DIS)
/* LCD Common or Segment Drive */
#define LED1_LCD_COM_SEG            (* (reg8 *) LED1__LCD_COM_SEG)
/* Enable Segment LCD */
#define LED1_LCD_EN                 (* (reg8 *) LED1__LCD_EN)
/* Slew Rate Control */
#define LED1_SLW                    (* (reg8 *) LED1__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define LED1_PRTDSI__CAPS_SEL       (* (reg8 *) LED1__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define LED1_PRTDSI__DBL_SYNC_IN    (* (reg8 *) LED1__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define LED1_PRTDSI__OE_SEL0        (* (reg8 *) LED1__PRTDSI__OE_SEL0) 
#define LED1_PRTDSI__OE_SEL1        (* (reg8 *) LED1__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define LED1_PRTDSI__OUT_SEL0       (* (reg8 *) LED1__PRTDSI__OUT_SEL0) 
#define LED1_PRTDSI__OUT_SEL1       (* (reg8 *) LED1__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define LED1_PRTDSI__SYNC_OUT       (* (reg8 *) LED1__PRTDSI__SYNC_OUT) 


#if defined(LED1__INTSTAT)  /* Interrupt Registers */

    #define LED1_INTSTAT                (* (reg8 *) LED1__INTSTAT)
    #define LED1_SNAP                   (* (reg8 *) LED1__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_LED1_H */


/* [] END OF FILE */
