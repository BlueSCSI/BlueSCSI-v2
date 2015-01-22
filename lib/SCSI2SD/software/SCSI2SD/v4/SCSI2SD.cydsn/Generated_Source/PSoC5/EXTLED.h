/*******************************************************************************
* File Name: EXTLED.h  
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

#if !defined(CY_PINS_EXTLED_H) /* Pins EXTLED_H */
#define CY_PINS_EXTLED_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "EXTLED_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v2_10 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 EXTLED__PORT == 15 && ((EXTLED__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    EXTLED_Write(uint8 value) ;
void    EXTLED_SetDriveMode(uint8 mode) ;
uint8   EXTLED_ReadDataReg(void) ;
uint8   EXTLED_Read(void) ;
uint8   EXTLED_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define EXTLED_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define EXTLED_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define EXTLED_DM_RES_UP          PIN_DM_RES_UP
#define EXTLED_DM_RES_DWN         PIN_DM_RES_DWN
#define EXTLED_DM_OD_LO           PIN_DM_OD_LO
#define EXTLED_DM_OD_HI           PIN_DM_OD_HI
#define EXTLED_DM_STRONG          PIN_DM_STRONG
#define EXTLED_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define EXTLED_MASK               EXTLED__MASK
#define EXTLED_SHIFT              EXTLED__SHIFT
#define EXTLED_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define EXTLED_PS                     (* (reg8 *) EXTLED__PS)
/* Data Register */
#define EXTLED_DR                     (* (reg8 *) EXTLED__DR)
/* Port Number */
#define EXTLED_PRT_NUM                (* (reg8 *) EXTLED__PRT) 
/* Connect to Analog Globals */                                                  
#define EXTLED_AG                     (* (reg8 *) EXTLED__AG)                       
/* Analog MUX bux enable */
#define EXTLED_AMUX                   (* (reg8 *) EXTLED__AMUX) 
/* Bidirectional Enable */                                                        
#define EXTLED_BIE                    (* (reg8 *) EXTLED__BIE)
/* Bit-mask for Aliased Register Access */
#define EXTLED_BIT_MASK               (* (reg8 *) EXTLED__BIT_MASK)
/* Bypass Enable */
#define EXTLED_BYP                    (* (reg8 *) EXTLED__BYP)
/* Port wide control signals */                                                   
#define EXTLED_CTL                    (* (reg8 *) EXTLED__CTL)
/* Drive Modes */
#define EXTLED_DM0                    (* (reg8 *) EXTLED__DM0) 
#define EXTLED_DM1                    (* (reg8 *) EXTLED__DM1)
#define EXTLED_DM2                    (* (reg8 *) EXTLED__DM2) 
/* Input Buffer Disable Override */
#define EXTLED_INP_DIS                (* (reg8 *) EXTLED__INP_DIS)
/* LCD Common or Segment Drive */
#define EXTLED_LCD_COM_SEG            (* (reg8 *) EXTLED__LCD_COM_SEG)
/* Enable Segment LCD */
#define EXTLED_LCD_EN                 (* (reg8 *) EXTLED__LCD_EN)
/* Slew Rate Control */
#define EXTLED_SLW                    (* (reg8 *) EXTLED__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define EXTLED_PRTDSI__CAPS_SEL       (* (reg8 *) EXTLED__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define EXTLED_PRTDSI__DBL_SYNC_IN    (* (reg8 *) EXTLED__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define EXTLED_PRTDSI__OE_SEL0        (* (reg8 *) EXTLED__PRTDSI__OE_SEL0) 
#define EXTLED_PRTDSI__OE_SEL1        (* (reg8 *) EXTLED__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define EXTLED_PRTDSI__OUT_SEL0       (* (reg8 *) EXTLED__PRTDSI__OUT_SEL0) 
#define EXTLED_PRTDSI__OUT_SEL1       (* (reg8 *) EXTLED__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define EXTLED_PRTDSI__SYNC_OUT       (* (reg8 *) EXTLED__PRTDSI__SYNC_OUT) 


#if defined(EXTLED__INTSTAT)  /* Interrupt Registers */

    #define EXTLED_INTSTAT                (* (reg8 *) EXTLED__INTSTAT)
    #define EXTLED_SNAP                   (* (reg8 *) EXTLED__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_EXTLED_H */


/* [] END OF FILE */
