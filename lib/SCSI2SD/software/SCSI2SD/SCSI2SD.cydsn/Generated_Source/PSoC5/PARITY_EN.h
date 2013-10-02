/*******************************************************************************
* File Name: PARITY_EN.h  
* Version 1.90
*
* Description:
*  This file containts Control Register function prototypes and register defines
*
* Note:
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_PINS_PARITY_EN_H) /* Pins PARITY_EN_H */
#define CY_PINS_PARITY_EN_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "PARITY_EN_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v1_90 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 PARITY_EN__PORT == 15 && ((PARITY_EN__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    PARITY_EN_Write(uint8 value) ;
void    PARITY_EN_SetDriveMode(uint8 mode) ;
uint8   PARITY_EN_ReadDataReg(void) ;
uint8   PARITY_EN_Read(void) ;
uint8   PARITY_EN_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define PARITY_EN_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define PARITY_EN_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define PARITY_EN_DM_RES_UP          PIN_DM_RES_UP
#define PARITY_EN_DM_RES_DWN         PIN_DM_RES_DWN
#define PARITY_EN_DM_OD_LO           PIN_DM_OD_LO
#define PARITY_EN_DM_OD_HI           PIN_DM_OD_HI
#define PARITY_EN_DM_STRONG          PIN_DM_STRONG
#define PARITY_EN_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define PARITY_EN_MASK               PARITY_EN__MASK
#define PARITY_EN_SHIFT              PARITY_EN__SHIFT
#define PARITY_EN_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define PARITY_EN_PS                     (* (reg8 *) PARITY_EN__PS)
/* Data Register */
#define PARITY_EN_DR                     (* (reg8 *) PARITY_EN__DR)
/* Port Number */
#define PARITY_EN_PRT_NUM                (* (reg8 *) PARITY_EN__PRT) 
/* Connect to Analog Globals */                                                  
#define PARITY_EN_AG                     (* (reg8 *) PARITY_EN__AG)                       
/* Analog MUX bux enable */
#define PARITY_EN_AMUX                   (* (reg8 *) PARITY_EN__AMUX) 
/* Bidirectional Enable */                                                        
#define PARITY_EN_BIE                    (* (reg8 *) PARITY_EN__BIE)
/* Bit-mask for Aliased Register Access */
#define PARITY_EN_BIT_MASK               (* (reg8 *) PARITY_EN__BIT_MASK)
/* Bypass Enable */
#define PARITY_EN_BYP                    (* (reg8 *) PARITY_EN__BYP)
/* Port wide control signals */                                                   
#define PARITY_EN_CTL                    (* (reg8 *) PARITY_EN__CTL)
/* Drive Modes */
#define PARITY_EN_DM0                    (* (reg8 *) PARITY_EN__DM0) 
#define PARITY_EN_DM1                    (* (reg8 *) PARITY_EN__DM1)
#define PARITY_EN_DM2                    (* (reg8 *) PARITY_EN__DM2) 
/* Input Buffer Disable Override */
#define PARITY_EN_INP_DIS                (* (reg8 *) PARITY_EN__INP_DIS)
/* LCD Common or Segment Drive */
#define PARITY_EN_LCD_COM_SEG            (* (reg8 *) PARITY_EN__LCD_COM_SEG)
/* Enable Segment LCD */
#define PARITY_EN_LCD_EN                 (* (reg8 *) PARITY_EN__LCD_EN)
/* Slew Rate Control */
#define PARITY_EN_SLW                    (* (reg8 *) PARITY_EN__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define PARITY_EN_PRTDSI__CAPS_SEL       (* (reg8 *) PARITY_EN__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define PARITY_EN_PRTDSI__DBL_SYNC_IN    (* (reg8 *) PARITY_EN__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define PARITY_EN_PRTDSI__OE_SEL0        (* (reg8 *) PARITY_EN__PRTDSI__OE_SEL0) 
#define PARITY_EN_PRTDSI__OE_SEL1        (* (reg8 *) PARITY_EN__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define PARITY_EN_PRTDSI__OUT_SEL0       (* (reg8 *) PARITY_EN__PRTDSI__OUT_SEL0) 
#define PARITY_EN_PRTDSI__OUT_SEL1       (* (reg8 *) PARITY_EN__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define PARITY_EN_PRTDSI__SYNC_OUT       (* (reg8 *) PARITY_EN__PRTDSI__SYNC_OUT) 


#if defined(PARITY_EN__INTSTAT)  /* Interrupt Registers */

    #define PARITY_EN_INTSTAT                (* (reg8 *) PARITY_EN__INTSTAT)
    #define PARITY_EN_SNAP                   (* (reg8 *) PARITY_EN__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_PARITY_EN_H */


/* [] END OF FILE */
