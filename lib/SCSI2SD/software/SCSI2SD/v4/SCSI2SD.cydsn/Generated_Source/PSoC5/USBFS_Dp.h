/*******************************************************************************
* File Name: USBFS_Dp.h  
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

#if !defined(CY_PINS_USBFS_Dp_H) /* Pins USBFS_Dp_H */
#define CY_PINS_USBFS_Dp_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"
#include "USBFS_Dp_aliases.h"

/* Check to see if required defines such as CY_PSOC5A are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5A)
    #error Component cy_pins_v2_10 requires cy_boot v3.0 or later
#endif /* (CY_PSOC5A) */

/* APIs are not generated for P15[7:6] */
#if !(CY_PSOC5A &&\
	 USBFS_Dp__PORT == 15 && ((USBFS_Dp__MASK & 0xC0) != 0))


/***************************************
*        Function Prototypes             
***************************************/    

void    USBFS_Dp_Write(uint8 value) ;
void    USBFS_Dp_SetDriveMode(uint8 mode) ;
uint8   USBFS_Dp_ReadDataReg(void) ;
uint8   USBFS_Dp_Read(void) ;
uint8   USBFS_Dp_ClearInterrupt(void) ;


/***************************************
*           API Constants        
***************************************/

/* Drive Modes */
#define USBFS_Dp_DM_ALG_HIZ         PIN_DM_ALG_HIZ
#define USBFS_Dp_DM_DIG_HIZ         PIN_DM_DIG_HIZ
#define USBFS_Dp_DM_RES_UP          PIN_DM_RES_UP
#define USBFS_Dp_DM_RES_DWN         PIN_DM_RES_DWN
#define USBFS_Dp_DM_OD_LO           PIN_DM_OD_LO
#define USBFS_Dp_DM_OD_HI           PIN_DM_OD_HI
#define USBFS_Dp_DM_STRONG          PIN_DM_STRONG
#define USBFS_Dp_DM_RES_UPDWN       PIN_DM_RES_UPDWN

/* Digital Port Constants */
#define USBFS_Dp_MASK               USBFS_Dp__MASK
#define USBFS_Dp_SHIFT              USBFS_Dp__SHIFT
#define USBFS_Dp_WIDTH              1u


/***************************************
*             Registers        
***************************************/

/* Main Port Registers */
/* Pin State */
#define USBFS_Dp_PS                     (* (reg8 *) USBFS_Dp__PS)
/* Data Register */
#define USBFS_Dp_DR                     (* (reg8 *) USBFS_Dp__DR)
/* Port Number */
#define USBFS_Dp_PRT_NUM                (* (reg8 *) USBFS_Dp__PRT) 
/* Connect to Analog Globals */                                                  
#define USBFS_Dp_AG                     (* (reg8 *) USBFS_Dp__AG)                       
/* Analog MUX bux enable */
#define USBFS_Dp_AMUX                   (* (reg8 *) USBFS_Dp__AMUX) 
/* Bidirectional Enable */                                                        
#define USBFS_Dp_BIE                    (* (reg8 *) USBFS_Dp__BIE)
/* Bit-mask for Aliased Register Access */
#define USBFS_Dp_BIT_MASK               (* (reg8 *) USBFS_Dp__BIT_MASK)
/* Bypass Enable */
#define USBFS_Dp_BYP                    (* (reg8 *) USBFS_Dp__BYP)
/* Port wide control signals */                                                   
#define USBFS_Dp_CTL                    (* (reg8 *) USBFS_Dp__CTL)
/* Drive Modes */
#define USBFS_Dp_DM0                    (* (reg8 *) USBFS_Dp__DM0) 
#define USBFS_Dp_DM1                    (* (reg8 *) USBFS_Dp__DM1)
#define USBFS_Dp_DM2                    (* (reg8 *) USBFS_Dp__DM2) 
/* Input Buffer Disable Override */
#define USBFS_Dp_INP_DIS                (* (reg8 *) USBFS_Dp__INP_DIS)
/* LCD Common or Segment Drive */
#define USBFS_Dp_LCD_COM_SEG            (* (reg8 *) USBFS_Dp__LCD_COM_SEG)
/* Enable Segment LCD */
#define USBFS_Dp_LCD_EN                 (* (reg8 *) USBFS_Dp__LCD_EN)
/* Slew Rate Control */
#define USBFS_Dp_SLW                    (* (reg8 *) USBFS_Dp__SLW)

/* DSI Port Registers */
/* Global DSI Select Register */
#define USBFS_Dp_PRTDSI__CAPS_SEL       (* (reg8 *) USBFS_Dp__PRTDSI__CAPS_SEL) 
/* Double Sync Enable */
#define USBFS_Dp_PRTDSI__DBL_SYNC_IN    (* (reg8 *) USBFS_Dp__PRTDSI__DBL_SYNC_IN) 
/* Output Enable Select Drive Strength */
#define USBFS_Dp_PRTDSI__OE_SEL0        (* (reg8 *) USBFS_Dp__PRTDSI__OE_SEL0) 
#define USBFS_Dp_PRTDSI__OE_SEL1        (* (reg8 *) USBFS_Dp__PRTDSI__OE_SEL1) 
/* Port Pin Output Select Registers */
#define USBFS_Dp_PRTDSI__OUT_SEL0       (* (reg8 *) USBFS_Dp__PRTDSI__OUT_SEL0) 
#define USBFS_Dp_PRTDSI__OUT_SEL1       (* (reg8 *) USBFS_Dp__PRTDSI__OUT_SEL1) 
/* Sync Output Enable Registers */
#define USBFS_Dp_PRTDSI__SYNC_OUT       (* (reg8 *) USBFS_Dp__PRTDSI__SYNC_OUT) 


#if defined(USBFS_Dp__INTSTAT)  /* Interrupt Registers */

    #define USBFS_Dp_INTSTAT                (* (reg8 *) USBFS_Dp__INTSTAT)
    #define USBFS_Dp_SNAP                   (* (reg8 *) USBFS_Dp__SNAP)

#endif /* Interrupt Registers */

#endif /* CY_PSOC5A... */

#endif /*  CY_PINS_USBFS_Dp_H */


/* [] END OF FILE */
