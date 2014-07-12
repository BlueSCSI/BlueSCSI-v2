/*******************************************************************************
* File Name: SCSI_RST.h  
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

#if !defined(CY_PINS_SCSI_RST_H) /* Pins SCSI_RST_H */
#define CY_PINS_SCSI_RST_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "SCSI_RST_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v1_90 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 SCSI_RST__PORT == 15 && ((SCSI_RST__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    SCSI_RST_Write(uint8 value) ;
void    SCSI_RST_SetDriveMode(uint8 mode) ;
uint8   SCSI_RST_ReadDataReg(void) ;
uint8   SCSI_RST_Read(void) ;
uint8   SCSI_RST_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define SCSI_RST_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define SCSI_RST_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define SCSI_RST_DM_RES_UP          PIN_DM_RES_UP
#define SCSI_RST_DM_RES_DWN         PIN_DM_RES_DWN
#define SCSI_RST_DM_OD_LO           PIN_DM_OD_LO
#define SCSI_RST_DM_OD_HI           PIN_DM_OD_HI
#define SCSI_RST_DM_STRONG          PIN_DM_STRONG
#define SCSI_RST_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define SCSI_RST_MASK               SCSI_RST__MASK
#define SCSI_RST_SHIFT              SCSI_RST__SHIFT
#define SCSI_RST_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define SCSI_RST_PS                     (* (reg8 *) SCSI_RST__PS)
/* Data Register */
#define SCSI_RST_DR                     (* (reg8 *) SCSI_RST__DR)
/* Port Number */
#define SCSI_RST_PRT_NUM                (* (reg8 *) SCSI_RST__PRT) 
/* Connect to Analog Globals */                                                  
#define SCSI_RST_AG                     (* (reg8 *) SCSI_RST__AG)                       
/* Analog MUX bux enable */
#define SCSI_RST_AMUX                   (* (reg8 *) SCSI_RST__AMUX) 
/* Bidirectional Enable */                                                        
#define SCSI_RST_BIE                    (* (reg8 *) SCSI_RST__BIE)
/* Bit-mask for Aliased Register Access */
#define SCSI_RST_BIT_MASK               (* (reg8 *) SCSI_RST__BIT_MASK)
/* Bypass Enable */
#define SCSI_RST_BYP                    (* (reg8 *) SCSI_RST__BYP)
/* Port wide control signals */                                                   
#define SCSI_RST_CTL                    (* (reg8 *) SCSI_RST__CTL)
/* Drive Modes */
#define SCSI_RST_DM0                    (* (reg8 *) SCSI_RST__DM0) 
#define SCSI_RST_DM1                    (* (reg8 *) SCSI_RST__DM1)
#define SCSI_RST_DM2                    (* (reg8 *) SCSI_RST__DM2) 
/* Input Buffer Disable Override */
#define SCSI_RST_INP_DIS                (* (reg8 *) SCSI_RST__INP_DIS)
/* LCD Common or Segment Drive */
#define SCSI_RST_LCD_COM_SEG            (* (reg8 *) SCSI_RST__LCD_COM_SEG)
/* Enable Segment LCD */
#define SCSI_RST_LCD_EN                 (* (reg8 *) SCSI_RST__LCD_EN)
/* Slew Rate Control */
#define SCSI_RST_SLW                    (* (reg8 *) SCSI_RST__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define SCSI_RST_PRTDSI__CAPS_SEL       (* (reg8 *) SCSI_RST__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define SCSI_RST_PRTDSI__DBL_SYNC_IN    (* (reg8 *) SCSI_RST__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define SCSI_RST_PRTDSI__OE_SEL0        (* (reg8 *) SCSI_RST__PRTDSI__OE_SEL0) 
#define SCSI_RST_PRTDSI__OE_SEL1        (* (reg8 *) SCSI_RST__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define SCSI_RST_PRTDSI__OUT_SEL0       (* (reg8 *) SCSI_RST__PRTDSI__OUT_SEL0) 
#define SCSI_RST_PRTDSI__OUT_SEL1       (* (reg8 *) SCSI_RST__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define SCSI_RST_PRTDSI__SYNC_OUT       (* (reg8 *) SCSI_RST__PRTDSI__SYNC_OUT) 


#if defined(SCSI_RST__INTSTAT)  /* Interrupt Registers */

    #define SCSI_RST_INTSTAT                (* (reg8 *) SCSI_RST__INTSTAT)
    #define SCSI_RST_SNAP                   (* (reg8 *) SCSI_RST__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_SCSI_RST_H */


/* [] END OF FILE */
