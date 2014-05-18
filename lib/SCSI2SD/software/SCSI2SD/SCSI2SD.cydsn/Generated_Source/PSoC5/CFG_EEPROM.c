/*******************************************************************************
* File Name: CFG_EEPROM.c
* Version 2.10
*
* Description:
*  Provides the source code to the API for the EEPROM component.
*
********************************************************************************
* Copyright 2008-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "CFG_EEPROM.h"


#if (CY_PSOC3 || CY_PSOC5LP)

    /*******************************************************************************
    * Function Name: CFG_EEPROM_Enable
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
    void CFG_EEPROM_Enable(void) 
    {
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
        /* Enable the EEPROM */
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
        /* Disable EEPROM */
        CyEEPROM_Stop();
    }

#endif /* (CY_PSOC3 || CY_PSOC5LP) */


/*******************************************************************************
* Function Name: CFG_EEPROM_EraseSector
********************************************************************************
*
* Summary:
*  Erases a sector of memory. This function blocks until the operation is
*  complete.
*
* Parameters:
*  sectorNumber:  Sector number to erase.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter sectorNumber out of range.
*  CYRET_LOCKED, if the spc is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_EraseSector(uint8 sectorNumber) 
{
    cystatus status;

    /* Start the SPC */
    CySpcStart();

    if(sectorNumber < (uint8) CY_EEPROM_NUMBER_ARRAYS)
    {
        /* See if we can get the SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            #if(CY_PSOC5A)

                /* Plan for failure */
                status = CYRET_UNKNOWN;

                /* Command to load a row of data */
                if(CySpcLoadRow(CY_SPC_FIRST_EE_ARRAYID, 0, CYDEV_EEPROM_ROW_SIZE) == CYRET_STARTED)
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
                }

                /* Command to erase a sector */
                if(status == CYRET_SUCCESS)
                {

            #endif /* (CY_PSOC5A) */

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

            #if(CY_PSOC5A)

                }
                else
                {
                    status = CYRET_UNKNOWN;
                }

            #endif /* (CY_PSOC5A) */

                /* Unlock the SPC so someone else can use it. */
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
*  Writes a row, CYDEV_EEPROM_ROW_SIZE of data to the EEPROM. This is
*  a blocking call. It will not return until the function succeeds or fails.
*
* Parameters:
*  rowData:  Address of the data to write to the EEPROM.
*  rowNumber:  EEPROM row number to program.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter rowNumber out of range.
*  CYRET_LOCKED, if the spc is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_Write(const uint8 * rowData, uint8 rowNumber) 
{
    cystatus status;

    /* Start the SPC */
    CySpcStart();

    if(rowNumber < (uint8) CY_EEPROM_NUMBER_ROWS)
    {
        /* See if we can get the SPC. */
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
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0],
                    dieTemperature[1]) == CYRET_STARTED)
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

            /* Unlock the SPC so someone else can use it. */
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
*  Starts the SPC write function. This function does not block, it returns
*  once the command has begun the SPC write function. This function must be used
*  in combination with CFG_EEPROM_QueryWrite(). Once this function has
*  been called the SPC will be locked until CFG_EEPROM_QueryWrite()
*  returns CYRET_SUCCESS.
*
* Parameters:
*  rowData:  Address of buffer containing a row of data to write to the EEPROM.
*  rowNumber:  EEPROM row number to program.
*
* Return:
*  CYRET_STARTED, if the spc command to write was successfuly started.
*  CYRET_BAD_PARAM, if the parameter rowNumber out of range.
*  CYRET_LOCKED, if the spc is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_StartWrite(const uint8 * rowData, uint8 rowNumber) \

{
    cystatus status;

    if(rowNumber < (uint8) CY_EEPROM_NUMBER_ROWS)
    {
        /* See if we can get the SPC. */
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
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0],
                    dieTemperature[1]) == CYRET_STARTED)
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
* Function Name: CFG_EEPROM_QueryWrite
********************************************************************************
*
* Summary:
*  Checks the state of write to EEPROM. This function must be called until
*  the return value is not CYRET_STARTED.
*
* Parameters:
*  None
*
* Return:
*  CYRET_STARTED, if the spc command is still processing.
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_QueryWrite(void) 
{
    cystatus status;

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

        /* Unlock the SPC so someone else can use it. */
        CySpcUnlock();
    }
    else
    {
        status = CYRET_STARTED;
    }

    return(status);
}


/*******************************************************************************
* Function Name: CFG_EEPROM_ByteWrite
********************************************************************************
*
* Summary:
*  Writes a byte of data to the EEPROM. This is a blocking call. It will not
*  return until the function succeeds or fails.
*
* Parameters:
*  dataByte:  Byte of data to write to the EEPROM.
*  rowNumber:  EEPROM row number to program.
*  byteNumber:  Byte number within the row to program.
*
* Return:
*  CYRET_SUCCESS, if the operation was successful.
*  CYRET_BAD_PARAM, if the parameter rowNumber or byteNumber out of range.
*  CYRET_LOCKED, if the spc is being used.
*  CYRET_UNKNOWN, if there was an SPC error.
*
*******************************************************************************/
cystatus CFG_EEPROM_ByteWrite(uint8 dataByte, uint8 rowNumber, uint8 byteNumber) \

{
    cystatus status;

    /* Start the SPC */
    CySpcStart();

    if((rowNumber < (uint8) CY_EEPROM_NUMBER_ROWS) && (byteNumber < (uint8) SIZEOF_EEPROM_ROW))
    {
        /* See if we can get the SPC. */
        if(CySpcLock() == CYRET_SUCCESS)
        {
            /* Plan for failure */
            status = CYRET_UNKNOWN;

            /* Command to load a byte of data */
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
                    if(CySpcWriteRow(CY_SPC_FIRST_EE_ARRAYID, (uint16)rowNumber, dieTemperature[0],
                    dieTemperature[1]) == CYRET_STARTED)
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

            /* Unlock the SPC so someone else can use it. */
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
