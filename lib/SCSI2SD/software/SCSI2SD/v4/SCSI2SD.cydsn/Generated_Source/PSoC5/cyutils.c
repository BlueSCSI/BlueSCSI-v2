/*******************************************************************************
* FILENAME: cyutils.c
* Version 4.20
*
*  Description:
*   CyUtils provides a function to handle 24-bit value writes.
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "cytypes.h"

#if (!CY_PSOC3)

    /***************************************************************************
    * Function Name: CySetReg24
    ****************************************************************************
    *
    * Summary:
    *  Writes a 24-bit value to the specified register.
    *
    * Parameters:
    *  addr : the address where data must be written.
    *  value: the data that must be written.
    *
    * Return:
    *  None
    *
    * Reentrant:
    *  No
    *
    ***************************************************************************/
    void CySetReg24(uint32 volatile * addr, uint32 value)
    {
        uint8 volatile *tmpAddr;

        tmpAddr = (uint8 volatile *) addr;

        tmpAddr[0u] = (uint8) value;
        tmpAddr[1u] = (uint8) (value >> 8u);
        tmpAddr[2u] = (uint8) (value >> 16u);
    }


    #if(CY_PSOC4)

        /***************************************************************************
        * Function Name: CyGetReg24
        ****************************************************************************
        *
        * Summary:
        *  Reads the 24-bit value from the specified register.
        *
        * Parameters:
        *  addr : the address where data must be read.
        *
        * Return:
        *  None
        *
        * Reentrant:
        *  No
        *
        ***************************************************************************/
        uint32 CyGetReg24(uint32 const volatile * addr)
        {
            uint8 const volatile *tmpAddr;
            uint32 value;

            tmpAddr = (uint8 const volatile *) addr;

            value  =  (uint32) tmpAddr[0u];
            value |= ((uint32) tmpAddr[1u] << 8u );
            value |= ((uint32) tmpAddr[2u] << 16u);

            return(value);
        }

    #endif  /*(CY_PSOC4)*/

#endif  /* (!CY_PSOC3) */


/* [] END OF FILE */
