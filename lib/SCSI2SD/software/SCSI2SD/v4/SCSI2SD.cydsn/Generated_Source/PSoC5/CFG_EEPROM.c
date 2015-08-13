/*******************************************************************************
* File Name: CFG_EEPROM.c
* Version 3.0
*
*  Description:
*   Provides the source code to the API for the EEPROM component.
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "CFG_EEPROM.h"


/*******************************************************************************
* Function Name: CFG_EEPROM_Enable
********************************************************************************
*
* Summary:
*  Enable the EEPROM block. Also reads the temperature and stores it for
*  future writes.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CFG_EEPROM_Enable(void) 
{
    /* Read temperature value */
    (void)CySetTemp();

    /* Start EEPROM block */
    CyEEPROM_Start();
}


/*******************************************************************************
* Function Name: CFG_EEPROM_Start
********************************************************************************
*
* Summary:
*  Starts EEPROM.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CFG_EEPROM_Start(void) 
{
    CFG_EEPROM_Enable();
}


/*******************************************************************************
* Function Name: CFG_EEPROM_Stop
********************************************************************************
*
* Summary:
*  Stops and powers down EEPROM.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CFG_EEPROM_Stop (void) 
{
    /* Stop and power down EEPROM block */
    CyEEPROM_Stop();
}


/*******************************************************************************
* Function Name: CFG_EEPROM_WriteByte
********************************************************************************
*
* Summary:
*  Writes a byte of data to the EEPROM. This function blocks until
*  the function is complete. For a reliable write procedure to occur you should
*  call CFG_EEPROM_UpdateTemperature() function if the temperature of the
*  silicon has been changed for more than 10C since the component was started.
*
* Parameters:
*  dataByte:  The byte of data to write to the EEPROM
*  address:   The address of data to be written. The maximum address is dependent
*             on the EEPROM size.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter sectorNumber is out of range.
*  CYRET_LOCKED, if the SPC is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_WriteByte(uint8 dataByte, uint16 address) 
{
    cystatus status;
    uint16 rowNumber;
    uint16 byteNumber;
    
    CySpcStart();

    if (address < CY_EEPROM_SIZE)
    {
        rowNumber = address/(uint16)CY_EEPROM_SIZEOF_ROW;
        byteNumber = address - (rowNumber * ((uint16)CY_EEPROM_SIZEOF_ROW));
        if(CYRET_SUCCESS == CySpcLock())
        {
            status = CySpcLoadMultiByte(CY_SPC_FIRST_EE_ARRAYID, byteNumber, &dataByte, \
                                                                    CFG_EEPROM_SPC_BYTE_WRITE_SIZE);
            if (CYRET_STARTED == status)
            {
                /* Plan for failure */
                status = CYRET_UNKNOWN;

                while(CY_SPC_BUSY)
                {
                    /* Wait until SPC becomes idle */
                }

                if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                {
                    status = CYRET_SUCCESS;
                }
                /* Command to erase and program the row. */
                if(CYRET_SUCCESS == status)
                {
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0u],
                    dieTemperature[1u]) == CYRET_STARTED)
                    {
                        /* Plan for failure */
                        status = CYRET_UNKNOWN;

                        while(CY_SPC_BUSY)
                        {
                            /* Wait until SPC becomes idle */
                        }

                        /* SPC is idle now */
                        if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                        {
                            status = CYRET_SUCCESS;
                        }
                    }
                    else
                    {
                        status = CYRET_UNKNOWN;
                    }
                }
                else
                {
                    status = CYRET_UNKNOWN;
                }
            }
            else
            {
                if (CYRET_BAD_PARAM != status)
                {
                    status = CYRET_UNKNOWN;
                }
            }
            CySpcUnlock();
        }
        else
        {
            status = CYRET_LOCKED;
        }
    }
    else
    {
        status = CYRET_BAD_PARAM;
    }


    return (status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_ReadByte
********************************************************************************
*
* Summary:
*  Reads and returns a byte of data from the on-chip EEPROM memory. Although
*  the data is present in the CPU memory space, this function provides an
*  intuitive user interface, addressing the EEPROM memory as a separate block with
*  the first EERPOM byte address equal to 0x0000.
*
* Parameters:
*  address:   The address of data to be read. The maximum address is limited by the
*             size of the EEPROM array on a specific device.
*
* Return:
*  Data located at an address.
*
*******************************************************************************/
uint8 CFG_EEPROM_ReadByte(uint16 address) 
{
    uint8 retByte;
    uint8 interruptState;

    interruptState = CyEnterCriticalSection();

    /* Request access to EEPROM for reading.
    This is needed to reserve PHUB for read operation from EEPROM */
    CyEEPROM_ReadReserve();
    
    retByte = *((reg8 *) (CYDEV_EE_BASE + address));

    /* Release EEPROM array */
    CyEEPROM_ReadRelease();
    
    CyExitCriticalSection(interruptState);

    return (retByte);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_UpdateTemperature
********************************************************************************
*
* Summary:
*  Updates and stores the temperature value. This function should be called
*  before EEPROM writes if the temperature may have been changed by more than
*  10 degrees Celsius.
*
* Parameters:
*  None
*
* Return:
*  Status of operation, 0 if operation complete, non-zero value if error
*  was detected.
*
*******************************************************************************/
uint8 CFG_EEPROM_UpdateTemperature(void) 
{
    return ((uint8)CySetTemp());
}


/*******************************************************************************
* Function Name: CFG_EEPROM_EraseSector
********************************************************************************
*
* Summary:
*  Erase an EEPROM sector (64 rows). This function blocks until the erase
*  operation is complete. Using this API helps to erase the EEPROM sector at
*  a time. This is faster than using individual writes but affects a cycle
*  recourse of the whole EEPROM row.
*
* Parameters:
*  sectorNumber:  The sector number to erase.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter sectorNumber is out of range.
*  CYRET_LOCKED, if the SPC is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_EraseSector(uint8 sectorNumber) 
{
    cystatus status;
    
    CySpcStart();

    if(sectorNumber < (uint8) CFG_EEPROM_SECTORS_NUMBER)
    {
        /* See if we can get SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            if(CySpcEraseSector(CY_SPC_FIRST_EE_ARRAYID, sectorNumber) == CYRET_STARTED)
            {
                /* Plan for failure */
                status = CYRET_UNKNOWN;

                while(CY_SPC_BUSY)
                {
                    /* Wait until SPC becomes idle */
                }

                /* SPC is idle now */
                if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                {
                    status = CYRET_SUCCESS;
                }
            }
            else
            {
                status = CYRET_UNKNOWN;
            }

            /* Unlock SPC so that someone else can use it. */
            CySpcUnlock();
        }
        else
        {
            status = CYRET_LOCKED;
        }
    }
    else
    {
        status = CYRET_BAD_PARAM;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_Write
********************************************************************************
*
* Summary:
*  Writes a row (16 bytes) of data to the EEPROM. This function blocks until
*  the write operation is complete. Compared to functions that write one byte,
*  this function allows writing a whole row (16 bytes) at a time. For
*  a reliable write procedure to occur you should call the
*  CFG_EEPROM_UpdateTemperature() function if the temperature of the
*  silicon has changed for more than 10C since component was started.
*
* Parameters:
*  rowData:    The address of the data to write to the EEPROM.
*  rowNumber:  The row number to write.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter rowNumber is out of range.
*  CYRET_LOCKED, if the SPC is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_Write(const uint8 * rowData, uint8 rowNumber) 
{
    cystatus status;
    
    CySpcStart();

    if(rowNumber < (uint8) CY_EEPROM_NUMBER_ROWS)
    {
        /* See if we can get SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            /* Plan for failure */
            status = CYRET_UNKNOWN;

            /* Command to load a row of data */
            if(CySpcLoadRow(CY_SPC_FIRST_EE_ARRAYID, rowData, CYDEV_EEPROM_ROW_SIZE) == CYRET_STARTED)
            {
                while(CY_SPC_BUSY)
                {
                    /* Wait until SPC becomes idle */
                }

                /* SPC is idle now */
                if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                {
                    status = CYRET_SUCCESS;
                }

                /* Command to erase and program the row. */
                if(status == CYRET_SUCCESS)
                {
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0u],
                    dieTemperature[1u]) == CYRET_STARTED)
                    {
                        /* Plan for failure */
                        status = CYRET_UNKNOWN;

                        while(CY_SPC_BUSY)
                        {
                            /* Wait until SPC becomes idle */
                        }

                        /* SPC is idle now */
                        if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                        {
                            status = CYRET_SUCCESS;
                        }
                    }
                    else
                    {
                        status = CYRET_UNKNOWN;
                    }
                }
                else
                {
                    status = CYRET_UNKNOWN;
                }
            }

            /* Unlock SPC so that someone else can use it. */
            CySpcUnlock();
        }
        else
        {
            status = CYRET_LOCKED;
        }
    }
    else
    {
        status = CYRET_BAD_PARAM;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_StartWrite
********************************************************************************
*
* Summary:
*  Starts a write of a row (16 bytes) of data to the EEPROM.
*  This function does not block. The function returns once the SPC has begun
*  writing the data. This function must be used in combination with
*  CFG_EEPROM_Query(). CFG_EEPROM_Query() must be called
*  until it returns a status other than CYRET_STARTED. That indicates that the
*  write has completed. Until CFG_EEPROM_Query() detects that
*  the write is complete, the SPC is marked as locked to prevent another
*  SPC operation from being performed. For a reliable write procedure to occur
*  you should call CFG_EEPROM_UpdateTemperature() API if the temperature
*  of the silicon has changed for more than 10C since component was started.
*
* Parameters:
*  rowData:    The address of the data to write to the EEPROM.
*  rowNumber:  The row number to write.
*
* Return:
*  CYRET_STARTED, if the SPC command to write was successfully started.
*  CYRET_BAD_PARAM, if the parameter rowNumber is out of range.
*  CYRET_LOCKED, if the SPC is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
* Side effects:
*  After calling this API, the device should not be powered down, reset or switched
*  to low power modes until EEPROM operation is complete. 
*  Ignoring this recommendation may lead to data corruption or silicon
*  unexpected behavior.
*
*******************************************************************************/
cystatus CFG_EEPROM_StartWrite(const uint8 * rowData, uint8 rowNumber) \

{
    cystatus status;
    
    CySpcStart();

    if(rowNumber < (uint8) CY_EEPROM_NUMBER_ROWS)
    {
        /* See if we can get SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            /* Plan for failure */
            status = CYRET_UNKNOWN;

            /* Command to load a row of data */
            if(CySpcLoadRow(CY_SPC_FIRST_EE_ARRAYID, rowData, CYDEV_EEPROM_ROW_SIZE) == CYRET_STARTED)
            {
                while(CY_SPC_BUSY)
                {
                    /* Wait until SPC becomes idle */
                }

                /* SPC is idle now */
                if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                {
                    status = CYRET_SUCCESS;
                }

                /* Command to erase and program the row. */
                if(status == CYRET_SUCCESS)
                {
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0u],
                    dieTemperature[1u]) == CYRET_STARTED)
                    {
                        status = CYRET_STARTED;
                    }
                    else
                    {
                        status = CYRET_UNKNOWN;
                    }
                }
                else
                {
                    status = CYRET_UNKNOWN;
                }
            }
        }
        else
        {
            status = CYRET_LOCKED;
        }
    }
    else
    {
        status = CYRET_BAD_PARAM;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_StartErase
********************************************************************************
*
* Summary:
*  Starts the EEPROM sector erase. This function does not block.
*  The function returns once the SPC has begun writing the data. This function
*  must be used in combination with CFG_EEPROM_Query().
*  CFG_EEPROM_Query() must be called until it returns a status
*  other than CYRET_STARTED. That indicates the erase has been completed.
*  Until CFG_EEPROM_Query() detects that the erase is
*  complete, the SPC is marked as locked to prevent another SPC operation
*  from being performed.
*
* Parameters:
*  sectorNumber:  The sector number to erase.
*
* Return:
*  CYRET_STARTED, if the SPC command to erase was successfully started.
*  CYRET_BAD_PARAM, if the parameter sectorNumber is out of range.
*  CYRET_LOCKED, if the SPC is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
* Side effects:
*  After calling this API, the device should not be powered down, reset or switched
*  to low power modes until EEPROM operation is complete.
*  Ignoring this recommendation may lead to data corruption or silicon
*  unexpected behavior.
*
*******************************************************************************/
cystatus CFG_EEPROM_StartErase(uint8 sectorNumber) 
{
    cystatus status;
    
    CySpcStart();

    if(sectorNumber < (uint8) CY_EEPROM_NUMBER_ARRAYS)
    {
        /* See if we can get SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            /* Plan for failure */
            status = CYRET_UNKNOWN;

            /* Command to load a row of data */
            if(CySpcEraseSector(CY_SPC_FIRST_EE_ARRAYID, sectorNumber) == CYRET_STARTED)
            {
                status = CYRET_SUCCESS;
            }
        }
        else
        {
            status = CYRET_LOCKED;
        }
    }
    else
    {
        status = CYRET_BAD_PARAM;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_Query
********************************************************************************
*
* Summary:
*  Checks the status of an earlier call to CFG_EEPROM_StartWrite() or
*  CFG_EEPROM_StartErase().
*  This function must be called until it returns a value other than
*  CYRET_STARTED. Once that occurs, the write or erase has been completed and
*  the SPC is unlocked.
*
* Parameters:
*  None
*
* Return:
*  CYRET_STARTED, if the SPC command is still processing.
*  CYRET_SUCCESS, if the operation was completed successfully.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_Query(void) 
{
    cystatus status;
    
    CySpcStart();

    /* Check if SPC is idle */
    if(CY_SPC_IDLE)
    {
        /* SPC is idle now */
        if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
        {
            status = CYRET_SUCCESS;
        }
        else
        {
            status = CYRET_UNKNOWN;
        }

        /* Unlock SPC so that someone else can use it. */
        CySpcUnlock();
    }
    else
    {
        status = CYRET_STARTED;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_ByteWritePos
********************************************************************************
*
* Summary:
*  Writes a byte of data to the EEPROM. This is a blocking call. It will not
*  return until the write operation succeeds or fails.
*
* Parameters:
*  dataByte:   The byte of data to write to the EEPROM.
*  rowNumber:  The EEPROM row number to program.
*  byteNumber: The byte number within the row to program.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter rowNumber or byteNumber is out of range.
*  CYRET_LOCKED, if the SPC is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_ByteWritePos(uint8 dataByte, uint8 rowNumber, uint8 byteNumber) \

{
    cystatus status;

    /* Start SPC */
    CySpcStart();

    if((rowNumber < (uint8) CY_EEPROM_NUMBER_ROWS) && (byteNumber < (uint8) SIZEOF_EEPROM_ROW))
    {
        /* See if we can get SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            /* Plan for failure */
            status = CYRET_UNKNOWN;

            /* Command to load byte of data */
            if(CySpcLoadMultiByte(CY_SPC_FIRST_EE_ARRAYID, (uint16)byteNumber, &dataByte,\
                                                                CFG_EEPROM_SPC_BYTE_WRITE_SIZE) == CYRET_STARTED)
            {
                while(CY_SPC_BUSY)
                {
                    /* Wait until SPC becomes idle */
                }

                /* SPC is idle now */
                if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                {
                    status = CYRET_SUCCESS;
                }

                /* Command to erase and program the row. */
                if(status == CYRET_SUCCESS)
                {
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0u],
                    dieTemperature[1u]) == CYRET_STARTED)
                    {
                        /* Plan for failure */
                        status = CYRET_UNKNOWN;

                        while(CY_SPC_BUSY)
                        {
                            /* Wait until SPC becomes idle */
                        }

                        /* SPC is idle now */
                        if(CY_SPC_STATUS_SUCCESS == CY_SPC_READ_STATUS)
                        {
                            status = CYRET_SUCCESS;
                        }
                    }
                    else
                    {
                        status = CYRET_UNKNOWN;
                    }
                }
                else
                {
                    status = CYRET_UNKNOWN;
                }
            }

            /* Unlock SPC so that someone else can use it. */
            CySpcUnlock();
        }
        else
        {
            status = CYRET_LOCKED;
        }
    }
    else
    {
        status = CYRET_BAD_PARAM;
    }

    return(status);
}


/* [] END OF FILE */
