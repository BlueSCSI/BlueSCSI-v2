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

#if !defined(CY_SPIM_PVT_SD_H)
#define CY_SPIM_PVT_SD_H

#include "SD.h"


/**********************************
*   Functions with external linkage
**********************************/


/**********************************
*   Variables with external linkage
**********************************/

extern volatile uint8 SD_swStatusTx;
extern volatile uint8 SD_swStatusRx;

#if(SD_TX_SOFTWARE_BUF_ENABLED)
    extern volatile uint8 SD_txBuffer[SD_TX_BUFFER_SIZE];
    extern volatile uint8 SD_txBufferRead;
    extern volatile uint8 SD_txBufferWrite;
    extern volatile uint8 SD_txBufferFull;
#endif /* (SD_TX_SOFTWARE_BUF_ENABLED) */

#if(SD_RX_SOFTWARE_BUF_ENABLED)
    extern volatile uint8 SD_rxBuffer[SD_RX_BUFFER_SIZE];
    extern volatile uint8 SD_rxBufferRead;
    extern volatile uint8 SD_rxBufferWrite;
    extern volatile uint8 SD_rxBufferFull;
#endif /* (SD_RX_SOFTWARE_BUF_ENABLED) */

#endif /* CY_SPIM_PVT_SD_H */


/* [] END OF FILE */
