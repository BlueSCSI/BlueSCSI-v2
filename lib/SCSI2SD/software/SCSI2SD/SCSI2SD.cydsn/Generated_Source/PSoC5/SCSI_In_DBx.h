/*******************************************************************************
* File Name: SCSI_In_DBx.h  
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

#if !defined(CY_PINS_SCSI_In_DBx_H) /* Pins SCSI_In_DBx_H */
#define CY_PINS_SCSI_In_DBx_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "SCSI_In_DBx_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v1_90 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 SCSI_In_DBx__PORT == 15 && ((SCSI_In_DBx__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    SCSI_In_DBx_Write(uint8 value) ;
void    SCSI_In_DBx_SetDriveMode(uint8 mode) ;
uint8   SCSI_In_DBx_ReadDataReg(void) ;
uint8   SCSI_In_DBx_Read(void) ;
uint8   SCSI_In_DBx_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define SCSI_In_DBx_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define SCSI_In_DBx_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define SCSI_In_DBx_DM_RES_UP          PIN_DM_RES_UP
#define SCSI_In_DBx_DM_RES_DWN         PIN_DM_RES_DWN
#define SCSI_In_DBx_DM_OD_LO           PIN_DM_OD_LO
#define SCSI_In_DBx_DM_OD_HI           PIN_DM_OD_HI
#define SCSI_In_DBx_DM_STRONG          PIN_DM_STRONG
#define SCSI_In_DBx_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define SCSI_In_DBx_MASK               SCSI_In_DBx__MASK
#define SCSI_In_DBx_SHIFT              SCSI_In_DBx__SHIFT
#define SCSI_In_DBx_WIDTH              8u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define SCSI_In_DBx_PS                     (* (reg8 *) SCSI_In_DBx__PS)
/* Data Register */
#define SCSI_In_DBx_DR                     (* (reg8 *) SCSI_In_DBx__DR)
/* Port Number */
#define SCSI_In_DBx_PRT_NUM                (* (reg8 *) SCSI_In_DBx__PRT) 
/* Connect to Analog Globals */                                                  
#define SCSI_In_DBx_AG                     (* (reg8 *) SCSI_In_DBx__AG)                       
/* Analog MUX bux enable */
#define SCSI_In_DBx_AMUX                   (* (reg8 *) SCSI_In_DBx__AMUX) 
/* Bidirectional Enable */                                                        
#define SCSI_In_DBx_BIE                    (* (reg8 *) SCSI_In_DBx__BIE)
/* Bit-mask for Aliased Register Access */
#define SCSI_In_DBx_BIT_MASK               (* (reg8 *) SCSI_In_DBx__BIT_MASK)
/* Bypass Enable */
#define SCSI_In_DBx_BYP                    (* (reg8 *) SCSI_In_DBx__BYP)
/* Port wide control signals */                                                   
#define SCSI_In_DBx_CTL                    (* (reg8 *) SCSI_In_DBx__CTL)
/* Drive Modes */
#define SCSI_In_DBx_DM0                    (* (reg8 *) SCSI_In_DBx__DM0) 
#define SCSI_In_DBx_DM1                    (* (reg8 *) SCSI_In_DBx__DM1)
#define SCSI_In_DBx_DM2                    (* (reg8 *) SCSI_In_DBx__DM2) 
/* Input Buffer Disable Override */
#define SCSI_In_DBx_INP_DIS                (* (reg8 *) SCSI_In_DBx__INP_DIS)
/* LCD Common or Segment Drive */
#define SCSI_In_DBx_LCD_COM_SEG            (* (reg8 *) SCSI_In_DBx__LCD_COM_SEG)
/* Enable Segment LCD */
#define SCSI_In_DBx_LCD_EN                 (* (reg8 *) SCSI_In_DBx__LCD_EN)
/* Slew Rate Control */
#define SCSI_In_DBx_SLW                    (* (reg8 *) SCSI_In_DBx__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define SCSI_In_DBx_PRTDSI__CAPS_SEL       (* (reg8 *) SCSI_In_DBx__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define SCSI_In_DBx_PRTDSI__DBL_SYNC_IN    (* (reg8 *) SCSI_In_DBx__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define SCSI_In_DBx_PRTDSI__OE_SEL0        (* (reg8 *) SCSI_In_DBx__PRTDSI__OE_SEL0) 
#define SCSI_In_DBx_PRTDSI__OE_SEL1        (* (reg8 *) SCSI_In_DBx__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define SCSI_In_DBx_PRTDSI__OUT_SEL0       (* (reg8 *) SCSI_In_DBx__PRTDSI__OUT_SEL0) 
#define SCSI_In_DBx_PRTDSI__OUT_SEL1       (* (reg8 *) SCSI_In_DBx__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define SCSI_In_DBx_PRTDSI__SYNC_OUT       (* (reg8 *) SCSI_In_DBx__PRTDSI__SYNC_OUT) 


#if defined(SCSI_In_DBx__INTSTAT)  /* Interrupt Registers */

    #define SCSI_In_DBx_INTSTAT                (* (reg8 *) SCSI_In_DBx__INTSTAT)
    #define SCSI_In_DBx_SNAP                   (* (reg8 *) SCSI_In_DBx__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_SCSI_In_DBx_H */


/* [] END OF FILE */
