/*******************************************************************************
* File Name: SDCard_PM.c
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

#include "SDCard_PVT.h"

static SDCard_BACKUP_STRUCT SDCard_backup =
{
    SDCard_DISABLED,
    SDCard_BITCTR_INIT,
    #if(CY_UDB_V0)
        SDCard_TX_INIT_INTERRUPTS_MASK,
        SDCard_RX_INIT_INTERRUPTS_MASK
    #endif /* CY_UDB_V0 */
};


/*******************************************************************************
* Function Name: SDCard_SaveConfig
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
*  SDCard_backup - modified when non-retention registers are saved.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_SaveConfig(void) 
{
    /* Store Status Mask registers */
    #if(CY_UDB_V0)
       SDCard_backup.cntrPeriod      = SDCard_COUNTER_PERIOD_REG;
       SDCard_backup.saveSrTxIntMask = SDCard_TX_STATUS_MASK_REG;
       SDCard_backup.saveSrRxIntMask = SDCard_RX_STATUS_MASK_REG;
    #endif /* (CY_UDB_V0) */
}


/*******************************************************************************
* Function Name: SDCard_RestoreConfig
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
*  SDCard_backup - used when non-retention registers are restored.
*
* Side Effects:
*  If this API is called without first calling SaveConfig then in the following
*  registers will be default values from Customizer:
*  SDCard_STATUS_MASK_REG and SDCard_COUNTER_PERIOD_REG.
*
*******************************************************************************/
void SDCard_RestoreConfig(void) 
{
    /* Restore the data, saved by SaveConfig() function */
    #if(CY_UDB_V0)
        SDCard_COUNTER_PERIOD_REG = SDCard_backup.cntrPeriod;
        SDCard_TX_STATUS_MASK_REG = ((uint8) SDCard_backup.saveSrTxIntMask);
        SDCard_RX_STATUS_MASK_REG = ((uint8) SDCard_backup.saveSrRxIntMask);
    #endif /* (CY_UDB_V0) */
}


/*******************************************************************************
* Function Name: SDCard_Sleep
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
*  SDCard_backup - modified when non-retention registers are saved.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_Sleep(void) 
{
    /* Save components enable state */
    SDCard_backup.enableState = ((uint8) SDCard_IS_ENABLED);

    SDCard_Stop();
    SDCard_SaveConfig();
}


/*******************************************************************************
* Function Name: SDCard_Wakeup
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
*  SDCard_backup - used when non-retention registers are restored.
*  SDCard_txBufferWrite - modified every function call - resets to
*  zero.
*  SDCard_txBufferRead - modified every function call - resets to
*  zero.
*  SDCard_rxBufferWrite - modified every function call - resets to
*  zero.
*  SDCard_rxBufferRead - modified every function call - resets to
*  zero.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_Wakeup(void) 
{
    SDCard_RestoreConfig();

    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)
        SDCard_rxBufferFull  = 0u;
        SDCard_rxBufferRead  = 0u;
        SDCard_rxBufferWrite = 0u;
    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        SDCard_txBufferFull  = 0u;
        SDCard_txBufferRead  = 0u;
        SDCard_txBufferWrite = 0u;
    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    /* Clear any data from the RX and TX FIFO */
    SDCard_ClearFIFO();

    /* Restore components block enable state */
    if(0u != SDCard_backup.enableState)
    {
        SDCard_Enable();
    }
}


/* [] END OF FILE */
