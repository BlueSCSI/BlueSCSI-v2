/*******************************************************************************
* File Name: SCSI_CTL_PHASE.h  
* Version 1.80
*
* Description:
*  This file containts Control Register function prototypes and register defines
*
* Note:
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_CONTROL_REG_SCSI_CTL_PHASE_H) /* CY_CONTROL_REG_SCSI_CTL_PHASE_H */
#define CY_CONTROL_REG_SCSI_CTL_PHASE_H

#include "cytypes.h"

    
/***************************************
*     Data Struct Definitions
***************************************/

/* Sleep Mode API Support */
typedef struct
{
    uint8 controlState;

} SCSI_CTL_PHASE_BACKUP_STRUCT;


/***************************************
*         Function Prototypes 
***************************************/

void    SCSI_CTL_PHASE_Write(uint8 control) ;
uint8   SCSI_CTL_PHASE_Read(void) ;

void SCSI_CTL_PHASE_SaveConfig(void) ;
void SCSI_CTL_PHASE_RestoreConfig(void) ;
void SCSI_CTL_PHASE_Sleep(void) ; 
void SCSI_CTL_PHASE_Wakeup(void) ;


/***************************************
*            Registers        
***************************************/

/* Control Register */
#define SCSI_CTL_PHASE_Control        (* (reg8 *) SCSI_CTL_PHASE_Sync_ctrl_reg__CONTROL_REG )
#define SCSI_CTL_PHASE_Control_PTR    (  (reg8 *) SCSI_CTL_PHASE_Sync_ctrl_reg__CONTROL_REG )

#endif /* End CY_CONTROL_REG_SCSI_CTL_PHASE_H */


/* [] END OF FILE */
