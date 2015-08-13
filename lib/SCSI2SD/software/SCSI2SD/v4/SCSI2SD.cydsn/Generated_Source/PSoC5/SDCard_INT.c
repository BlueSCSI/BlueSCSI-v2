/*******************************************************************************
* File Name: SDCard_INT.c
* Version 2.50
*
* Description:
*  This file provides all Interrupt Service Routine (ISR) for the SPI Master
*  component.
*
* Note:
*  None.
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "SDCard_PVT.h"

/* User code required at start of ISR */
/* `#START SDCard_ISR_START_DEF` */

/* `#END` */


/*******************************************************************************
* Function Name: SDCard_TX_ISR
********************************************************************************
*
* Summary:
*  Interrupt Service Routine for TX portion of the SPI Master.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global variables:
*  SDCard_txBufferWrite - used for the account of the bytes which
*  have been written down in the TX software buffer.
*  SDCard_txBufferRead - used for the account of the bytes which
*  have been read from the TX software buffer, modified when exist data to
*  sending and FIFO Not Full.
*  SDCard_txBuffer[SDCard_TX_BUFFER_SIZE] - used to store
*  data to sending.
*  All described above Global variables are used when Software Buffer is used.
*
*******************************************************************************/
CY_ISR(SDCard_TX_ISR)
{
    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        uint8 tmpStatus;
    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    /* User code required at start of ISR */
    /* `#START SDCard_TX_ISR_START` */

    /* `#END` */

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        /* Check if TX data buffer is not empty and there is space in TX FIFO */
        while(SDCard_txBufferRead != SDCard_txBufferWrite)
        {
            tmpStatus = SDCard_GET_STATUS_TX(SDCard_swStatusTx);
            SDCard_swStatusTx = tmpStatus;

            if(0u != (SDCard_swStatusTx & SDCard_STS_TX_FIFO_NOT_FULL))
            {
                if(0u == SDCard_txBufferFull)
                {
                   SDCard_txBufferRead++;

                    if(SDCard_txBufferRead >= SDCard_TX_BUFFER_SIZE)
                    {
                        SDCard_txBufferRead = 0u;
                    }
                }
                else
                {
                    SDCard_txBufferFull = 0u;
                }

                /* Put data element into the TX FIFO */
                CY_SET_REG8(SDCard_TXDATA_PTR, 
                                             SDCard_txBuffer[SDCard_txBufferRead]);
            }
            else
            {
                break;
            }
        }

        if(SDCard_txBufferRead == SDCard_txBufferWrite)
        {
            /* TX Buffer is EMPTY: disable interrupt on TX NOT FULL */
            SDCard_TX_STATUS_MASK_REG &= ((uint8) ~SDCard_STS_TX_FIFO_NOT_FULL);
        }

    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    /* User code required at end of ISR (Optional) */
    /* `#START SDCard_TX_ISR_END` */

    /* `#END` */
}


/*******************************************************************************
* Function Name: SDCard_RX_ISR
********************************************************************************
*
* Summary:
*  Interrupt Service Routine for RX portion of the SPI Master.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global variables:
*  SDCard_rxBufferWrite - used for the account of the bytes which
*  have been written down in the RX software buffer modified when FIFO contains
*  new data.
*  SDCard_rxBufferRead - used for the account of the bytes which
*  have been read from the RX software buffer, modified when overflow occurred.
*  SDCard_rxBuffer[SDCard_RX_BUFFER_SIZE] - used to store
*  received data, modified when FIFO contains new data.
*  All described above Global variables are used when Software Buffer is used.
*
*******************************************************************************/
CY_ISR(SDCard_RX_ISR)
{
    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)
        uint8 tmpStatus;
        uint8 rxData;
    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

    /* User code required at start of ISR */
    /* `#START SDCard_RX_ISR_START` */

    /* `#END` */

    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)

        tmpStatus = SDCard_GET_STATUS_RX(SDCard_swStatusRx);
        SDCard_swStatusRx = tmpStatus;

        /* Check if RX data FIFO has some data to be moved into the RX Buffer */
        while(0u != (SDCard_swStatusRx & SDCard_STS_RX_FIFO_NOT_EMPTY))
        {
            rxData = CY_GET_REG8(SDCard_RXDATA_PTR);

            /* Set next pointer. */
            SDCard_rxBufferWrite++;
            if(SDCard_rxBufferWrite >= SDCard_RX_BUFFER_SIZE)
            {
                SDCard_rxBufferWrite = 0u;
            }

            if(SDCard_rxBufferWrite == SDCard_rxBufferRead)
            {
                SDCard_rxBufferRead++;
                if(SDCard_rxBufferRead >= SDCard_RX_BUFFER_SIZE)
                {
                    SDCard_rxBufferRead = 0u;
                }

                SDCard_rxBufferFull = 1u;
            }

            /* Move data from the FIFO to the Buffer */
            SDCard_rxBuffer[SDCard_rxBufferWrite] = rxData;

            tmpStatus = SDCard_GET_STATUS_RX(SDCard_swStatusRx);
            SDCard_swStatusRx = tmpStatus;
        }

    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

    /* User code required at end of ISR (Optional) */
    /* `#START SDCard_RX_ISR_END` */

    /* `#END` */
}

/* [] END OF FILE */
