/*******************************************************************************
* File Name: SD_MOSI.h  
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

#if !defined(CY_PINS_SD_MOSI_H) /* Pins SD_MOSI_H */
#define CY_PINS_SD_MOSI_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "SD_MOSI_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v2_10 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 SD_MOSI__PORT == 15 && ((SD_MOSI__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    SD_MOSI_Write(uint8 value) ;
void    SD_MOSI_SetDriveMode(uint8 mode) ;
uint8   SD_MOSI_ReadDataReg(void) ;
uint8   SD_MOSI_Read(void) ;
uint8   SD_MOSI_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define SD_MOSI_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define SD_MOSI_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define SD_MOSI_DM_RES_UP          PIN_DM_RES_UP
#define SD_MOSI_DM_RES_DWN         PIN_DM_RES_DWN
#define SD_MOSI_DM_OD_LO           PIN_DM_OD_LO
#define SD_MOSI_DM_OD_HI           PIN_DM_OD_HI
#define SD_MOSI_DM_STRONG          PIN_DM_STRONG
#define SD_MOSI_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define SD_MOSI_MASK               SD_MOSI__MASK
#define SD_MOSI_SHIFT              SD_MOSI__SHIFT
#define SD_MOSI_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define SD_MOSI_PS                     (* (reg8 *) SD_MOSI__PS)
/* Data Register */
#define SD_MOSI_DR                     (* (reg8 *) SD_MOSI__DR)
/* Port Number */
#define SD_MOSI_PRT_NUM                (* (reg8 *) SD_MOSI__PRT) 
/* Connect to Analog Globals */                                                  
#define SD_MOSI_AG                     (* (reg8 *) SD_MOSI__AG)                       
/* Analog MUX bux enable */
#define SD_MOSI_AMUX                   (* (reg8 *) SD_MOSI__AMUX) 
/* Bidirectional Enable */                                                        
#define SD_MOSI_BIE                    (* (reg8 *) SD_MOSI__BIE)
/* Bit-mask for Aliased Register Access */
#define SD_MOSI_BIT_MASK               (* (reg8 *) SD_MOSI__BIT_MASK)
/* Bypass Enable */
#define SD_MOSI_BYP                    (* (reg8 *) SD_MOSI__BYP)
/* Port wide control signals */                                                   
#define SD_MOSI_CTL                    (* (reg8 *) SD_MOSI__CTL)
/* Drive Modes */
#define SD_MOSI_DM0                    (* (reg8 *) SD_MOSI__DM0) 
#define SD_MOSI_DM1                    (* (reg8 *) SD_MOSI__DM1)
#define SD_MOSI_DM2                    (* (reg8 *) SD_MOSI__DM2) 
/* Input Buffer Disable Override */
#define SD_MOSI_INP_DIS                (* (reg8 *) SD_MOSI__INP_DIS)
/* LCD Common or Segment Drive */
#define SD_MOSI_LCD_COM_SEG            (* (reg8 *) SD_MOSI__LCD_COM_SEG)
/* Enable Segment LCD */
#define SD_MOSI_LCD_EN                 (* (reg8 *) SD_MOSI__LCD_EN)
/* Slew Rate Control */
#define SD_MOSI_SLW                    (* (reg8 *) SD_MOSI__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define SD_MOSI_PRTDSI__CAPS_SEL       (* (reg8 *) SD_MOSI__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define SD_MOSI_PRTDSI__DBL_SYNC_IN    (* (reg8 *) SD_MOSI__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define SD_MOSI_PRTDSI__OE_SEL0        (* (reg8 *) SD_MOSI__PRTDSI__OE_SEL0) 
#define SD_MOSI_PRTDSI__OE_SEL1        (* (reg8 *) SD_MOSI__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define SD_MOSI_PRTDSI__OUT_SEL0       (* (reg8 *) SD_MOSI__PRTDSI__OUT_SEL0) 
#define SD_MOSI_PRTDSI__OUT_SEL1       (* (reg8 *) SD_MOSI__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define SD_MOSI_PRTDSI__SYNC_OUT       (* (reg8 *) SD_MOSI__PRTDSI__SYNC_OUT) 


#if defined(SD_MOSI__INTSTAT)  /* Interrupt Registers */

    #define SD_MOSI_INTSTAT                (* (reg8 *) SD_MOSI__INTSTAT)
    #define SD_MOSI_SNAP                   (* (reg8 *) SD_MOSI__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_SD_MOSI_H */


/* [] END OF FILE */
