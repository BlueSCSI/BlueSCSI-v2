/*******************************************************************************
* File Name: Bootloadable_1.c
* Version 1.30
*
*  Description:
*   Provides an API for the Bootloadable application. The API includes a
*   single function for starting the bootloader.
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "Bootloadable_1.h"


/*******************************************************************************
* Function Name: Bootloadable_1_Load
********************************************************************************
* Summary:
*  Begins the bootloading algorithm downloading a new ACD image from the host.
*
* Parameters:
*  None
*
* Returns:
*  This method will never return. It will load a new application and reset
*  the device.
*
*******************************************************************************/
void Bootloadable_1_Load(void) 
{
    /* Schedule Bootloader to start after reset */
    Bootloadable_1_SET_RUN_TYPE(Bootloadable_1_START_BTLDR);

    CySoftwareReset();
}


/*******************************************************************************
* The following code is OBSOLETE and must not be used.
*******************************************************************************/
void Bootloadable_1_SetFlashByte(uint32 address, uint8 runType) 
{
    uint32 flsAddr = address - CYDEV_FLASH_BASE;
    uint8  rowData[CYDEV_FLS_ROW_SIZE];

    #if !(CY_PSOC4)
        uint8 arrayId = ( uint8 )(flsAddr / CYDEV_FLS_SECTOR_SIZE);
    #endif  /* !(CY_PSOC4) */

    #if (CY_PSOC4)
        uint16 rowNum = ( uint16 )(flsAddr / CYDEV_FLS_ROW_SIZE);
    #else
        uint16 rowNum = ( uint16 )((flsAddr % CYDEV_FLS_SECTOR_SIZE) / CYDEV_FLS_ROW_SIZE);
    #endif  /* (CY_PSOC4) */

    uint32 baseAddr = address - (address % CYDEV_FLS_ROW_SIZE);
    uint16 idx;


    for (idx = 0u; idx < CYDEV_FLS_ROW_SIZE; idx++)
    {
        rowData[idx] = Bootloadable_1_GET_CODE_DATA(baseAddr + idx);
    }
    rowData[address % CYDEV_FLS_ROW_SIZE] = runType;

    #if(CY_PSOC4)
        (void) CySysFlashWriteRow((uint32) rowNum, rowData);
    #else
        (void) CyWriteRowData(arrayId, rowNum, rowData);
    #endif  /* (CY_PSOC4) */

    #if(CY_PSOC5)
        /***************************************************************************
        * When writing Flash, data in the instruction cache can become stale.
        * Therefore, the cache data does not correlate to the data just written to
        * Flash. A call to CyFlushCache() is required to invalidate the data in the
        * cache and force fresh information to be loaded from Flash.
        ***************************************************************************/
        CyFlushCache();
    #endif /* (CY_PSOC5) */
}


/* [] END OF FILE */
