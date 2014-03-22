/*******************************************************************************
* File Name: .h
* Version 2.40
*
* Description:
*  This private header file contains internal definitions for the SPIM
*  component. Do not use these definitions directly in your application.
*
* Note:
*
********************************************************************************
* Copyright 2012, Cypress Semiconductor Corporation. All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_SPIM_PVT_SDCard_H)
#define CY_SPIM_PVT_SDCard_H

#include "SDCard.h"


/**********************************
*   Functions with external linkage
**********************************/


/**********************************
*   Variables with external linkage
**********************************/

extern volatile uint8 SDCard_swStatusTx;
extern volatile uint8 SDCard_swStatusRx;

#if(SDCard_TX_SOFTWARE_BUF_ENABLED)
    extern volatile uint8 SDCard_txBuffer[SDCard_TX_BUFFER_SIZE];
    extern volatile uint8 SDCard_txBufferRead;
    extern volatile uint8 SDCard_txBufferWrite;
    extern volatile uint8 SDCard_txBufferFull;
#endif /* (SDCard_TX_SOFTWARE_BUF_ENABLED) */

#if(SDCard_RX_SOFTWARE_BUF_ENABLED)
    extern volatile uint8 SDCard_rxBuffer[SDCard_RX_BUFFER_SIZE];
    extern volatile uint8 SDCard_rxBufferRead;
    extern volatile uint8 SDCard_rxBufferWrite;
    extern volatile uint8 SDCard_rxBufferFull;
#endif /* (SDCard_RX_SOFTWARE_BUF_ENABLED) */

#endif /* CY_SPIM_PVT_SDCard_H */


/* [] END OF FILE */
