/*******************************************************************************
* File Name: SD_PM.c
* Version 2.40
*
* Description:
*  This file contains the setup, control and status commands to support
*  component operations in low power mode.
*
* Note:
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "SD_PVT.h"

static SD_BACKUP_STRUCT SD_backup =
{
    SD_DISABLED,
    SD_BITCTR_INIT,
    #if(CY_UDB_V0)
        SD_TX_INIT_INTERRUPTS_MASK,
        SD_RX_INIT_INTERRUPTS_MASK
    #endif /* CY_UDB_V0 */
};


/*******************************************************************************
* Function Name: SD_SaveConfig
********************************************************************************
*
* Summary:
*  Saves SPIM configuration.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global Variables:
*  SD_backup - modified when non-retention registers are saved.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SD_SaveConfig(void) 
{
    /* Store Status Mask registers */
    #if(CY_UDB_V0)
       SD_backup.cntrPeriod      = SD_COUNTER_PERIOD_REG;
       SD_backup.saveSrTxIntMask = SD_TX_STATUS_MASK_REG;
       SD_backup.saveSrRxIntMask = SD_RX_STATUS_MASK_REG;
    #endif /* (CY_UDB_V0) */
}


/*******************************************************************************
* Function Name: SD_RestoreConfig
********************************************************************************
*
* Summary:
*  Restores SPIM configuration.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global Variables:
*  SD_backup - used when non-retention registers are restored.
*
* Side Effects:
*  If this API is called without first calling SaveConfig then in the following
*  registers will be default values from Customizer:
*  SD_STATUS_MASK_REG and SD_COUNTER_PERIOD_REG.
*
*******************************************************************************/
void SD_RestoreConfig(void) 
{
    /* Restore the data, saved by SaveConfig() function */
    #if(CY_UDB_V0)
        SD_COUNTER_PERIOD_REG = SD_backup.cntrPeriod;
        SD_TX_STATUS_MASK_REG = ((uint8) SD_backup.saveSrTxIntMask);
        SD_RX_STATUS_MASK_REG = ((uint8) SD_backup.saveSrRxIntMask);
    #endif /* (CY_UDB_V0) */
}


/*******************************************************************************
* Function Name: SD_Sleep
********************************************************************************
*
* Summary:
*  Prepare SPIM Component goes to sleep.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global Variables:
*  SD_backup - modified when non-retention registers are saved.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SD_Sleep(void) 
{
    /* Save components enable state */
    SD_backup.enableState = ((uint8) SD_IS_ENABLED);

    SD_Stop();
    SD_SaveConfig();
}


/*******************************************************************************
* Function Name: SD_Wakeup
********************************************************************************
*
* Summary:
*  Prepare SPIM Component to wake up.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global Variables:
*  SD_backup - used when non-retention registers are restored.
*  SD_txBufferWrite - modified every function call - resets to
*  zero.
*  SD_txBufferRead - modified every function call - resets to
*  zero.
*  SD_rxBufferWrite - modified every function call - resets to
*  zero.
*  SD_rxBufferRead - modified every function call - resets to
*  zero.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SD_Wakeup(void) 
{
    SD_RestoreConfig();

    #if(SD_RX_SOFTWARE_BUF_ENABLED)
        SD_rxBufferFull  = 0u;
        SD_rxBufferRead  = 0u;
        SD_rxBufferWrite = 0u;
    #endif /* (SD_RX_SOFTWARE_BUF_ENABLED) */

    #if(SD_TX_SOFTWARE_BUF_ENABLED)
        SD_txBufferFull  = 0u;
        SD_txBufferRead  = 0u;
        SD_txBufferWrite = 0u;
    #endif /* (SD_TX_SOFTWARE_BUF_ENABLED) */

    /* Clear any data from the RX and TX FIFO */
    SD_ClearFIFO();

    /* Restore components block enable state */
    if(0u != SD_backup.enableState)
    {
        SD_Enable();
    }
}


/* [] END OF FILE */
