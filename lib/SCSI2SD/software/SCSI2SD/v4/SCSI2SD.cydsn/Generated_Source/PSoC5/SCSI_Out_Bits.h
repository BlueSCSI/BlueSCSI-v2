/*******************************************************************************
* File Name: SCSI_Out_Bits.h  
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

#if !defined(CY_CONTROL_REG_SCSI_Out_Bits_H) /* CY_CONTROL_REG_SCSI_Out_Bits_H */
#define CY_CONTROL_REG_SCSI_Out_Bits_H

#include "cytypes.h"

    
/***************************************
*     Data Struct Definitions
***************************************/

/* Sleep Mode API Support */
typedef struct
{
    uint8 controlState;

} SCSI_Out_Bits_BACKUP_STRUCT;


/***************************************
*         Function Prototypes 
***************************************/

void    SCSI_Out_Bits_Write(uint8 control) ;
uint8   SCSI_Out_Bits_Read(void) ;

void SCSI_Out_Bits_SaveConfig(void) ;
void SCSI_Out_Bits_RestoreConfig(void) ;
void SCSI_Out_Bits_Sleep(void) ; 
void SCSI_Out_Bits_Wakeup(void) ;


/***************************************
*            Registers        
***************************************/

/* Control Register */
#define SCSI_Out_Bits_Control        (* (reg8 *) SCSI_Out_Bits_Sync_ctrl_reg__CONTROL_REG )
#define SCSI_Out_Bits_Control_PTR    (  (reg8 *) SCSI_Out_Bits_Sync_ctrl_reg__CONTROL_REG )

#endif /* End CY_CONTROL_REG_SCSI_Out_Bits_H */


/* [] END OF FILE */
