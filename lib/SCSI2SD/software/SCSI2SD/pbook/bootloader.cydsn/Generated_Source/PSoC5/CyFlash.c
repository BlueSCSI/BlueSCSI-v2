/*******************************************************************************
* File Name: CyFlash.c
* Version 4.0
*
*  Description:
*   Provides an API for the FLASH/EEPROM.
*
*  Note:
*   This code is endian agnostic.
*
*  Note:
*   Documentation of the API's in this file is located in the
*   System Reference Guide provided with PSoC Creator.
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation. All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "CyFlash.h"


/*******************************************************************************
* Holds die temperature, updated by CySetTemp(). Used for flash writting.
* The first byte is the sign of the temperature (0 = negative, 1 = positive).
* The second byte is the magnitude.
*******************************************************************************/
uint8 dieTemperature[CY_FLASH_DIE_TEMP_DATA_SIZE];

#if(CYDEV_ECC_ENABLE == 0)
    static uint8 * rowBuffer = 0;
#endif  /* (CYDEV_ECC_ENABLE == 0) */


static cystatus CySetTempInt(void);


/*******************************************************************************
* Function Name: CyFlash_Start
********************************************************************************
*
* Summary:
*  Enable the Flash.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyFlash_Start(void) 
{
    /* Active Power Mode */
    *CY_FLASH_PM_ACT_EEFLASH_PTR |= CY_FLASH_PM_FLASH_MASK;

    /* Standby Power Mode */
    *CY_FLASH_PM_ALTACT_EEFLASH_PTR |= CY_FLASH_PM_FLASH_MASK;

    CyDelayUs(CY_FLASH_EE_STARTUP_DELAY);
}


/*******************************************************************************
* Function Name: CyFlash_Stop
********************************************************************************
*
* Summary:
*  Disable the Flash.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Side Effects:
*  This setting is ignored as long as the CPU is currently running.  This will
*  only take effect when the CPU is later disabled.
*
*******************************************************************************/
void CyFlash_Stop(void) 
{
    /* Active Power Mode */
    *CY_FLASH_PM_ACT_EEFLASH_PTR &= ((uint8)(~CY_FLASH_PM_FLASH_MASK));

    /* Standby Power Mode */
    *CY_FLASH_PM_ALTACT_EEFLASH_PTR &= ((uint8)(~CY_FLASH_PM_FLASH_MASK));
}


/*******************************************************************************
* Function Name: CySetTempInt
********************************************************************************
*
* Summary:
*  Sends a command to the SPC to read the die temperature. Sets a global value
*  used by the Write functions. This function must be called once before
*  executing a series of Flash writing functions.
*
* Parameters:
*  None
*
* Return:
*  status:
*   CYRET_SUCCESS - if successful
*   CYRET_LOCKED  - if Flash writing already in use
*   CYRET_UNKNOWN - if there was an SPC error
*
*******************************************************************************/
static cystatus CySetTempInt(void) 
{
    cystatus status;

    /* Make sure SPC is powered */
    CySpcStart();

    /* Plan for failure. */
    status = CYRET_UNKNOWN;

    if(CySpcLock() == CYRET_SUCCESS)
    {
        /* Write the command. */
        if(CYRET_STARTED == CySpcGetTemp(CY_TEMP_NUMBER_OF_SAMPLES))
        {
            do
            {
                if(CySpcReadData(dieTemperature, CY_FLASH_DIE_TEMP_DATA_SIZE) == CY_FLASH_DIE_TEMP_DATA_SIZE)
                {
                    status = CYRET_SUCCESS;

                    while(CY_SPC_BUSY)
                    {
                        /* Spin until idle. */
                        CyDelayUs(1u);
                    }
                    break;
                }

            } while(CY_SPC_BUSY);
        }

        CySpcUnlock();
    }
    else
    {
        status = CYRET_LOCKED;
    }

    return (status);
}


/*******************************************************************************
* Function Name: CySetTemp
********************************************************************************
*
* Summary:
*  This is a wraparound for CySetTempInt(). It is used to return second
*  successful read of temperature value.
*
* Parameters:
*  None
*
* Return:
*  status:
*   CYRET_SUCCESS if successful.
*   CYRET_LOCKED  if Flash writing already in use
*   CYRET_UNKNOWN if there was an SPC error.
*
*  uint8 dieTemperature[2]:
*   Holds die temperature for the flash writting algorithm. The first byte is
*   the sign of the temperature (0 = negative, 1 = positive). The second byte is
*   the magnitude.
*
*******************************************************************************/
cystatus CySetTemp(void) 
{
    cystatus status = CySetTempInt();

    if(status == CYRET_SUCCESS)
    {
        status = CySetTempInt();
    }

    return (status);
}


/*******************************************************************************
* Function Name: CySetFlashEEBuffer
********************************************************************************
*
* Summary:
*  Sets the user supplied temporary buffer to store SPC data while performing
*  flash and EEPROM commands. This buffer is only necessary when Flash ECC is
*  disabled.
*
* Parameters:
*  buffer:
*   Address of block of memory to store temporary memory. The size of the block
*   of memory is CYDEV_FLS_ROW_SIZE + CYDEV_ECC_ROW_SIZE.
*
* Return:
*  status:
*   CYRET_SUCCESS if successful.
*   CYRET_BAD_PARAM if the buffer is NULL
*
*******************************************************************************/
cystatus CySetFlashEEBuffer(uint8 * buffer) 
{
    cystatus status = CYRET_SUCCESS;

    CySpcStart();

    #if(CYDEV_ECC_ENABLE == 0)

        if(NULL == buffer)
        {
            status = CYRET_BAD_PARAM;
        }
        else if(CySpcLock() != CYRET_SUCCESS)
        {
            status = CYRET_LOCKED;
        }
        else
        {
            rowBuffer = buffer;
            CySpcUnlock();
        }

    #else

        /* To supress the warning */
        buffer = buffer;

    #endif  /* (CYDEV_ECC_ENABLE == 0u) */

    return(status);
}


#if(CYDEV_ECC_ENABLE == 1)

    /*******************************************************************************
    * Function Name: CyWriteRowData
    ********************************************************************************
    *
    * Summary:
    *  Sends a command to the SPC to load and program a row of data in
    *  Flash or EEPROM.
    *
    * Parameters:
    *  arrayID:    ID of the array to write.
    *   The type of write, Flash or EEPROM, is determined from the array ID.
    *   The arrays in the part are sequential starting at the first ID for the
    *   specific memory type. The array ID for the Flash memory lasts from 0x00 to
    *   0x3F and for the EEPROM memory it lasts from 0x40 to 0x7F.
    *  rowAddress: rowAddress of flash row to program.
    *  rowData:    Array of bytes to write.
    *
    * Return:
    *  status:
    *   CYRET_SUCCESS if successful.
    *   CYRET_LOCKED if the SPC is already in use.
    *   CYRET_CANCELED if command not accepted
    *   CYRET_UNKNOWN if there was an SPC error.
    *
    *******************************************************************************/
    cystatus CyWriteRowData(uint8 arrayId, uint16 rowAddress, const uint8 * rowData) 
    {
        uint16 rowSize;
        cystatus status;

        rowSize = (arrayId > CY_SPC_LAST_FLASH_ARRAYID) ? CYDEV_EEPROM_ROW_SIZE : CYDEV_FLS_ROW_SIZE;
        status = CyWriteRowFull(arrayId, rowAddress, rowData, rowSize);

        return(status);
    }

#else

    /*******************************************************************************
    * Function Name: CyWriteRowData
    ********************************************************************************
    *
    * Summary:
    *   Sends a command to the SPC to load and program a row of data in
    *   Flash or EEPROM.
    *
    * Parameters:
    *  arrayID      : ID of the array to write.
    *   The type of write, Flash or EEPROM, is determined from the array ID.
    *   The arrays in the part are sequential starting at the first ID for the
    *   specific memory type. The array ID for the Flash memory lasts from 0x00 to
    *   0x3F and for the EEPROM memory it lasts from 0x40 to 0x7F.
    *  rowAddress   : rowAddress of flash row to program.
    *  rowData      : Array of bytes to write.
    *
    * Return:
    *  status:
    *   CYRET_SUCCESS if successful.
    *   CYRET_LOCKED if the SPC is already in use.
    *   CYRET_CANCELED if command not accepted
    *   CYRET_UNKNOWN if there was an SPC error.
    *
    *******************************************************************************/
    cystatus CyWriteRowData(uint8 arrayId, uint16 rowAddress, const uint8 * rowData) 
    {
        uint8 i;
        uint32 offset;
        uint16 rowSize;
        cystatus status;

        /* Check whether rowBuffer pointer has been initialized by CySetFlashEEBuffer() */
        if(NULL != rowBuffer)
        {
            if(arrayId > CY_SPC_LAST_FLASH_ARRAYID)
            {
                rowSize = CYDEV_EEPROM_ROW_SIZE;
            }
            else
            {
                rowSize = CYDEV_FLS_ROW_SIZE + CYDEV_ECC_ROW_SIZE;

                /* Save the ECC area. */
                offset = CYDEV_ECC_BASE +
                        ((uint32)arrayId * CYDEV_ECC_SECTOR_SIZE) +
                        ((uint32)rowAddress * CYDEV_ECC_ROW_SIZE);

                for(i = 0u; i < CYDEV_ECC_ROW_SIZE; i++)
                {
                    *(rowBuffer + CYDEV_FLS_ROW_SIZE + i) = CY_GET_XTND_REG8((void CYFAR *)(offset + i));
                }
            }

            /* Copy the rowdata to the temporary buffer. */
        #if(CY_PSOC3)
            (void) memcpy((void *) rowBuffer, (void *)((uint32) rowData), (int16) CYDEV_FLS_ROW_SIZE);
        #else
            (void) memcpy((void *) rowBuffer, (const void *) rowData, CYDEV_FLS_ROW_SIZE);
        #endif  /* (CY_PSOC3) */

            status = CyWriteRowFull(arrayId, rowAddress, rowBuffer, rowSize);
        }
        else
        {
            status = CYRET_UNKNOWN;
        }

        return(status);
    }

#endif /* (CYDEV_ECC_ENABLE == 0u) */


#if ((CYDEV_ECC_ENABLE == 0u) && (CYDEV_CONFIGURATION_ECC == 0u))

    /*******************************************************************************
    * Function Name: CyWriteRowConfig
    ********************************************************************************
    *
    * Summary:
    *  Sends a command to the SPC to load and program a row of config data in flash.
    *  This function is only valid for Flash array IDs (not for EEPROM).
    *
    * Parameters:
    *  arrayId:      ID of the array to write
    *   The arrays in the part are sequential starting at the first ID for the
    *   specific memory type. The array ID for the Flash memory lasts
    *   from 0x00 to 0x3F.
    *  rowAddress:   Address of the sector to erase.
    *  rowECC:       Array of bytes to write.
    *
    * Return:
    *  status:
    *   CYRET_SUCCESS if successful.
    *   CYRET_LOCKED if the SPC is already in use.
    *   CYRET_CANCELED if command not accepted
    *   CYRET_UNKNOWN if there was an SPC error.
    *
    *******************************************************************************/
    cystatus CyWriteRowConfig(uint8 arrayId, uint16 rowAddress, const uint8 * rowECC)\
    
    {
        uint32 offset;
        uint16 i;
        cystatus status;

        /* Check whether rowBuffer pointer has been initialized by CySetFlashEEBuffer() */
        if(NULL != rowBuffer)
        {
            /* Read the existing flash data. */
            offset = ((uint32)arrayId * CYDEV_FLS_SECTOR_SIZE) +
                     ((uint32)rowAddress * CYDEV_FLS_ROW_SIZE);

            #if (CYDEV_FLS_BASE != 0u)
                offset += CYDEV_FLS_BASE;
            #endif  /* (CYDEV_FLS_BASE != 0u) */

            for (i = 0u; i < CYDEV_FLS_ROW_SIZE; i++)
            {
                rowBuffer[i] = CY_GET_XTND_REG8((void CYFAR *)(offset + i));
            }

            #if(CY_PSOC3)
                (void) memcpy((void *)&rowBuffer[CYDEV_FLS_ROW_SIZE],
                              (void *)(uint32)rowECC,
                              (int16)CYDEV_ECC_ROW_SIZE);
            #else
                (void) memcpy((void *)&rowBuffer[CYDEV_FLS_ROW_SIZE],
                              (const void *)rowECC,
                              CYDEV_ECC_ROW_SIZE);
            #endif  /* (CY_PSOC3) */

            status = CyWriteRowFull(arrayId, rowAddress, rowBuffer, CYDEV_FLS_ROW_SIZE + CYDEV_ECC_ROW_SIZE);
        }
        else
        {
            status = CYRET_UNKNOWN;
        }

        return (status);
    }

#endif  /* ((CYDEV_ECC_ENABLE == 0u) && (CYDEV_CONFIGURATION_ECC == 0u)) */



/*******************************************************************************
* Function Name: CyWriteRowFull
********************************************************************************
* Summary:
*  Sends a command to the SPC to load and program a row of data in flash.
*  rowData array is expected to contain Flash and ECC data if needed.
*
* Parameters:
*  arrayId:    FLASH or EEPROM array id.
*  rowData:    Pointer to a row of data to write.
*  rowNumber:  Zero based number of the row.
*  rowSize:    Size of the row.
*
* Return:
*  CYRET_SUCCESS if successful.
*  CYRET_LOCKED if the SPC is already in use.
*  CYRET_CANCELED if command not accepted
*  CYRET_UNKNOWN if there was an SPC error.
*
*******************************************************************************/
cystatus CyWriteRowFull(uint8 arrayId, uint16 rowNumber, const uint8* rowData, uint16 rowSize) \
        
{
    cystatus status;

    if(CySpcLock() == CYRET_SUCCESS)
    {
        /* Load row data into SPC internal latch */
        status = CySpcLoadRow(arrayId, rowData, rowSize);

        if(CYRET_STARTED == status)
        {
            while(CY_SPC_BUSY)
            {
                /* Wait for SPC to finish and get SPC status */
                CyDelayUs(1u);
            }

            /* Hide SPC status */
            if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
            {
                status = CYRET_SUCCESS;
            }
            else
            {
                status = CYRET_UNKNOWN;
            }

            if(CYRET_SUCCESS == status)
            {
                /* Erase and program flash with the data from SPC interval latch */
                status = CySpcWriteRow(arrayId, rowNumber, dieTemperature[0u], dieTemperature[1u]);

                if(CYRET_STARTED == status)
                {
                    while(CY_SPC_BUSY)
                    {
                        /* Wait for SPC to finish and get SPC status */
                        CyDelayUs(1u);
                    }

                    /* Hide SPC status */
                    if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                    {
                        status = CYRET_SUCCESS;
                    }
                    else
                    {
                        status = CYRET_UNKNOWN;
                    }
                }
            }

        }

        CySpcUnlock();
    }
    else
    {
        status = CYRET_LOCKED;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CyFlash_SetWaitCycles
********************************************************************************
*
* Summary:
*  Sets the number of clock cycles the cache will wait before it samples data
*  coming back from Flash. This function must be called before increasing CPU
*  clock frequency. It can optionally be called after lowering CPU clock
*  frequency in order to improve CPU performance.
*
* Parameters:
*  uint8 freq:
*   Frequency of operation in Megahertz.
*
* Return:
*  None
*
*******************************************************************************/
void CyFlash_SetWaitCycles(uint8 freq) 
{
    uint8 interruptState;

    /* Save current global interrupt enable and disable it */
    interruptState = CyEnterCriticalSection();

    /***************************************************************************
    * The number of clock cycles the cache will wait before it samples data
    * coming back from Flash must be equal or greater to to the CPU frequency
    * outlined in clock cycles.
    ***************************************************************************/

    #if (CY_PSOC3)

        if (freq <= 22u)
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_LESSER_OR_EQUAL_22MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }
        else if (freq <= 44u)
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_LESSER_OR_EQUAL_44MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }
        else
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_GREATER_44MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }

    #endif  /* (CY_PSOC3) */


    #if (CY_PSOC5)

        if (freq <= 16u)
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_LESSER_OR_EQUAL_16MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }
        else if (freq <= 33u)
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_LESSER_OR_EQUAL_33MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }
        else if (freq <= 50u)
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_LESSER_OR_EQUAL_50MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }
        else
        {
            *CY_FLASH_CONTROL_PTR = ((*CY_FLASH_CONTROL_PTR & ((uint8)(~CY_FLASH_CYCLES_MASK))) |
                ((uint8)(CY_FLASH_GREATER_51MHz << CY_FLASH_CYCLES_MASK_SHIFT)));
        }

    #endif  /* (CY_PSOC5) */

    /* Restore global interrupt enable state */
    CyExitCriticalSection(interruptState);
}


/*******************************************************************************
* Function Name: CyEEPROM_Start
********************************************************************************
*
* Summary:
*  Enable the EEPROM.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyEEPROM_Start(void) 
{
    /* Active Power Mode */
    *CY_FLASH_PM_ACT_EEFLASH_PTR |= CY_FLASH_PM_EE_MASK;

    /* Standby Power Mode */
    *CY_FLASH_PM_ALTACT_EEFLASH_PTR |= CY_FLASH_PM_EE_MASK;
}


/*******************************************************************************
* Function Name: CyEEPROM_Stop
********************************************************************************
*
* Summary:
*  Disable the EEPROM.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyEEPROM_Stop (void) 
{
    /* Active Power Mode */
    *CY_FLASH_PM_ACT_EEFLASH_PTR &= ((uint8)(~CY_FLASH_PM_EE_MASK));

    /* Standby Power Mode */
    *CY_FLASH_PM_ALTACT_EEFLASH_PTR &= ((uint8)(~CY_FLASH_PM_EE_MASK));
}


/*******************************************************************************
* Function Name: CyEEPROM_ReadReserve
********************************************************************************
*
* Summary:
*  Request access to the EEPROM for reading and wait until access is available.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyEEPROM_ReadReserve(void) 
{
    /* Make a request for PHUB to have access */
    *CY_FLASH_EE_SCR_PTR |= CY_FLASH_EE_SCR_AHB_EE_REQ;

    while (0u == (*CY_FLASH_EE_SCR_PTR & CY_FLASH_EE_SCR_AHB_EE_ACK))
    {
        /* Wait for acknowledgement from PHUB */
    }
}


/*******************************************************************************
* Function Name: CyEEPROM_ReadRelease
********************************************************************************
*
* Summary:
*  Release the read reservation of the EEPROM.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyEEPROM_ReadRelease(void) 
{
    *CY_FLASH_EE_SCR_PTR |= 0x00u;
}


/* [] END OF FILE */
