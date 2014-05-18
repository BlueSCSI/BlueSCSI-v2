/*******************************************************************************
* File Name: SCSI_ATN.h  
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

#if !defined(CY_PINS_SCSI_ATN_H) /* Pins SCSI_ATN_H */
#define CY_PINS_SCSI_ATN_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "SCSI_ATN_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v1_90 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 SCSI_ATN__PORT == 15 && ((SCSI_ATN__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    SCSI_ATN_Write(uint8 value) ;
void    SCSI_ATN_SetDriveMode(uint8 mode) ;
uint8   SCSI_ATN_ReadDataReg(void) ;
uint8   SCSI_ATN_Read(void) ;
uint8   SCSI_ATN_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define SCSI_ATN_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define SCSI_ATN_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define SCSI_ATN_DM_RES_UP          PIN_DM_RES_UP
#define SCSI_ATN_DM_RES_DWN         PIN_DM_RES_DWN
#define SCSI_ATN_DM_OD_LO           PIN_DM_OD_LO
#define SCSI_ATN_DM_OD_HI           PIN_DM_OD_HI
#define SCSI_ATN_DM_STRONG          PIN_DM_STRONG
#define SCSI_ATN_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define SCSI_ATN_MASK               SCSI_ATN__MASK
#define SCSI_ATN_SHIFT              SCSI_ATN__SHIFT
#define SCSI_ATN_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define SCSI_ATN_PS                     (* (reg8 *) SCSI_ATN__PS)
/* Data Register */
#define SCSI_ATN_DR                     (* (reg8 *) SCSI_ATN__DR)
/* Port Number */
#define SCSI_ATN_PRT_NUM                (* (reg8 *) SCSI_ATN__PRT) 
/* Connect to Analog Globals */                                                  
#define SCSI_ATN_AG                     (* (reg8 *) SCSI_ATN__AG)                       
/* Analog MUX bux enable */
#define SCSI_ATN_AMUX                   (* (reg8 *) SCSI_ATN__AMUX) 
/* Bidirectional Enable */                                                        
#define SCSI_ATN_BIE                    (* (reg8 *) SCSI_ATN__BIE)
/* Bit-mask for Aliased Register Access */
#define SCSI_ATN_BIT_MASK               (* (reg8 *) SCSI_ATN__BIT_MASK)
/* Bypass Enable */
#define SCSI_ATN_BYP                    (* (reg8 *) SCSI_ATN__BYP)
/* Port wide control signals */                                                   
#define SCSI_ATN_CTL                    (* (reg8 *) SCSI_ATN__CTL)
/* Drive Modes */
#define SCSI_ATN_DM0                    (* (reg8 *) SCSI_ATN__DM0) 
#define SCSI_ATN_DM1                    (* (reg8 *) SCSI_ATN__DM1)
#define SCSI_ATN_DM2                    (* (reg8 *) SCSI_ATN__DM2) 
/* Input Buffer Disable Override */
#define SCSI_ATN_INP_DIS                (* (reg8 *) SCSI_ATN__INP_DIS)
/* LCD Common or Segment Drive */
#define SCSI_ATN_LCD_COM_SEG            (* (reg8 *) SCSI_ATN__LCD_COM_SEG)
/* Enable Segment LCD */
#define SCSI_ATN_LCD_EN                 (* (reg8 *) SCSI_ATN__LCD_EN)
/* Slew Rate Control */
#define SCSI_ATN_SLW                    (* (reg8 *) SCSI_ATN__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define SCSI_ATN_PRTDSI__CAPS_SEL       (* (reg8 *) SCSI_ATN__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define SCSI_ATN_PRTDSI__DBL_SYNC_IN    (* (reg8 *) SCSI_ATN__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define SCSI_ATN_PRTDSI__OE_SEL0        (* (reg8 *) SCSI_ATN__PRTDSI__OE_SEL0) 
#define SCSI_ATN_PRTDSI__OE_SEL1        (* (reg8 *) SCSI_ATN__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define SCSI_ATN_PRTDSI__OUT_SEL0       (* (reg8 *) SCSI_ATN__PRTDSI__OUT_SEL0) 
#define SCSI_ATN_PRTDSI__OUT_SEL1       (* (reg8 *) SCSI_ATN__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define SCSI_ATN_PRTDSI__SYNC_OUT       (* (reg8 *) SCSI_ATN__PRTDSI__SYNC_OUT) 


#if defined(SCSI_ATN__INTSTAT)  /* Interrupt Registers */

    #define SCSI_ATN_INTSTAT                (* (reg8 *) SCSI_ATN__INTSTAT)
    #define SCSI_ATN_SNAP                   (* (reg8 *) SCSI_ATN__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_SCSI_ATN_H */


/* [] END OF FILE */
