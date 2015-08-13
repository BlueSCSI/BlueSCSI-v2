/*******************************************************************************
* File Name: SDCard.c
* Version 2.50
*
* Description:
*  This file provides all API functionality of the SPI Master component.
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

#if(SDCard_TX_SOFTWARE_BUF_ENABLED)
    volatile uint8 SDCard_txBuffer[SDCard_TX_BUFFER_SIZE];
    volatile uint8 SDCard_txBufferFull;
    volatile uint8 SDCard_txBufferRead;
    volatile uint8 SDCard_txBufferWrite;
#endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

#if(SDCard_RX_SOFTWARE_BUF_ENABLED)
    volatile uint8 SDCard_rxBuffer[SDCard_RX_BUFFER_SIZE];
    volatile uint8 SDCard_rxBufferFull;
    volatile uint8 SDCard_rxBufferRead;
    volatile uint8 SDCard_rxBufferWrite;
#endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

uint8 SDCard_initVar = 0u;

volatile uint8 SDCard_swStatusTx;
volatile uint8 SDCard_swStatusRx;


/*******************************************************************************
* Function Name: SDCard_Init
********************************************************************************
*
* Summary:
*  Inits/Restores default SPIM configuration provided with customizer.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Side Effects:
*  When this function is called it initializes all of the necessary parameters
*  for execution. i.e. setting the initial interrupt mask, configuring the
*  interrupt service routine, configuring the bit-counter parameters and
*  clearing the FIFO and Status Register.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_Init(void) 
{
    /* Initialize the Bit counter */
    SDCard_COUNTER_PERIOD_REG = SDCard_BITCTR_INIT;

    /* Init TX ISR  */
    #if(0u != SDCard_INTERNAL_TX_INT_ENABLED)
        CyIntDisable         (SDCard_TX_ISR_NUMBER);
        CyIntSetPriority     (SDCard_TX_ISR_NUMBER,  SDCard_TX_ISR_PRIORITY);
        (void) CyIntSetVector(SDCard_TX_ISR_NUMBER, &SDCard_TX_ISR);
    #endif /* (0u != SDCard_INTERNAL_TX_INT_ENABLED) */

    /* Init RX ISR  */
    #if(0u != SDCard_INTERNAL_RX_INT_ENABLED)
        CyIntDisable         (SDCard_RX_ISR_NUMBER);
        CyIntSetPriority     (SDCard_RX_ISR_NUMBER,  SDCard_RX_ISR_PRIORITY);
        (void) CyIntSetVector(SDCard_RX_ISR_NUMBER, &SDCard_RX_ISR);
    #endif /* (0u != SDCard_INTERNAL_RX_INT_ENABLED) */

    /* Clear any stray data from the RX and TX FIFO */
    SDCard_ClearFIFO();

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

    (void) SDCard_ReadTxStatus(); /* Clear Tx status and swStatusTx */
    (void) SDCard_ReadRxStatus(); /* Clear Rx status and swStatusRx */

    /* Configure TX and RX interrupt mask */
    SDCard_TX_STATUS_MASK_REG = SDCard_TX_INIT_INTERRUPTS_MASK;
    SDCard_RX_STATUS_MASK_REG = SDCard_RX_INIT_INTERRUPTS_MASK;
}


/*******************************************************************************
* Function Name: SDCard_Enable
********************************************************************************
*
* Summary:
*  Enable SPIM component.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void SDCard_Enable(void) 
{
    uint8 enableInterrupts;

    enableInterrupts = CyEnterCriticalSection();
    SDCard_COUNTER_CONTROL_REG |= SDCard_CNTR_ENABLE;
    SDCard_TX_STATUS_ACTL_REG  |= SDCard_INT_ENABLE;
    SDCard_RX_STATUS_ACTL_REG  |= SDCard_INT_ENABLE;
    CyExitCriticalSection(enableInterrupts);

    #if(0u != SDCard_INTERNAL_CLOCK)
        SDCard_IntClock_Enable();
    #endif /* (0u != SDCard_INTERNAL_CLOCK) */

    SDCard_EnableTxInt();
    SDCard_EnableRxInt();
}


/*******************************************************************************
* Function Name: SDCard_Start
********************************************************************************
*
* Summary:
*  Initialize and Enable the SPI Master component.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global variables:
*  SDCard_initVar - used to check initial configuration, modified on
*  first function call.
*
* Theory:
*  Enable the clock input to enable operation.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_Start(void) 
{
    if(0u == SDCard_initVar)
    {
        SDCard_Init();
        SDCard_initVar = 1u;
    }

    SDCard_Enable();
}


/*******************************************************************************
* Function Name: SDCard_Stop
********************************************************************************
*
* Summary:
*  Disable the SPI Master component.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Disable the clock input to enable operation.
*
*******************************************************************************/
void SDCard_Stop(void) 
{
    uint8 enableInterrupts;

    enableInterrupts = CyEnterCriticalSection();
    SDCard_TX_STATUS_ACTL_REG &= ((uint8) ~SDCard_INT_ENABLE);
    SDCard_RX_STATUS_ACTL_REG &= ((uint8) ~SDCard_INT_ENABLE);
    CyExitCriticalSection(enableInterrupts);

    #if(0u != SDCard_INTERNAL_CLOCK)
        SDCard_IntClock_Disable();
    #endif /* (0u != SDCard_INTERNAL_CLOCK) */

    SDCard_DisableTxInt();
    SDCard_DisableRxInt();
}


/*******************************************************************************
* Function Name: SDCard_EnableTxInt
********************************************************************************
*
* Summary:
*  Enable internal Tx interrupt generation.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Enable the internal Tx interrupt output -or- the interrupt component itself.
*
*******************************************************************************/
void SDCard_EnableTxInt(void) 
{
    #if(0u != SDCard_INTERNAL_TX_INT_ENABLED)
        CyIntEnable(SDCard_TX_ISR_NUMBER);
    #endif /* (0u != SDCard_INTERNAL_TX_INT_ENABLED) */
}


/*******************************************************************************
* Function Name: SDCard_EnableRxInt
********************************************************************************
*
* Summary:
*  Enable internal Rx interrupt generation.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Enable the internal Rx interrupt output -or- the interrupt component itself.
*
*******************************************************************************/
void SDCard_EnableRxInt(void) 
{
    #if(0u != SDCard_INTERNAL_RX_INT_ENABLED)
        CyIntEnable(SDCard_RX_ISR_NUMBER);
    #endif /* (0u != SDCard_INTERNAL_RX_INT_ENABLED) */
}


/*******************************************************************************
* Function Name: SDCard_DisableTxInt
********************************************************************************
*
* Summary:
*  Disable internal Tx interrupt generation.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Disable the internal Tx interrupt output -or- the interrupt component itself.
*
*******************************************************************************/
void SDCard_DisableTxInt(void) 
{
    #if(0u != SDCard_INTERNAL_TX_INT_ENABLED)
        CyIntDisable(SDCard_TX_ISR_NUMBER);
    #endif /* (0u != SDCard_INTERNAL_TX_INT_ENABLED) */
}


/*******************************************************************************
* Function Name: SDCard_DisableRxInt
********************************************************************************
*
* Summary:
*  Disable internal Rx interrupt generation.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Disable the internal Rx interrupt output -or- the interrupt component itself.
*
*******************************************************************************/
void SDCard_DisableRxInt(void) 
{
    #if(0u != SDCard_INTERNAL_RX_INT_ENABLED)
        CyIntDisable(SDCard_RX_ISR_NUMBER);
    #endif /* (0u != SDCard_INTERNAL_RX_INT_ENABLED) */
}


/*******************************************************************************
* Function Name: SDCard_SetTxInterruptMode
********************************************************************************
*
* Summary:
*  Configure which status bits trigger an interrupt event.
*
* Parameters:
*  intSrc: An or'd combination of the desired status bit masks (defined in the
*  header file).
*
* Return:
*  None.
*
* Theory:
*  Enables the output of specific status bits to the interrupt controller.
*
*******************************************************************************/
void SDCard_SetTxInterruptMode(uint8 intSrc) 
{
    SDCard_TX_STATUS_MASK_REG = intSrc;
}


/*******************************************************************************
* Function Name: SDCard_SetRxInterruptMode
********************************************************************************
*
* Summary:
*  Configure which status bits trigger an interrupt event.
*
* Parameters:
*  intSrc: An or'd combination of the desired status bit masks (defined in the
*  header file).
*
* Return:
*  None.
*
* Theory:
*  Enables the output of specific status bits to the interrupt controller.
*
*******************************************************************************/
void SDCard_SetRxInterruptMode(uint8 intSrc) 
{
    SDCard_RX_STATUS_MASK_REG  = intSrc;
}


/*******************************************************************************
* Function Name: SDCard_ReadTxStatus
********************************************************************************
*
* Summary:
*  Read the Tx status register for the component.
*
* Parameters:
*  None.
*
* Return:
*  Contents of the Tx status register.
*
* Global variables:
*  SDCard_swStatusTx - used to store in software status register,
*  modified every function call - resets to zero.
*
* Theory:
*  Allows the user and the API to read the Tx status register for error
*  detection and flow control.
*
* Side Effects:
*  Clear Tx status register of the component.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 SDCard_ReadTxStatus(void) 
{
    uint8 tmpStatus;

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        /* Disable TX interrupt to protect global veriables */
        SDCard_DisableTxInt();

        tmpStatus = SDCard_GET_STATUS_TX(SDCard_swStatusTx);
        SDCard_swStatusTx = 0u;

        SDCard_EnableTxInt();

    #else

        tmpStatus = SDCard_TX_STATUS_REG;

    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    return(tmpStatus);
}


/*******************************************************************************
* Function Name: SDCard_ReadRxStatus
********************************************************************************
*
* Summary:
*  Read the Rx status register for the component.
*
* Parameters:
*  None.
*
* Return:
*  Contents of the Rx status register.
*
* Global variables:
*  SDCard_swStatusRx - used to store in software Rx status register,
*  modified every function call - resets to zero.
*
* Theory:
*  Allows the user and the API to read the Rx status register for error
*  detection and flow control.
*
* Side Effects:
*  Clear Rx status register of the component.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 SDCard_ReadRxStatus(void) 
{
    uint8 tmpStatus;

    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)
        /* Disable RX interrupt to protect global veriables */
        SDCard_DisableRxInt();

        tmpStatus = SDCard_GET_STATUS_RX(SDCard_swStatusRx);
        SDCard_swStatusRx = 0u;

        SDCard_EnableRxInt();

    #else

        tmpStatus = SDCard_RX_STATUS_REG;

    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

    return(tmpStatus);
}


/*******************************************************************************
* Function Name: SDCard_WriteTxData
********************************************************************************
*
* Summary:
*  Write a byte of data to be sent across the SPI.
*
* Parameters:
*  txDataByte: The data value to send across the SPI.
*
* Return:
*  None.
*
* Global variables:
*  SDCard_txBufferWrite - used for the account of the bytes which
*  have been written down in the TX software buffer, modified every function
*  call if TX Software Buffer is used.
*  SDCard_txBufferRead - used for the account of the bytes which
*  have been read from the TX software buffer.
*  SDCard_txBuffer[SDCard_TX_BUFFER_SIZE] - used to store
*  data to sending, modified every function call if TX Software Buffer is used.
*
* Theory:
*  Allows the user to transmit any byte of data in a single transfer.
*
* Side Effects:
*  If this function is called again before the previous byte is finished then
*  the next byte will be appended to the transfer with no time between
*  the byte transfers. Clear Tx status register of the component.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_WriteTxData(uint8 txData) 
{
    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)

        uint8 tempStatus;
        uint8 tmpTxBufferRead;

        /* Block if TX buffer is FULL: don't overwrite */
        do
        {
            tmpTxBufferRead = SDCard_txBufferRead;
            if(0u == tmpTxBufferRead)
            {
                tmpTxBufferRead = (SDCard_TX_BUFFER_SIZE - 1u);
            }
            else
            {
                tmpTxBufferRead--;
            }

        }while(tmpTxBufferRead == SDCard_txBufferWrite);

        /* Disable TX interrupt to protect global veriables */
        SDCard_DisableTxInt();

        tempStatus = SDCard_GET_STATUS_TX(SDCard_swStatusTx);
        SDCard_swStatusTx = tempStatus;


        if((SDCard_txBufferRead == SDCard_txBufferWrite) &&
           (0u != (SDCard_swStatusTx & SDCard_STS_TX_FIFO_NOT_FULL)))
        {
            /* Put data element into the TX FIFO */
            CY_SET_REG8(SDCard_TXDATA_PTR, txData);
        }
        else
        {
            /* Add to the TX software buffer */
            SDCard_txBufferWrite++;
            if(SDCard_txBufferWrite >= SDCard_TX_BUFFER_SIZE)
            {
                SDCard_txBufferWrite = 0u;
            }

            if(SDCard_txBufferWrite == SDCard_txBufferRead)
            {
                SDCard_txBufferRead++;
                if(SDCard_txBufferRead >= SDCard_TX_BUFFER_SIZE)
                {
                    SDCard_txBufferRead = 0u;
                }
                SDCard_txBufferFull = 1u;
            }

            SDCard_txBuffer[SDCard_txBufferWrite] = txData;

            SDCard_TX_STATUS_MASK_REG |= SDCard_STS_TX_FIFO_NOT_FULL;
        }

        SDCard_EnableTxInt();

    #else
        /* Wait until TX FIFO has a place */
        while(0u == (SDCard_TX_STATUS_REG & SDCard_STS_TX_FIFO_NOT_FULL))
        {
        }

        /* Put data element into the TX FIFO */
        CY_SET_REG8(SDCard_TXDATA_PTR, txData);

    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */
}


/*******************************************************************************
* Function Name: SDCard_ReadRxData
********************************************************************************
*
* Summary:
*  Read the next byte of data received across the SPI.
*
* Parameters:
*  None.
*
* Return:
*  The next byte of data read from the FIFO.
*
* Global variables:
*  SDCard_rxBufferWrite - used for the account of the bytes which
*  have been written down in the RX software buffer.
*  SDCard_rxBufferRead - used for the account of the bytes which
*  have been read from the RX software buffer, modified every function
*  call if RX Software Buffer is used.
*  SDCard_rxBuffer[SDCard_RX_BUFFER_SIZE] - used to store
*  received data.
*
* Theory:
*  Allows the user to read a byte of data received.
*
* Side Effects:
*  Will return invalid data if the FIFO is empty. The user should Call
*  GetRxBufferSize() and if it returns a non-zero value then it is safe to call
*  ReadByte() function.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 SDCard_ReadRxData(void) 
{
    uint8 rxData;

    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)

        /* Disable RX interrupt to protect global veriables */
        SDCard_DisableRxInt();

        if(SDCard_rxBufferRead != SDCard_rxBufferWrite)
        {
            if(0u == SDCard_rxBufferFull)
            {
                SDCard_rxBufferRead++;
                if(SDCard_rxBufferRead >= SDCard_RX_BUFFER_SIZE)
                {
                    SDCard_rxBufferRead = 0u;
                }
            }
            else
            {
                SDCard_rxBufferFull = 0u;
            }
        }

        rxData = SDCard_rxBuffer[SDCard_rxBufferRead];

        SDCard_EnableRxInt();

    #else

        rxData = CY_GET_REG8(SDCard_RXDATA_PTR);

    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

    return(rxData);
}


/*******************************************************************************
* Function Name: SDCard_GetRxBufferSize
********************************************************************************
*
* Summary:
*  Returns the number of bytes/words of data currently held in the RX buffer.
*  If RX Software Buffer not used then function return 0 if FIFO empty or 1 if
*  FIFO not empty. In another case function return size of RX Software Buffer.
*
* Parameters:
*  None.
*
* Return:
*  Integer count of the number of bytes/words in the RX buffer.
*
* Global variables:
*  SDCard_rxBufferWrite - used for the account of the bytes which
*  have been written down in the RX software buffer.
*  SDCard_rxBufferRead - used for the account of the bytes which
*  have been read from the RX software buffer.
*
* Side Effects:
*  Clear status register of the component.
*
*******************************************************************************/
uint8 SDCard_GetRxBufferSize(void) 
{
    uint8 size;

    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)

        /* Disable RX interrupt to protect global veriables */
        SDCard_DisableRxInt();

        if(SDCard_rxBufferRead == SDCard_rxBufferWrite)
        {
            size = 0u;
        }
        else if(SDCard_rxBufferRead < SDCard_rxBufferWrite)
        {
            size = (SDCard_rxBufferWrite - SDCard_rxBufferRead);
        }
        else
        {
            size = (SDCard_RX_BUFFER_SIZE - SDCard_rxBufferRead) + SDCard_rxBufferWrite;
        }

        SDCard_EnableRxInt();

    #else

        /* We can only know if there is data in the RX FIFO */
        size = (0u != (SDCard_RX_STATUS_REG & SDCard_STS_RX_FIFO_NOT_EMPTY)) ? 1u : 0u;

    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    return(size);
}


/*******************************************************************************
* Function Name: SDCard_GetTxBufferSize
********************************************************************************
*
* Summary:
*  Returns the number of bytes/words of data currently held in the TX buffer.
*  If TX Software Buffer not used then function return 0 - if FIFO empty, 1 - if
*  FIFO not full, 4 - if FIFO full. In another case function return size of TX
*  Software Buffer.
*
* Parameters:
*  None.
*
* Return:
*  Integer count of the number of bytes/words in the TX buffer.
*
* Global variables:
*  SDCard_txBufferWrite - used for the account of the bytes which
*  have been written down in the TX software buffer.
*  SDCard_txBufferRead - used for the account of the bytes which
*  have been read from the TX software buffer.
*
* Side Effects:
*  Clear status register of the component.
*
*******************************************************************************/
uint8  SDCard_GetTxBufferSize(void) 
{
    uint8 size;

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        /* Disable TX interrupt to protect global veriables */
        SDCard_DisableTxInt();

        if(SDCard_txBufferRead == SDCard_txBufferWrite)
        {
            size = 0u;
        }
        else if(SDCard_txBufferRead < SDCard_txBufferWrite)
        {
            size = (SDCard_txBufferWrite - SDCard_txBufferRead);
        }
        else
        {
            size = (SDCard_TX_BUFFER_SIZE - SDCard_txBufferRead) + SDCard_txBufferWrite;
        }

        SDCard_EnableTxInt();

    #else

        size = SDCard_TX_STATUS_REG;

        if(0u != (size & SDCard_STS_TX_FIFO_EMPTY))
        {
            size = 0u;
        }
        else if(0u != (size & SDCard_STS_TX_FIFO_NOT_FULL))
        {
            size = 1u;
        }
        else
        {
            size = SDCard_FIFO_SIZE;
        }

    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

    return(size);
}


/*******************************************************************************
* Function Name: SDCard_ClearRxBuffer
********************************************************************************
*
* Summary:
*  Clear the RX RAM buffer by setting the read and write pointers both to zero.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global variables:
*  SDCard_rxBufferWrite - used for the account of the bytes which
*  have been written down in the RX software buffer, modified every function
*  call - resets to zero.
*  SDCard_rxBufferRead - used for the account of the bytes which
*  have been read from the RX software buffer, modified every function call -
*  resets to zero.
*
* Theory:
*  Setting the pointers to zero makes the system believe there is no data to
*  read and writing will resume at address 0 overwriting any data that may have
*  remained in the RAM.
*
* Side Effects:
*  Any received data not read from the RAM buffer will be lost when overwritten.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_ClearRxBuffer(void) 
{
    /* Clear Hardware RX FIFO */
    while(0u !=(SDCard_RX_STATUS_REG & SDCard_STS_RX_FIFO_NOT_EMPTY))
    {
        (void) CY_GET_REG8(SDCard_RXDATA_PTR);
    }

    #if(SDCard_RX_SOFTWARE_BUF_ENABLED)
        /* Disable RX interrupt to protect global veriables */
        SDCard_DisableRxInt();

        SDCard_rxBufferFull  = 0u;
        SDCard_rxBufferRead  = 0u;
        SDCard_rxBufferWrite = 0u;

        SDCard_EnableRxInt();
    #endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */
}


/*******************************************************************************
* Function Name: SDCard_ClearTxBuffer
********************************************************************************
*
* Summary:
*  Clear the TX RAM buffer by setting the read and write pointers both to zero.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Global variables:
*  SDCard_txBufferWrite - used for the account of the bytes which
*  have been written down in the TX software buffer, modified every function
*  call - resets to zero.
*  SDCard_txBufferRead - used for the account of the bytes which
*  have been read from the TX software buffer, modified every function call -
*  resets to zero.
*
* Theory:
*  Setting the pointers to zero makes the system believe there is no data to
*  read and writing will resume at address 0 overwriting any data that may have
*  remained in the RAM.
*
* Side Effects:
*  Any data not yet transmitted from the RAM buffer will be lost when
*  overwritten.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_ClearTxBuffer(void) 
{
    uint8 enableInterrupts;

    enableInterrupts = CyEnterCriticalSection();
    /* Clear TX FIFO */
    SDCard_AUX_CONTROL_DP0_REG |= ((uint8)  SDCard_TX_FIFO_CLR);
    SDCard_AUX_CONTROL_DP0_REG &= ((uint8) ~SDCard_TX_FIFO_CLR);

    #if(SDCard_USE_SECOND_DATAPATH)
        /* Clear TX FIFO for 2nd Datapath */
        SDCard_AUX_CONTROL_DP1_REG |= ((uint8)  SDCard_TX_FIFO_CLR);
        SDCard_AUX_CONTROL_DP1_REG &= ((uint8) ~SDCard_TX_FIFO_CLR);
    #endif /* (SDCard_USE_SECOND_DATAPATH) */
    CyExitCriticalSection(enableInterrupts);

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED)
        /* Disable TX interrupt to protect global veriables */
        SDCard_DisableTxInt();

        SDCard_txBufferFull  = 0u;
        SDCard_txBufferRead  = 0u;
        SDCard_txBufferWrite = 0u;

        /* Buffer is EMPTY: disable TX FIFO NOT FULL interrupt */
        SDCard_TX_STATUS_MASK_REG &= ((uint8) ~SDCard_STS_TX_FIFO_NOT_FULL);

        SDCard_EnableTxInt();
    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */
}


#if(0u != SDCard_BIDIRECTIONAL_MODE)
    /*******************************************************************************
    * Function Name: SDCard_TxEnable
    ********************************************************************************
    *
    * Summary:
    *  If the SPI master is configured to use a single bi-directional pin then this
    *  will set the bi-directional pin to transmit.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    void SDCard_TxEnable(void) 
    {
        SDCard_CONTROL_REG |= SDCard_CTRL_TX_SIGNAL_EN;
    }


    /*******************************************************************************
    * Function Name: SDCard_TxDisable
    ********************************************************************************
    *
    * Summary:
    *  If the SPI master is configured to use a single bi-directional pin then this
    *  will set the bi-directional pin to receive.
    *
    * Parameters:
    *  None.
    *
    * Return:
    *  None.
    *
    *******************************************************************************/
    void SDCard_TxDisable(void) 
    {
        SDCard_CONTROL_REG &= ((uint8) ~SDCard_CTRL_TX_SIGNAL_EN);
    }

#endif /* (0u != SDCard_BIDIRECTIONAL_MODE) */


/*******************************************************************************
* Function Name: SDCard_PutArray
********************************************************************************
*
* Summary:
*  Write available data from ROM/RAM to the TX buffer while space is available
*  in the TX buffer. Keep trying until all data is passed to the TX buffer.
*
* Parameters:
*  *buffer: Pointer to the location in RAM containing the data to send
*  byteCount: The number of bytes to move to the transmit buffer.
*
* Return:
*  None.
*
* Side Effects:
*  Will stay in this routine until all data has been sent.  May get locked in
*  this loop if data is not being initiated by the master if there is not
*  enough room in the TX FIFO.
*
* Reentrant:
*  No.
*
*******************************************************************************/
void SDCard_PutArray(const uint8 buffer[], uint8 byteCount)
                                                                          
{
    uint8 bufIndex;

    bufIndex = 0u;

    while(byteCount > 0u)
    {
        SDCard_WriteTxData(buffer[bufIndex]);
        bufIndex++;
        byteCount--;
    }
}


/*******************************************************************************
* Function Name: SDCard_ClearFIFO
********************************************************************************
*
* Summary:
*  Clear the RX and TX FIFO's of all data for a fresh start.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Side Effects:
*  Clear status register of the component.
*
*******************************************************************************/
void SDCard_ClearFIFO(void) 
{
    uint8 enableInterrupts;

    /* Clear Hardware RX FIFO */
    while(0u !=(SDCard_RX_STATUS_REG & SDCard_STS_RX_FIFO_NOT_EMPTY))
    {
        (void) CY_GET_REG8(SDCard_RXDATA_PTR);
    }

    enableInterrupts = CyEnterCriticalSection();
    /* Clear TX FIFO */
    SDCard_AUX_CONTROL_DP0_REG |= ((uint8)  SDCard_TX_FIFO_CLR);
    SDCard_AUX_CONTROL_DP0_REG &= ((uint8) ~SDCard_TX_FIFO_CLR);

    #if(SDCard_USE_SECOND_DATAPATH)
        /* Clear TX FIFO for 2nd Datapath */
        SDCard_AUX_CONTROL_DP1_REG |= ((uint8)  SDCard_TX_FIFO_CLR);
        SDCard_AUX_CONTROL_DP1_REG &= ((uint8) ~SDCard_TX_FIFO_CLR);
    #endif /* (SDCard_USE_SECOND_DATAPATH) */
    CyExitCriticalSection(enableInterrupts);
}


/* Following functions are for version Compatibility, they are obsolete.
*  Please do not use it in new projects.
*/


/*******************************************************************************
* Function Name: SDCard_EnableInt
********************************************************************************
*
* Summary:
*  Enable internal interrupt generation.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Enable the internal interrupt output -or- the interrupt component itself.
*
*******************************************************************************/
void SDCard_EnableInt(void) 
{
    SDCard_EnableRxInt();
    SDCard_EnableTxInt();
}


/*******************************************************************************
* Function Name: SDCard_DisableInt
********************************************************************************
*
* Summary:
*  Disable internal interrupt generation.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
* Theory:
*  Disable the internal interrupt output -or- the interrupt component itself.
*
*******************************************************************************/
void SDCard_DisableInt(void) 
{
    SDCard_DisableTxInt();
    SDCard_DisableRxInt();
}


/*******************************************************************************
* Function Name: SDCard_SetInterruptMode
********************************************************************************
*
* Summary:
*  Configure which status bits trigger an interrupt event.
*
* Parameters:
*  intSrc: An or'd combination of the desired status bit masks (defined in the
*  header file).
*
* Return:
*  None.
*
* Theory:
*  Enables the output of specific status bits to the interrupt controller.
*
*******************************************************************************/
void SDCard_SetInterruptMode(uint8 intSrc) 
{
    SDCard_TX_STATUS_MASK_REG  = (intSrc & ((uint8) ~SDCard_STS_SPI_IDLE));
    SDCard_RX_STATUS_MASK_REG  =  intSrc;
}


/*******************************************************************************
* Function Name: SDCard_ReadStatus
********************************************************************************
*
* Summary:
*  Read the status register for the component.
*
* Parameters:
*  None.
*
* Return:
*  Contents of the status register.
*
* Global variables:
*  SDCard_swStatus - used to store in software status register,
*  modified every function call - resets to zero.
*
* Theory:
*  Allows the user and the API to read the status register for error detection
*  and flow control.
*
* Side Effects:
*  Clear status register of the component.
*
* Reentrant:
*  No.
*
*******************************************************************************/
uint8 SDCard_ReadStatus(void) 
{
    uint8 tmpStatus;

    #if(SDCard_TX_SOFTWARE_BUF_ENABLED || SDCard_RX_SOFTWARE_BUF_ENABLED)

        SDCard_DisableInt();

        tmpStatus  = SDCard_GET_STATUS_RX(SDCard_swStatusRx);
        tmpStatus |= SDCard_GET_STATUS_TX(SDCard_swStatusTx);
        tmpStatus &= ((uint8) ~SDCard_STS_SPI_IDLE);

        SDCard_swStatusTx = 0u;
        SDCard_swStatusRx = 0u;

        SDCard_EnableInt();

    #else

        tmpStatus  = SDCard_RX_STATUS_REG;
        tmpStatus |= SDCard_TX_STATUS_REG;
        tmpStatus &= ((uint8) ~SDCard_STS_SPI_IDLE);

    #endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED || SDCard_RX_SOFTWARE_BUF_ENABLED) */

    return(tmpStatus);
}


/* [] END OF FILE */
