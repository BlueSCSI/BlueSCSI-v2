/*******************************************************************************
* File Name: SCSI_Filtered.h  
* Version 1.90
*
* Description:
*  This file containts Status Register function prototypes and register defines
*
* Note:
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_STATUS_REG_SCSI_Filtered_H) /* CY_STATUS_REG_SCSI_Filtered_H */
#define CY_STATUS_REG_SCSI_Filtered_H

#include "cytypes.h"
#include "CyLib.h"

    
/***************************************
*     Data Struct Definitions
***************************************/

/* Sleep Mode API Support */
typedef struct
{
    uint8 statusState;

} SCSI_Filtered_BACKUP_STRUCT;


/***************************************
*        Function Prototypes
***************************************/

uint8 SCSI_Filtered_Read(void) ;
void SCSI_Filtered_InterruptEnable(void) ;
void SCSI_Filtered_InterruptDisable(void) ;
void SCSI_Filtered_WriteMask(uint8 mask) ;
uint8 SCSI_Filtered_ReadMask(void) ;


/***************************************
*           API Constants
***************************************/

#define SCSI_Filtered_STATUS_INTR_ENBL    0x10u


/***************************************
*         Parameter Constants
***************************************/

/* Status Register Inputs */
#define SCSI_Filtered_INPUTS              5


/***************************************
*             Registers
***************************************/

/* Status Register */
#define SCSI_Filtered_Status             (* (reg8 *) SCSI_Filtered_sts_sts_reg__STATUS_REG )
#define SCSI_Filtered_Status_PTR         (  (reg8 *) SCSI_Filtered_sts_sts_reg__STATUS_REG )
#define SCSI_Filtered_Status_Mask        (* (reg8 *) SCSI_Filtered_sts_sts_reg__MASK_REG )
#define SCSI_Filtered_Status_Aux_Ctrl    (* (reg8 *) SCSI_Filtered_sts_sts_reg__STATUS_AUX_CTL_REG )

#endif /* End CY_STATUS_REG_SCSI_Filtered_H */


/* [] END OF FILE */
