/*******************************************************************************
* File Name: BL.c
* Version 1.30
*
*  Description:
*   Provides an API for the Bootloader component. The API includes functions
*   for starting boot loading operations, validating the application and
*   jumping to the application.
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "BL_PVT.h"

#include "project.h"
#include <string.h>


/*******************************************************************************
* The Checksum and SizeBytes are forcefully set in code. We then post process
* the hex file from the linker and inject their values then. When the hex file
* is loaded onto the device these two variables should have valid values.
* Because the compiler can do optimizations to remove the constant
* accesses, these should not be accessed directly. Instead, the variables
* CyBtldr_ChecksumAccess & CyBtldr_SizeBytesAccess should be used to get the
* proper values at runtime.
*******************************************************************************/
#if defined(__ARMCC_VERSION) || defined (__GNUC__)
    __attribute__((section (".bootloader"), used))
#elif defined (__ICCARM__)
    #pragma location=".bootloader"
#endif  /* defined(__ARMCC_VERSION) || defined (__GNUC__) */

#if defined(__ARMCC_VERSION) || defined (__GNUC__) || defined (__C51__)
           const uint8  CYCODE BL_Checksum = 0u;
#elif defined (__ICCARM__)
    __root const uint8  CYCODE BL_Checksum = 0u;
#endif  /* defined(__ARMCC_VERSION) || defined (__GNUC__) || defined (__C51__) */
const uint8  CYCODE *BL_ChecksumAccess  = (const uint8  CYCODE *)(&BL_Checksum);

#if defined(__ARMCC_VERSION) || defined (__GNUC__)
    __attribute__((section (".bootloader"), used))
#elif defined (__ICCARM__)
    #pragma location=".bootloader"
#endif  /* defined(__ARMCC_VERSION) || defined (__GNUC__) */

const uint32 CYCODE BL_SizeBytes = 0xFFFFFFFFu;
const uint32 CYCODE *BL_SizeBytesAccess = (const uint32 CYCODE *)(&BL_SizeBytes);


#if(0u != BL_DUAL_APP_BOOTLOADER)
    uint8 BL_activeApp = BL_MD_BTLDB_ACTIVE_NONE;
#else
    #define BL_activeApp      (BL_MD_BTLDB_ACTIVE_0)
#endif  /* (0u != BL_DUAL_APP_BOOTLOADER) */


/***************************************
*     Function Prototypes
***************************************/
static cystatus BL_WritePacket(uint8 status, uint8 buffer[], uint16 size) CYSMALL \
                                    ;

static uint16   BL_CalcPacketChecksum(const uint8 buffer[], uint16 size) CYSMALL \
                                    ;

static void     BL_HostLink(uint8 timeOut) \
                                    ;

static void     BL_LaunchApplication(void) CYSMALL \
                                    ;

#if(!CY_PSOC3)
    /* Implementation for the PSoC 3 resides in a BL_psoc3.a51 file.  */
    static void     BL_LaunchBootloadable(uint32 appAddr);
#endif  /* (!CY_PSOC3) */


/*******************************************************************************
* Function Name: BL_CalcPacketChecksum
********************************************************************************
*
* Summary:
*  This computes the 16 bit checksum for the provided number of bytes contained
*  in the provided buffer
*
* Parameters:
*  buffer:
*     The buffer containing the data to compute the checksum for
*  size:
*     The number of bytes in the buffer to compute the checksum for
*
* Returns:
*  16 bit checksum for the provided data
*
*******************************************************************************/
static uint16 BL_CalcPacketChecksum(const uint8 buffer[], uint16 size) \
                    CYSMALL 
{
    #if(0u != BL_PACKET_CHECKSUM_CRC)

        uint16 CYDATA crc = BL_CRC_CCITT_INITIAL_VALUE;
        uint16 CYDATA tmp;
        uint8  CYDATA i;
        uint16 CYDATA tmpIndex = size;

        if(0u == size)
        {
            crc = ~crc;
        }
        else
        {
            do
            {
                tmp = buffer[tmpIndex - size];

                for (i = 0u; i < 8u; i++)
                {
                    if (0u != ((crc & 0x0001u) ^ (tmp & 0x0001u)))
                    {
                        crc = (crc >> 1u) ^ BL_CRC_CCITT_POLYNOMIAL;
                    }
                    else
                    {
                        crc >>= 1u;
                    }

                    tmp >>= 1u;
                }

                size--;
            }
            while(0u != size);

            crc = ~crc;
            tmp = crc;
            crc = ( uint16 )(crc << 8u) | (tmp >> 8u);
        }

        return(crc);

    #else

        uint16 CYDATA sum = 0u;

        while (size > 0u)
        {
            sum += buffer[size - 1u];
            size--;
        }

        return(( uint16 )1u + ( uint16 )(~sum));

    #endif /* (0u != BL_PACKET_CHECKSUM_CRC) */
}


/*******************************************************************************
* Function Name: BL_Calc8BitSum
********************************************************************************
*
* Summary:
*  This computes the 8 bit sum for the provided number of bytes contained in
*  FLASH (if baseAddr equals CY_FLASH_BASE) or EEPROM (if baseAddr equals
*  CY_EEPROM_BASE).
*
* Parameters:
* baseAddr:
*   CY_FLASH_BASE
*   CY_EEPROM_BASE - applicable only for PSoC 3 / PSoC 5LP devices.
*
*  start:
*     The starting address to start summing data for
*  size:
*     The number of bytes to read and compute the sum for
*
* Returns:
*   8 bit sum for the provided data
*
*******************************************************************************/
uint8 BL_Calc8BitSum(uint32 baseAddr, uint32 start, uint32 size) \
                CYSMALL 
{
    uint8 CYDATA sum = 0u;

    #if(!CY_PSOC4)
        CYASSERT((baseAddr == CY_EEPROM_BASE) || (baseAddr == CY_FLASH_BASE));
    #else
        CYASSERT(baseAddr == CY_FLASH_BASE);
    #endif  /* (!CY_PSOC4) */

    while (size > 0u)
    {
        size--;
        sum += (*((uint8  *)(baseAddr + start + size)));
    }

    return(sum);
}


/*******************************************************************************
* Function Name: BL_Start
********************************************************************************
* Summary:
*  This function is called in order to execute the following algorithm:
*
*  - Identify the active bootloadable application (applicable only to
*    the Multi-application bootloader)
*
*  - Validate the bootloader application (design-time configurable, Bootloader
*    application validation option of the component customizer)
*
*  - Validate the active bootloadable application. If active bootloadable
*    application is not valid, and the other bootloadable application (inactive)
*    is valid, the last one is started.
*
*  - Run a communication subroutine (design-time configurable, Wait for command
*    option of the component customizer)
*
*  - Schedule the bootloadable and reset the device
*
* Parameters:
*  None
*
* Return:
*  This method will never return. It will either load a new application and
*  reset the device or jump directly to the existing application. The CPU is
*  halted, if validation failed when "Bootloader application validation" option
*  is enabled.
*  PSoC 3/PSoC 5: The CPU is halted if Flash initialization fails.
*
* Side Effects:
*  If Bootloader application validation option is enabled and this method
*  determines that the bootloader application itself is corrupt, this method
*  will not return, instead it will simply hang the application.
*
*******************************************************************************/
void BL_Start(void) CYSMALL 
{
    #if(0u != BL_BOOTLOADER_APP_VALIDATION)
        uint8 CYDATA calcedChecksum;
    #endif    /* (0u != BL_BOOTLOADER_APP_VALIDATION) */

    #if(!CY_PSOC4)
        #if(0u != BL_FAST_APP_VALIDATION)
            #if !defined(CY_BOOT_VERSION)

                /* Not required starting from cy_boot 4.20 */
                uint8 CYXDATA BL_flashBuffer[BL_FROW_SIZE];

            #endif /* !defined(CY_BOOT_VERSION) */
        #endif  /* (0u != BL_FAST_APP_VALIDATION) */
    #endif  /* (!CY_PSOC4) */

    cystatus validApp  = CYRET_BAD_DATA;


    /* Identify active bootloadable application */
    #if(0u != BL_DUAL_APP_BOOTLOADER)

        /* Assumes no active bootloadable application. Bootloader is active. */
        BL_activeApp = BL_MD_BTLDB_ACTIVE_NONE;

        /* Bootloadable # A is active */
        if(BL_GetMetadata(BL_GET_BTLDB_ACTIVE, 0u) == BL_MD_BTLDB_IS_ACTIVE)
        {
            /*******************************************************************
            * -----------------------------------------------------------
            * |      | Bootloadable A | Bootloadable B |                |
            * | Case |---------------------------------|     Action     |
            * |      | Active | Valid | Active | Valid |                |
            * |------|--------------------------------------------------|
            * |  9   |    1   |   0   |   0    |   0   | Bootloader     |
            * |  10  |    1   |   0   |   0    |   1   | Bootloadable B |
            * |  11  |    1   |   0   |   1    |   0   | Bootloader     |
            * |  12  |    1   |   0   |   1    |   1   | Bootloadable B |
            * |  13  |    1   |   1   |   0    |   0   | Bootloadable A |
            * |  14  |    1   |   1   |   0    |   1   | Bootloadable A |
            * |  15  |    1   |   1   |   1    |   0   | Bootloadable A |
            * |  16  |    1   |   1   |   1    |   1   | Bootloadable A |
            * -----------------------------------------------------------
            *******************************************************************/
            if (CYRET_SUCCESS == BL_ValidateBootloadable(BL_MD_BTLDB_ACTIVE_0))
            {
                /* Cases # 13,  14, 15, and 16 */
                BL_activeApp = BL_MD_BTLDB_ACTIVE_0;
                validApp = CYRET_SUCCESS;
            }
            else
            {
                if (CYRET_SUCCESS == BL_ValidateBootloadable(BL_MD_BTLDB_ACTIVE_1))
                {
                    /* Cases # 10 and 12 */
                    BL_activeApp = BL_MD_BTLDB_ACTIVE_1;
                    validApp = CYRET_SUCCESS;
                }
            }
        }

        /*  Active bootloadable application is not identified */
        if(BL_activeApp == BL_MD_BTLDB_ACTIVE_NONE)
        {
            /*******************************************************************
            * -----------------------------------------------------------
            * |      | Bootloadable A | Bootloadable B |                |
            * | Case |---------------------------------|     Action     |
            * |      | Active | Valid | Active | Valid |                |
            * |------|--------------------------------------------------|
            * |  1   |    0   |   0   |   0    |   0   | Bootloader     |
            * |  2   |    0   |   0   |   0    |   1   | Bootloader     |
            * |  3   |    0   |   0   |   1    |   0   | Bootloader     |
            * |  4   |    0   |   0   |   1    |   1   | Bootloadable B |
            * |  5   |    0   |   1   |   0    |   0   | Bootloader     |
            * |  6   |    0   |   1   |   0    |   1   | Bootloader     |
            * |  7   |    0   |   1   |   1    |   0   | Bootloadable A |
            * |  8   |    0   |   1   |   1    |   1   | Bootloadable B |
            * -----------------------------------------------------------
            *******************************************************************/
            if (BL_GetMetadata(BL_GET_BTLDB_ACTIVE, 1u) ==
                    BL_MD_BTLDB_IS_ACTIVE)
            {
                /* Cases # 3, 4, 7, and 8 */
                if (CYRET_SUCCESS == BL_ValidateBootloadable(BL_MD_BTLDB_ACTIVE_1))
                {
                    /* Cases # 4 and 8 */
                    BL_activeApp = BL_MD_BTLDB_ACTIVE_1;
                    validApp = CYRET_SUCCESS;
                }
                else
                {
                    if (CYRET_SUCCESS == BL_ValidateBootloadable(BL_MD_BTLDB_ACTIVE_0))
                    {
                        /* Cases # 7 */
                        BL_activeApp = BL_MD_BTLDB_ACTIVE_0;
                        validApp = CYRET_SUCCESS;
                    }
                }
            }
        }
    #else
        if (CYRET_SUCCESS == BL_ValidateBootloadable(BL_MD_BTLDB_ACTIVE_0))
        {
            validApp = CYRET_SUCCESS;
        }
    #endif  /* (0u != BL_DUAL_APP_BOOTLOADER) */


    /* Initialize Flash subsystem for non-PSoC 4 devices */
    #if(!CY_PSOC4)
        #if(0u != BL_FAST_APP_VALIDATION)

            if (CYRET_SUCCESS != CySetTemp())
            {
                CyHalt(0x00u);
            }

            #if !defined(CY_BOOT_VERSION)

                /* Not required with cy_boot 4.20 */
                if (CYRET_SUCCESS != CySetFlashEEBuffer(BL_flashBuffer))
                {
                    CyHalt(0x00u);
                }

            #endif /* !defined(CY_BOOT_VERSION) */
        #endif  /* (0u != BL_FAST_APP_VALIDATION) */
    #endif  /* (CY_PSOC4) */


    /***********************************************************************
    * Bootloader Application Validation
    *
    * Halt the device if:
    *  - A calculated checksum does not match the one stored in the metadata
    *     section.
    *  - There is an invalid pointer to the place where the bootloader
    *    application ends.
    *  - Flash subsystem was not initialized correctly
    ***********************************************************************/
    #if(0u != BL_BOOTLOADER_APP_VALIDATION)

        /* Calculate Bootloader application checksum */
        calcedChecksum = BL_Calc8BitSum(CY_FLASH_BASE,
                BL_MD_BTLDR_ADDR_PTR,
                *BL_SizeBytesAccess - BL_MD_BTLDR_ADDR_PTR);

        /* we included checksum, so remove it */
        calcedChecksum -= *BL_ChecksumAccess;
        calcedChecksum = ( uint8 )1u + ( uint8 )(~calcedChecksum);

        /* Checksum and pointer to bootloader verification */
        if((calcedChecksum != *BL_ChecksumAccess) ||
           (0u == *BL_SizeBytesAccess))
        {
            CyHalt(0x00u);
        }

    #endif  /* (0u != BL_BOOTLOADER_APP_VALIDATION) */


    /***********************************************************************
    * If the active bootloadable application is invalid or a bootloader
    * application is scheduled - do the following:
    *  - schedule the bootloader application to be run after software reset
    *  - Go to the communication subroutine. The HostLink() will wait for
    *    the commands forever.
    ***********************************************************************/
    if ((BL_GET_RUN_TYPE == BL_START_BTLDR) ||
        (CYRET_SUCCESS != validApp))
    {
        BL_SET_RUN_TYPE(0u);

        BL_HostLink(BL_WAIT_FOR_COMMAND_FOREVER);
    }


    /* Go to communication subroutine. Will wait for commands for specifed time */
    #if(0u != BL_WAIT_FOR_COMMAND)

        /* Timeout is in 100s of milliseconds */
        BL_HostLink(BL_WAIT_FOR_COMMAND_TIME);

    #endif  /* (0u != BL_WAIT_FOR_COMMAND) */


    /* Schedule bootloadable application and perform software reset */
    BL_LaunchApplication();
}


/*******************************************************************************
* Function Name: BL_LaunchApplication
********************************************************************************
*
* Summary:
*  Schedules bootloadable application and resets device
*
* Parameters:
*  None
*
* Returns:
*  This method will never return.
*
*******************************************************************************/
static void BL_LaunchApplication(void) CYSMALL 
{
    /* Schedule Bootloadable to start after reset */
    BL_SET_RUN_TYPE(BL_START_APP);

    CySoftwareReset();
}


/*******************************************************************************
* Function Name: BL_Exit
********************************************************************************
*
* Summary:
*  Schedules the specified application and performs software reset to launch
*  a specified application.
*
*  If the specified application is not valid, the Bootloader (the result of the
*  ValidateBootloadable() function execution returns other than CYRET_SUCCESS,
*  the bootloader application is launched.
*
* Parameters:
*  appId: application to be started:
*  BL_EXIT_TO_BTLDR   - Bootloader application will be started on
*                                     software reset.
*  BL_EXIT_TO_BTLDB,
*  BL_EXIT_TO_BTLDB_1 - Bootloadable application # 1 will be
*                                     started on software reset.
*  BL_EXIT_TO_BTLDB_2 - Bootloadable application # 2 will be
*                                     started on software reset. Available only
*                                     if Multi-Application option is enabled in
*                                     the component customizer.
* Returns:
*  This function never returns.
*
*******************************************************************************/
void BL_Exit(uint8 appId) CYSMALL 
{
    if(BL_EXIT_TO_BTLDR == appId)
    {
        BL_SET_RUN_TYPE(0x0u);
    }
    else
    {
        if(CYRET_SUCCESS == BL_ValidateBootloadable(appId))
        {
            /* Set active application in metadata */
            uint8 CYDATA idx;
            for(idx = 0u; idx < BL_MAX_NUM_OF_BTLDB; idx++)
            {
                BL_SetFlashByte((uint32) BL_MD_BTLDB_ACTIVE_OFFSET(idx),
                                              (uint8 )(idx == appId));
            }

        #if(0u != BL_DUAL_APP_BOOTLOADER)
            BL_activeApp = appId;
        #endif /* (0u != BL_DUAL_APP_BOOTLOADER) */

            BL_SET_RUN_TYPE(BL_SCHEDULE_BTLDB);
        }
        else
        {
            BL_SET_RUN_TYPE(0u);
        }
    }

    CySoftwareReset();
}


/*******************************************************************************
* Function Name: CyBtldr_CheckLaunch
********************************************************************************
*
* Summary:
*  This routine checks if the bootloader or the bootloadable application has to
*  be run.  If the application has to be run, it will start executing.
*  If the bootloader is to be run, it will return, so the bootloader can
*  continue starting up.
*
* Parameters:
*  None
*
* Returns:
*  It will not return if it determines that the bootloadable application should
*  be run.
*
*******************************************************************************/
void CyBtldr_CheckLaunch(void) CYSMALL 
{

#if(CY_PSOC4)

    /*******************************************************************************
    * Set cyBtldrRunType to zero in case of non-software reset occurred. This means
    * that bootloader application is scheduled - that is initial clean state. The
    * value of cyBtldrRunType is valid only in case of software reset.
    *******************************************************************************/
    if (0u == (BL_RES_CAUSE_REG & BL_RES_CAUSE_RESET_SOFT))
    {
        cyBtldrRunType = 0u;
    }

#endif /* (CY_PSOC4) */


    if (BL_GET_RUN_TYPE == BL_START_APP)
    {
        BL_SET_RUN_TYPE(0u);

        /*******************************************************************************
        * Indicates that we have told ourselves to jump to the application since we have
        * already told ourselves to jump, we do not do any expensive verification of the
        * application. We just check to make sure that the value at CY_APP_ADDR_ADDRESS
        * is something other than 0.
        *******************************************************************************/
        if(0u != BL_GetMetadata(BL_GET_BTLDB_ADDR, BL_activeApp))
        {
            /* Never return from this method */
            BL_LaunchBootloadable(BL_GetMetadata(BL_GET_BTLDB_ADDR,
                                                                             BL_activeApp));
        }
    }
}


/* Moves argument appAddr (RO) into PC, moving execution to appAddr */
#if defined (__ARMCC_VERSION)

    __asm static void BL_LaunchBootloadable(uint32 appAddr)
    {
        BX  R0
        ALIGN
    }

#elif defined(__GNUC__)

    __attribute__((noinline)) /* Workaround for GCC toolchain bug with inlining */
    __attribute__((naked))
    static void BL_LaunchBootloadable(uint32 appAddr)
    {
        __asm volatile("    BX  R0\n");
    }

#elif defined (__ICCARM__)

    static void BL_LaunchBootloadable(uint32 appAddr)
    {
        __asm volatile("    BX  R0\n");
    }

#endif  /* (__ARMCC_VERSION) */


/*******************************************************************************
* Function Name: BL_ValidateBootloadable
********************************************************************************
* Summary:
*  Performs the bootloadable application validation by calculating the
*  application image checksum and comparing it with the checksum value stored
*  in the Bootloadable Application Checksum field of the metadata section.
*
*  If the Fast bootloadable application validation option is enabled in the
*  component customizer and bootloadable application successfully passes
*  validation, the Bootloadable Application Verification Status field of the
*  metadata section is updated. Refer to the Metadata Layout section for the
*  details.
*
*  If the Fast bootloadable application validation option is enabled and
*  Bootloadable Application Verification Status field of the metadata section
*  claims that bootloadable application is valid, the function returns
*  CYRET_SUCCESS without further checksum calculation.
*
* Parameters:
*  appId:
*  The number of the bootloadable application should be 0 for the normal
*  bootloader and 0 or 1 for the Multi-Application bootloader.
*
* Returns:
*  Returns CYRET_SUCCESS if the specified bootloadable application is valid.
*
*******************************************************************************/
cystatus BL_ValidateBootloadable(uint8 appId) CYSMALL \

    {
        uint32 CYDATA idx;

        uint32 CYDATA end   = BL_FIRST_APP_BYTE(appId) +
                                BL_GetMetadata(BL_GET_BTLDB_LENGTH,
                                                       appId);

        CYBIT         valid = 0u; /* Assume bad flash image */
        uint8  CYDATA calcedChecksum = 0u;


        #if(0u != BL_DUAL_APP_BOOTLOADER)

            if(appId > 1u)
            {
                return(CYRET_BAD_DATA);
            }

        #endif  /* (0u != BL_DUAL_APP_BOOTLOADER) */


        #if(0u != BL_FAST_APP_VALIDATION)


            if(BL_GetMetadata(BL_GET_BTLDB_STATUS, appId) ==
               BL_MD_BTLDB_IS_VERIFIED)
            {
                return(CYRET_SUCCESS);
            }

        #endif  /* (0u != BL_FAST_APP_VALIDATION) */


        /* Calculate checksum of bootloadable image */
        for(idx = BL_FIRST_APP_BYTE(appId); idx < end; ++idx)
        {
            uint8 CYDATA curByte = BL_GET_CODE_BYTE(idx);

            if((curByte != 0u) && (curByte != 0xFFu))
            {
                valid = 1u;
            }

            calcedChecksum += curByte;
        }


        /***************************************************************************
        * We do not compute checksum over the meta data section, so no need to
        * subtract off App Verified or App Active information here like we do when
        * verifying a row.
        ***************************************************************************/


        #if((!CY_PSOC4) && (CYDEV_ECC_ENABLE == 0u))

            /* Add ECC data to checksum */
            idx = ((BL_FIRST_APP_BYTE(appId)) >> 3u);

            /* Flash may run into meta data, so ECC does not use full row */
            end = (end == (CY_FLASH_SIZE - BL_MD_SIZEOF))
                ? (CY_FLASH_SIZE >> 3u)
                : (end >> 3u);

            for (; idx < end; ++idx)
            {
                calcedChecksum += CY_GET_XTND_REG8((volatile uint8 *)(CYDEV_ECC_BASE + idx));
            }

        #endif  /* ((!CY_PSOC4) && (CYDEV_ECC_ENABLE == 0u)) */


        calcedChecksum = ( uint8 )1u + ( uint8 )(~calcedChecksum);


        if((calcedChecksum != BL_GetMetadata(BL_GET_BTLDB_CHECKSUM, appId)) ||
           (0u == valid))
        {
            return(CYRET_BAD_DATA);
        }


        #if(0u != BL_FAST_APP_VALIDATION)
            BL_SetFlashByte((uint32) BL_MD_BTLDB_VERIFIED_OFFSET(appId),
                                          BL_MD_BTLDB_IS_VERIFIED);
        #endif  /* (0u != BL_FAST_APP_VALIDATION) */


        return(CYRET_SUCCESS);
}


/*******************************************************************************
* Function Name: BL_HostLink
********************************************************************************
*
* Summary:
*  Causes the bootloader to attempt to read data being transmitted by the
*  host application.  If data is sent from the host, this establishes the
*  communication interface to process all requests.
*
* Parameters:
*  timeOut:
*   The amount of time to listen for data before giving up. Timeout is
*   measured in 10s of ms.  Use 0 for an infinite wait.
*
* Return:
*   None
*
*******************************************************************************/
static void BL_HostLink(uint8 timeOut) 
{
    uint16    CYDATA numberRead;
    uint16    CYDATA rspSize;
    uint8     CYDATA ackCode;
    uint16    CYDATA pktChecksum;
    cystatus  CYDATA readStat;
    uint16    CYDATA pktSize    = 0u;
    uint16    CYDATA dataOffset = 0u;
    uint8     CYDATA timeOutCnt = 10u;

    #if(0u != BL_FAST_APP_VALIDATION)
        uint8 CYDATA clearedMetaData = 0u;
    #endif  /* (0u != BL_FAST_APP_VALIDATION) */

    CYBIT     communicationState = BL_COMMUNICATION_STATE_IDLE;

    uint8     packetBuffer[BL_SIZEOF_COMMAND_BUFFER];
    uint8     dataBuffer  [BL_SIZEOF_COMMAND_BUFFER];


    #if(!CY_PSOC4)
        #if(0u == BL_FAST_APP_VALIDATION)
            #if !defined(CY_BOOT_VERSION)

                /* Not required with cy_boot 4.20 */
                uint8 CYXDATA BL_flashBuffer[BL_FROW_SIZE];

            #endif /* !defined(CY_BOOT_VERSION) */
        #endif  /* (0u == BL_FAST_APP_VALIDATION) */
    #endif  /* (CY_PSOC4) */



    #if(!CY_PSOC4)
        #if(0u == BL_FAST_APP_VALIDATION)

            /* Initialize Flash subsystem for non-PSoC 4 devices */
            if (CYRET_SUCCESS != CySetTemp())
            {
                CyHalt(0x00u);
            }

            #if !defined(CY_BOOT_VERSION)

                /* Not required with cy_boot 4.20 */
                if (CYRET_SUCCESS != CySetFlashEEBuffer(BL_flashBuffer))
                {
                    CyHalt(0x00u);
                }

            #endif /* !defined(CY_BOOT_VERSION) */
        #endif  /* (0u == BL_FAST_APP_VALIDATION) */
    #endif  /* (CY_PSOC4) */

    /* Initialize communications channel. */
    CyBtldrCommStart();

    /* Enable global interrupts */
    CyGlobalIntEnable;

    do
    {
        ackCode = CYRET_SUCCESS;

        do
        {
            readStat = CyBtldrCommRead(packetBuffer,
                                        BL_SIZEOF_COMMAND_BUFFER,
                                        &numberRead,
                                        (0u == timeOut) ? 0xFFu : timeOut);
            if (0u != timeOut)
            {
                timeOutCnt--;
            }

        } while ( (0u != timeOutCnt) && (readStat != CYRET_SUCCESS) );


        if( readStat != CYRET_SUCCESS )
        {
            continue;
        }

        if((numberRead < BL_MIN_PKT_SIZE) ||
           (packetBuffer[BL_SOP_ADDR] != BL_SOP))
        {
            ackCode = BL_ERR_DATA;
        }
        else
        {
            pktSize = ((uint16)((uint16)packetBuffer[BL_SIZE_ADDR + 1u] << 8u)) |
                               packetBuffer[BL_SIZE_ADDR];

            pktChecksum = ((uint16)((uint16)packetBuffer[BL_CHK_ADDR(pktSize) + 1u] << 8u)) |
                                   packetBuffer[BL_CHK_ADDR(pktSize)];

            if((pktSize + BL_MIN_PKT_SIZE) > numberRead)
            {
                ackCode = BL_ERR_LENGTH;
            }
            else if(packetBuffer[BL_EOP_ADDR(pktSize)] != BL_EOP)
            {
                ackCode = BL_ERR_DATA;
            }
            else if(pktChecksum != BL_CalcPacketChecksum(packetBuffer,
                                                                        pktSize + BL_DATA_ADDR))
            {
                ackCode = BL_ERR_CHECKSUM;
            }
            else
            {
                /* Empty section */
            }
        }

        rspSize = 0u;
        if(ackCode == CYRET_SUCCESS)
        {
            uint8 CYDATA btldrData = packetBuffer[BL_DATA_ADDR];

            ackCode = BL_ERR_DATA;
            switch(packetBuffer[BL_CMD_ADDR])
            {


            /***************************************************************************
            *   Get metadata
            ***************************************************************************/
            #if(0u != BL_CMD_GET_METADATA)

                case BL_COMMAND_GET_METADATA:

                    if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 1u))
                    {
                        if (btldrData >= BL_MAX_NUM_OF_BTLDB)
                        {
                            ackCode = BL_ERR_APP;
                        }
                        else if(CYRET_SUCCESS == BL_ValidateBootloadable(btldrData))
                        {
                            #if(CY_PSOC3)
                                (void) memcpy(&packetBuffer[BL_DATA_ADDR],
                                            ((uint8  CYCODE *) (BL_META_BASE(btldrData))),
                                            BL_GET_METADATA_RESPONSE_SIZE);
                            #else
                                (void) memcpy(&packetBuffer[BL_DATA_ADDR],
                                            (uint8 *) BL_META_BASE(btldrData),
                                            BL_GET_METADATA_RESPONSE_SIZE);
                            #endif  /* (CY_PSOC3) */

                            rspSize = 56u;
                            ackCode = CYRET_SUCCESS;
                        }
                        else
                        {
                            ackCode = BL_ERR_APP;
                        }
                    }
                    break;

            #endif  /* (0u != BL_CMD_GET_METADATA) */


            /***************************************************************************
            *   Verify checksum
            ***************************************************************************/
            case BL_COMMAND_CHECKSUM:

                if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 0u))
                {
                    packetBuffer[BL_DATA_ADDR] =
                            (uint8)(BL_ValidateBootloadable(BL_activeApp) == CYRET_SUCCESS);

                    rspSize = 1u;
                    ackCode = CYRET_SUCCESS;
                }
                break;


            /***************************************************************************
            *   Get flash size
            ***************************************************************************/

            /* Replace BL_NUM_OF_FLASH_ARRAYS with CY_FLASH_NUMBER_ARRAYS */


            #if(0u != BL_CMD_GET_FLASH_SIZE_AVAIL)

                case BL_COMMAND_REPORT_SIZE:

                    /* btldrData - holds flash array ID sent by host */

                    if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 1u))
                    {
                        if(btldrData < CY_FLASH_NUMBER_ARRAYS)
                        {
                            uint16 CYDATA startRow;
                            uint8  CYDATA ArrayIdBtlderEnds;


                            /*******************************************************************************
                            * - For the flash array where bootloader application ends, return the first
                            *   full row after the bootloader application.
                            *
                            * - For the fully occupied flash array, the number of rows in array is returned.
                            *   As there is no space for the bootloadable application in this array.
                            *
                            * - For the arrays next to the occupied array, zero is returned.
                            *   The bootloadable application can written from the their beginning.
                            *
                            *******************************************************************************/
                            ArrayIdBtlderEnds = (uint8)  (*BL_SizeBytesAccess / CY_FLASH_SIZEOF_ARRAY);

                            if (btldrData == ArrayIdBtlderEnds)
                            {
                                startRow = (uint16) (*BL_SizeBytesAccess / CY_FLASH_SIZEOF_ROW) %
                                            BL_NUMBER_OF_ROWS_IN_ARRAY;
                            }
                            else if (btldrData > ArrayIdBtlderEnds)
                            {
                                startRow = BL_FIRST_ROW_IN_ARRAY;
                            }
                            else /* (btldrData < ArrayIdBtlderEnds) */
                            {
                                startRow = BL_NUMBER_OF_ROWS_IN_ARRAY;
                            }

                            packetBuffer[BL_DATA_ADDR]      = LO8(startRow);
                            packetBuffer[BL_DATA_ADDR + 1u] = HI8(startRow);

                            packetBuffer[BL_DATA_ADDR + 2u] =
                                        LO8(BL_NUMBER_OF_ROWS_IN_ARRAY - 1u);

                            packetBuffer[BL_DATA_ADDR + 3u] =
                                        HI8(BL_NUMBER_OF_ROWS_IN_ARRAY - 1u);

                            rspSize = 4u;
                            ackCode = CYRET_SUCCESS;
                        }

                    }
                    break;

            #endif  /* (0u != BL_CMD_GET_FLASH_SIZE_AVAIL) */


            /***************************************************************************
            *   Get application status
            ***************************************************************************/
            #if(0u != BL_DUAL_APP_BOOTLOADER)

                #if(0u != BL_CMD_GET_APP_STATUS_AVAIL)

                    case BL_COMMAND_APP_STATUS:

                        if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 1u))
                        {

                            packetBuffer[BL_DATA_ADDR] =
                                (uint8)BL_ValidateBootloadable(btldrData);

                            packetBuffer[BL_DATA_ADDR + 1u] =
                                (uint8) BL_GetMetadata(BL_GET_BTLDB_ACTIVE, btldrData);

                            rspSize = 2u;
                            ackCode = CYRET_SUCCESS;
                        }
                        break;

                #endif  /* (0u != BL_CMD_GET_APP_STATUS_AVAIL) */

            #endif  /* (0u != BL_DUAL_APP_BOOTLOADER) */


            /***************************************************************************
            *   Program / Erase row
            ***************************************************************************/
            case BL_COMMAND_PROGRAM:

            /* The btldrData variable holds Flash Array ID */

        #if (0u != BL_CMD_ERASE_ROW_AVAIL)

            case BL_COMMAND_ERASE:
                if (BL_COMMAND_ERASE == packetBuffer[BL_CMD_ADDR])
                {
                    if ((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 3u))
                    {
                        #if(!CY_PSOC4)
                            if((btldrData >= BL_FIRST_EE_ARRAYID) &&
                               (btldrData <= BL_LAST_EE_ARRAYID))
                            {
                                /* Size of EEPROM row */
                                dataOffset = CY_EEPROM_SIZEOF_ROW;
                            }
                            else
                            {
                                /* Size of FLASH row (depends on ECC configuration) */
                                dataOffset = BL_FROW_SIZE;
                            }
                        #else
                            /* Size of FLASH row (no ECC available) */
                            dataOffset = BL_FROW_SIZE;
                        #endif  /* (!CY_PSOC4) */

                        #if(CY_PSOC3)
                            (void) memset(dataBuffer, (char8) 0, (int16) dataOffset);
                        #else
                            (void) memset(dataBuffer, 0, (uint32) dataOffset);
                        #endif  /* (CY_PSOC3) */
                    }
                    else
                    {
                        break;
                    }
                }

        #endif  /* (0u != BL_CMD_ERASE_ROW_AVAIL) */


                if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize >= 3u))
                {

                    /* The command may be sent along with the last block of data, to program the row. */
                    #if(CY_PSOC3)
                        (void) memcpy(&dataBuffer[dataOffset],
                                      &packetBuffer[BL_DATA_ADDR + 3u],
                                      (int16) pktSize - 3);
                    #else
                        (void) memcpy(&dataBuffer[dataOffset],
                                      &packetBuffer[BL_DATA_ADDR + 3u],
                                      (uint32) pktSize - 3u);
                    #endif  /* (CY_PSOC3) */

                    dataOffset += (pktSize - 3u);

                    #if(!CY_PSOC4)
                        if((btldrData >= BL_FIRST_EE_ARRAYID) &&
                           (btldrData <= BL_LAST_EE_ARRAYID))
                        {

                            CyEEPROM_Start();

                            /* Size of EEPROM row */
                            pktSize = CY_EEPROM_SIZEOF_ROW;
                        }
                        else
                        {
                            /* Size of FLASH row (depends on ECC configuration) */
                            pktSize = BL_FROW_SIZE;
                        }
                    #else
                        /* Size of FLASH row (no ECC available) */
                        pktSize = BL_FROW_SIZE;
                    #endif  /* (!CY_PSOC4) */


                    /* Check if we have all data to program */
                    if(dataOffset == pktSize)
                    {
                        uint16 row;
                        uint16 firstRow;

                        /* Get FLASH/EEPROM row number inside of the array */
                        dataOffset = ((uint16)((uint16)packetBuffer[BL_DATA_ADDR + 2u] << 8u)) |
                                              packetBuffer[BL_DATA_ADDR + 1u];


                        /* Metadata section resides in Flash (cannot be in EEPROM). */
                        #if(!CY_PSOC4)
                            if(btldrData <= BL_LAST_FLASH_ARRAYID)
                            {
                        #endif  /* (!CY_PSOC4) */


                        /* btldrData  - holds flash array Id sent by host */
                        /* dataOffset - holds flash row Id sent by host   */
                        row = (uint16)(btldrData * BL_NUMBER_OF_ROWS_IN_ARRAY) + dataOffset;


                        /*******************************************************************************
                        * Refuse to write to the row within range of the bootloader application
                        *******************************************************************************/

                        /* First empty flash row after bootloader application */
                        firstRow = (uint16) (*BL_SizeBytesAccess / CYDEV_FLS_ROW_SIZE);
                        if ((*BL_SizeBytesAccess % CYDEV_FLS_ROW_SIZE) != 0u)
                        {
                            firstRow++;
                        }

                        /* Check to see if the row to program will not corrupt the bootloader application */
                        if(row < firstRow)
                        {
                            ackCode = BL_ERR_ROW;
                            dataOffset = 0u;
                            break;
                        }


                        #if(0u != BL_DUAL_APP_BOOTLOADER)

                            if(BL_activeApp < BL_MD_BTLDB_ACTIVE_NONE)
                            {
                                uint16 lastRow;


                                /*******************************************************************************
                                * For the first bootloadable application gets the last flash row occupied by
                                * the bootloader application image:
                                *  ---------------------------------------------------------------------------
                                * | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *  ---------------------------------------------------------------------------
                                * |<--firstRow---|>
                                *
                                * For the second bootloadable application gets the last flash row occupied by
                                * the first bootloadable application:
                                *  ---------------------------------------------------------------------------
                                * | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *  ---------------------------------------------------------------------------
                                * |<-------------firstRow-----------------|>
                                *
                                * Incremented by 1 to get the first available row.
                                *
                                * Note: M1 and M2 stands for the metadata # 1 and metadata # 2, metadata
                                * sections for the 1st and 2nd bootloadable applications.
                                *******************************************************************************/
                                firstRow = (uint16) 1u +
                                    (uint16) BL_GetMetadata(BL_GET_BTLDR_LAST_ROW,
                                                                          BL_activeApp);


                                /*******************************************************************************
                                * The number of flash rows available for the both bootloadable applications:
                                *
                                * First bootloadable application is active:
                                *  ---------------------------------------------------------------------------
                                * | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *  ---------------------------------------------------------------------------
                                *                |<-------------------lastRow -------------------->|
                                *
                                * Second bootloadable application is active:
                                *  ---------------------------------------------------------------------------
                                * | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *  ---------------------------------------------------------------------------
                                *                                         |<-------lastRow-------->|
                                *******************************************************************************/
                                lastRow = (uint16)(CY_FLASH_NUMBER_ROWS -
                                                    BL_NUMBER_OF_METADATA_ROWS -
                                                    firstRow);


                                /*******************************************************************************
                                * The number of flash rows available for the active bootloadable application:
                                *
                                * First bootloadable application is active: the number of flash rows available
                                * for the both bootloadable applications should be divided by 2 - 2 bootloadable
                                * applications should fit there.
                                *
                                * Second bootloadable application is active: the number of flash rows available
                                * for the both bootloadable applications should be divided by 1 - 1 bootloadable
                                * application should fit there.
                                *******************************************************************************/
                                lastRow = lastRow / (BL_NUMBER_OF_BTLDBLE_APPS -
                                                BL_activeApp);


                                /*******************************************************************************
                                * The last row equals to the first row plus the number of rows available for
                                * the each bootloadable application. That gives the flash row number right
                                * beyond the valid range, so we subtract 1.
                                *
                                * First bootloadable application is active:
                                *  ---------------------------------------------------------------------------
                                * | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *  ---------------------------------------------------------------------------
                                * |<----------------lastRow ------------->|
                                *
                                * Second bootloadable application is active:
                                *  ---------------------------------------------------------------------------
                                * | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *  ---------------------------------------------------------------------------
                                * |<-----------------------------lastRow-------------------------->|
                                *******************************************************************************/
                                lastRow = (firstRow + lastRow) - 1u;


                                /*******************************************************************************
                                * 1. Refuse to write row within the range of the active application
                                *
                                *  First bootloadable application is active:
                                *   ---------------------------------------------------------------------------
                                *  | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *   ---------------------------------------------------------------------------
                                *  |<----------------lastRow ------------->|
                                *  |<--firstRow---|>
                                *                 |<-------protected------>|
                                *
                                *  Second bootloadable application is active:
                                *   ---------------------------------------------------------------------------
                                *  | Bootloader   | Bootloadable # 1 |     | Bootloadable # 2 |     | M2 | M1 |
                                *   ---------------------------------------------------------------------------
                                *  |<-------------firstRow-----------------|>
                                *  |<-----------------------------lastRow-------------------------->|
                                *                                          |<-------protected------>|
                                *
                                * 2. Refuse to write to the row that contains metadata of the active
                                *    bootloadable application.
                                *
                                *******************************************************************************/
                                if(((row >= firstRow) && (row <= lastRow)) ||
                                   ((btldrData == BL_MD_FLASH_ARRAY_NUM) &&
                                   (dataOffset == BL_MD_ROW_NUM(BL_activeApp))))
                                {
                                    ackCode = BL_ERR_ACTIVE;
                                    dataOffset = 0u;
                                    break;
                                }
                            }

                        #endif  /* (0u != BL_DUAL_APP_BOOTLOADER) */



                        /*******************************************************************************
                        * Clear row that contains the metadata, when 'Fast bootloadable application
                        * validation' option is enabled.
                        *
                        * If 'Fast bootloadable application validation' option is enabled, the
                        * bootloader only computes the checksum the first time and assumes that it
                        * remains valid in each future startup. The metadata row is cleared because the
                        * bootloadable application might become corrupted during update, while
                        * 'Bootloadable Application Verification Status' field will still report that
                        * application is valid.
                        *******************************************************************************/
                        #if(0u != BL_FAST_APP_VALIDATION)

                            if(0u == clearedMetaData)
                            {
                                /* Metadata section must be filled with zeros */

                                uint8 erase[BL_FROW_SIZE];
                                uint8 BL_notActiveApp;


                                #if(CY_PSOC3)
                                    (void) memset(erase, (char8) 0, (int16) BL_FROW_SIZE);
                                #else
                                    (void) memset(erase, 0, BL_FROW_SIZE);
                                #endif  /* (CY_PSOC3) */


                                #if(0u != BL_DUAL_APP_BOOTLOADER)
                                    if (BL_MD_BTLDB_ACTIVE_0 == BL_activeApp)
                                    {
                                        BL_notActiveApp = BL_MD_BTLDB_ACTIVE_1;
                                    }
                                    else
                                    {
                                        BL_notActiveApp = BL_MD_BTLDB_ACTIVE_0;
                                    }
                                #else
                                    BL_notActiveApp = BL_MD_BTLDB_ACTIVE_0;
                                #endif /* (0u != BL_DUAL_APP_BOOTLOADER) */


                                #if(CY_PSOC4)
                                    (void) CySysFlashWriteRow(
                                            BL_MD_ROW_NUM(BL_notActiveApp),
                                            erase);
                                #else
                                    (void) CyWriteRowFull(
                                            (uint8)  BL_MD_FLASH_ARRAY_NUM,
                                            (uint16) BL_MD_ROW_NUM(BL_notActiveApp),
                                            erase,
                                            BL_FROW_SIZE);
                                #endif  /* (CY_PSOC4) */

                                /* PSoC 5: Do not care about flushing the cache as flash row has been erased. */

                                /* Set up flag that metadata was cleared */
                                clearedMetaData = 1u;
                            }

                        #endif  /* (0u != BL_FAST_APP_VALIDATION) */


                        #if(!CY_PSOC4)
                            }   /* (btldrData <= BL_LAST_FLASH_ARRAYID) */
                        #endif  /* (!CY_PSOC4) */


                        #if(CY_PSOC4)
                            ackCode = (CYRET_SUCCESS != CySysFlashWriteRow((uint32) row, dataBuffer)) \
                                ? BL_ERR_ROW \
                                : CYRET_SUCCESS;
                        #else
                            ackCode = (CYRET_SUCCESS != CyWriteRowFull(btldrData, dataOffset, dataBuffer, pktSize)) \
                                ? BL_ERR_ROW \
                                : CYRET_SUCCESS;
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
                    else
                    {
                        ackCode = BL_ERR_LENGTH;
                    }

                    dataOffset = 0u;
                }
                break;


            /***************************************************************************
            *   Sync bootloader
            ***************************************************************************/
            #if(0u != BL_CMD_SYNC_BOOTLOADER_AVAIL)

            case BL_COMMAND_SYNC:

                if(BL_COMMUNICATION_STATE_ACTIVE == communicationState)
                {
                    /* If something failed the host would send this command to reset the bootloader. */
                    dataOffset = 0u;

                    /* Don't acknowledge the packet, just get ready to accept the next one */
                    continue;
                }
                break;

            #endif  /* (0u != BL_CMD_SYNC_BOOTLOADER_AVAIL) */


            /***************************************************************************
            *   Set an active application
            ***************************************************************************/
            #if(0u != BL_DUAL_APP_BOOTLOADER)

                case BL_COMMAND_APP_ACTIVE:

                    if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 1u))
                    {
                        if(CYRET_SUCCESS == BL_ValidateBootloadable(btldrData))
                        {
                            uint8 CYDATA idx;

                            for(idx = 0u; idx < BL_MAX_NUM_OF_BTLDB; idx++)
                            {
                                BL_SetFlashByte((uint32) BL_MD_BTLDB_ACTIVE_OFFSET(idx),
                                                              (uint8 )(idx == btldrData));
                            }
                            BL_activeApp = btldrData;
                            ackCode = CYRET_SUCCESS;
                        }
                        else
                        {
                            ackCode = BL_ERR_APP;
                        }
                    }
                    break;

            #endif  /* (0u != BL_DUAL_APP_BOOTLOADER) */


            /***************************************************************************
            *   Send data
            ***************************************************************************/
            #if (0u != BL_CMD_SEND_DATA_AVAIL)

                case BL_COMMAND_DATA:

                    if(BL_COMMUNICATION_STATE_ACTIVE == communicationState)
                    {
                        /*  Make sure that dataOffset is valid before copying the data */
                        if((dataOffset + pktSize) <= BL_SIZEOF_COMMAND_BUFFER)
                        {
                            ackCode = CYRET_SUCCESS;

                            #if(CY_PSOC3)
                                (void) memcpy(&dataBuffer[dataOffset],
                                              &packetBuffer[BL_DATA_ADDR],
                                              ( int16 )pktSize);
                            #else
                                (void) memcpy(&dataBuffer[dataOffset],
                                              &packetBuffer[BL_DATA_ADDR],
                                              (uint32) pktSize);
                            #endif  /* (CY_PSOC3) */

                            dataOffset += pktSize;
                        }
                        else
                        {
                            ackCode = BL_ERR_LENGTH;
                        }
                    }

                    break;

            #endif  /* (0u != BL_CMD_SEND_DATA_AVAIL) */


            /***************************************************************************
            *   Enter bootloader
            ***************************************************************************/
            case BL_COMMAND_ENTER:

                if(pktSize == 0u)
                {
                    #if(CY_PSOC3)

                        BL_ENTER CYDATA BtldrVersion =
                            {CYSWAP_ENDIAN32(CYDEV_CHIP_JTAG_ID), CYDEV_CHIP_REV_EXPECT, BL_VERSION};

                    #else

                        BL_ENTER CYDATA BtldrVersion =
                            {CYDEV_CHIP_JTAG_ID, CYDEV_CHIP_REV_EXPECT, BL_VERSION};

                    #endif  /* (CY_PSOC3) */

                    communicationState = BL_COMMUNICATION_STATE_ACTIVE;

                    rspSize = sizeof(BL_ENTER);

                    #if(CY_PSOC3)
                        (void) memcpy(&packetBuffer[BL_DATA_ADDR],
                                      &BtldrVersion,
                                      ( int16 )rspSize);
                    #else
                        (void) memcpy(&packetBuffer[BL_DATA_ADDR],
                                      &BtldrVersion,
                                      (uint32) rspSize);
                    #endif  /* (CY_PSOC3) */

                    ackCode = CYRET_SUCCESS;
                }
                break;


            /***************************************************************************
            *   Verify row
            ***************************************************************************/
            #if (0u != BL_CMD_VERIFY_ROW_AVAIL)

            case BL_COMMAND_VERIFY:

                if((BL_COMMUNICATION_STATE_ACTIVE == communicationState) && (pktSize == 3u))
                {
                    /* Get FLASH/EEPROM row number */
                    uint16 CYDATA rowNum = ((uint16)((uint16)packetBuffer[BL_DATA_ADDR + 2u] << 8u)) |
                                                    packetBuffer[BL_DATA_ADDR + 1u];

                    #if(!CY_PSOC4)

                        uint32 CYDATA rowAddr;
                        uint8 CYDATA checksum;

                        if((btldrData >= BL_FIRST_EE_ARRAYID) &&
                           (btldrData <= BL_LAST_EE_ARRAYID))
                        {
                            /* EEPROM */
                            /* Both PSoC 3 and PSoC 5LP architectures have one EEPROM array. */
                            rowAddr = (uint32)rowNum * CYDEV_EEPROM_ROW_SIZE;

                            checksum = BL_Calc8BitSum(CY_EEPROM_BASE, rowAddr, CYDEV_EEPROM_ROW_SIZE);
                        }
                        else
                        {
                            /* FLASH */
                            rowAddr = ((uint32)btldrData * CYDEV_FLS_SECTOR_SIZE)
                                       + ((uint32)rowNum * CYDEV_FLS_ROW_SIZE);

                            checksum = BL_Calc8BitSum(CY_FLASH_BASE, rowAddr, CYDEV_FLS_ROW_SIZE);
                        }

                    #else

                        uint32 CYDATA rowAddr = ((uint32)btldrData * CYDEV_FLS_SECTOR_SIZE)
                                            + ((uint32)rowNum * CYDEV_FLS_ROW_SIZE);

                        uint8 CYDATA checksum = BL_Calc8BitSum(CY_FLASH_BASE,
                                                                             rowAddr,
                                                                             CYDEV_FLS_ROW_SIZE);

                    #endif  /* (!CY_PSOC4) */


                    /* Calculate checksum on data from ECC */
                    #if(!CY_PSOC4) && (CYDEV_ECC_ENABLE == 0u)

                        if(btldrData <= BL_LAST_FLASH_ARRAYID)
                        {
                            uint16 CYDATA tmpIndex;

                            rowAddr = CYDEV_ECC_BASE + ((uint32)btldrData * (CYDEV_FLS_SECTOR_SIZE / 8u))
                                        + ((uint32)rowNum * CYDEV_ECC_ROW_SIZE);

                            for(tmpIndex = 0u; tmpIndex < CYDEV_ECC_ROW_SIZE; tmpIndex++)
                            {
                                checksum += CY_GET_XTND_REG8((uint8 CYFAR *)(rowAddr + tmpIndex));
                            }
                        }

                    #endif  /* (!CY_PSOC4) && (CYDEV_ECC_ENABLE == 0u) */


                    /*******************************************************************************
                    * App Verified & App Active are information that is updated in Flash at runtime.
                    * Remove these items from the checksum to allow the host to verify everything is
                    * correct.
                     ******************************************************************************/
                    if((BL_MD_FLASH_ARRAY_NUM == btldrData) &&
                       (BL_CONTAIN_METADATA(rowNum)))
                    {

                        checksum -= (uint8)BL_GetMetadata(BL_GET_BTLDB_ACTIVE,
                                                                 BL_GET_APP_ID(rowNum));

                        checksum -= (uint8)BL_GetMetadata(BL_GET_BTLDB_STATUS,
                                                                 BL_GET_APP_ID(rowNum));
                    }

                    packetBuffer[BL_DATA_ADDR] = (uint8)1u + (uint8)(~checksum);
                    ackCode = CYRET_SUCCESS;
                    rspSize = 1u;
                }
                break;

            #endif /* (0u != BL_CMD_VERIFY_ROW_AVAIL) */


            /***************************************************************************
            *   Exit bootloader
            ***************************************************************************/
            case BL_COMMAND_EXIT:

                if(CYRET_SUCCESS == BL_ValidateBootloadable(BL_activeApp))
                {
                    BL_SET_RUN_TYPE(BL_SCHEDULE_BTLDB);
                }

                CySoftwareReset();

                /* Will never get here */
                break;


            /***************************************************************************
            *   Unsupported command
            ***************************************************************************/
            default:
                ackCode = BL_ERR_CMD;
                break;
            }
        }

        /* Reply with acknowledge or not acknowledge packet */
        (void) BL_WritePacket(ackCode, packetBuffer, rspSize);

    } while ((0u == timeOut) || (BL_COMMUNICATION_STATE_ACTIVE == communicationState));
}


/*******************************************************************************
* Function Name: BL_WritePacket
********************************************************************************
*
* Summary:
*  Creates a bootloader response packet and transmits it back to the bootloader
*  host application over the already established communications protocol.
*
* Parameters:
*  status:
*      The status code to pass back as the second byte of the packet
*  buffer:
*      The buffer containing the data portion of the packet
*  size:
*      The number of bytes contained within the buffer to pass back
*
* Return:
*   CYRET_SUCCESS if successful. Any other non-zero value if failure occurred.
*
*******************************************************************************/
static cystatus BL_WritePacket(uint8 status, uint8 buffer[], uint16 size) CYSMALL \
                                            
{
    uint16 CYDATA checksum;

    /* Start of packet. */
    buffer[BL_SOP_ADDR]      = BL_SOP;
    buffer[BL_CMD_ADDR]      = status;
    buffer[BL_SIZE_ADDR]     = LO8(size);
    buffer[BL_SIZE_ADDR + 1u] = HI8(size);

    /* Compute checksum. */
    checksum = BL_CalcPacketChecksum(buffer, size + BL_DATA_ADDR);

    buffer[BL_CHK_ADDR(size)]     = LO8(checksum);
    buffer[BL_CHK_ADDR(1u + size)] = HI8(checksum);
    buffer[BL_EOP_ADDR(size)]     = BL_EOP;

    /* Start packet transmit. */
    return(CyBtldrCommWrite(buffer, size + BL_MIN_PKT_SIZE, &size, 150u));
}


/*******************************************************************************
* Function Name: BL_SetFlashByte
********************************************************************************
*
* Summary:
*  Writes a byte to the specified Flash memory location.
*
* Parameters:
*  address:
*      The address in Flash memory where data will be written
*
*  runType:
*      Byte to be written
*
* Return:
*  None
*
*******************************************************************************/
void BL_SetFlashByte(uint32 address, uint8 runType) 
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

    for(idx = 0u; idx < CYDEV_FLS_ROW_SIZE; idx++)
    {
        rowData[idx] = BL_GET_CODE_BYTE(baseAddr + idx);
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


/*******************************************************************************
* Function Name: BL_GetMetadata
********************************************************************************
*
* Summary:
*  Returns the value of the specified field of the metadata section.
*
* Parameters:
*  field:
*   The field to get data from:
*   BL_GET_BTLDB_CHECKSUM    - Bootloadable Application Checksum
*   BL_GET_BTLDB_ADDR        - Bootloadable Application Start
*                                            Routine Address
*   BL_GET_BTLDR_LAST_ROW    - Bootloader Last Flash Row
*   BL_GET_BTLDB_LENGTH      - Bootloadable Application Length
*   BL_GET_BTLDB_ACTIVE      - Active Bootloadable Application
*   BL_GET_BTLDB_STATUS      - Bootloadable Application
*                                            Verification Status
*   BL_GET_BTLDR_APP_VERSION - Bootloader Application Version
*   BL_GET_BTLDB_APP_VERSION - Bootloadable Application Version
*   BL_GET_BTLDB_APP_ID      - Bootloadable Application ID
*   BL_GET_BTLDB_APP_CUST_ID - Bootloadable Application Custom ID
*
*  appId:
*   Number of the bootlodable application. Should be 0 for the normal
*   bootloader and 0 or 1 for the Multi-Application bootloader.
*
* Return:
*  The value of the specified field of the specified application.
*
*******************************************************************************/
uint32 BL_GetMetadata(uint8 field, uint8 appId)
{
    uint32 fieldPtr;
    uint8  fieldSize = 2u;
    uint32 result = 0u;

    switch (field)
    {
    case BL_GET_BTLDB_CHECKSUM:
        fieldPtr  = BL_MD_BTLDB_CHECKSUM_OFFSET(appId);
        fieldSize = 1u;
        break;

    case BL_GET_BTLDB_ADDR:
        fieldPtr  = BL_MD_BTLDB_ADDR_OFFSET(appId);
    #if(!CY_PSOC3)
        fieldSize = 4u;
    #endif  /* (!CY_PSOC3) */
        break;

    case BL_GET_BTLDR_LAST_ROW:
        fieldPtr  = BL_MD_BTLDR_LAST_ROW_OFFSET(appId);
        break;

    case BL_GET_BTLDB_LENGTH:
        fieldPtr  = BL_MD_BTLDB_LENGTH_OFFSET(appId);
    #if(!CY_PSOC3)
        fieldSize = 4u;
    #endif  /* (!CY_PSOC3) */
        break;

    case BL_GET_BTLDB_ACTIVE:
        fieldPtr  = BL_MD_BTLDB_ACTIVE_OFFSET(appId);
        fieldSize = 1u;
        break;

    case BL_GET_BTLDB_STATUS:
        fieldPtr  = BL_MD_BTLDB_VERIFIED_OFFSET(appId);
        fieldSize = 1u;
        break;

    case BL_GET_BTLDB_APP_VERSION:
        fieldPtr  = BL_MD_BTLDB_APP_VERSION_OFFSET(appId);
        break;

    case BL_GET_BTLDR_APP_VERSION:
        fieldPtr  = BL_MD_BTLDR_APP_VERSION_OFFSET(appId);
        break;

    case BL_GET_BTLDB_APP_ID:
        fieldPtr  = BL_MD_BTLDB_APP_ID_OFFSET(appId);
        break;

    case BL_GET_BTLDB_APP_CUST_ID:
        fieldPtr  = BL_MD_BTLDB_APP_CUST_ID_OFFSET(appId);
        fieldSize = 4u;
        break;

    default:
        /* Should never be here */
        CYASSERT(0u != 0u);
        fieldPtr  = 0u;
        break;
    }


    if (1u == fieldSize)
    {
        result =  (uint32) CY_GET_XTND_REG8((volatile uint8 *)fieldPtr);
    }

    #if(CY_PSOC3)   /* Big-endian */

        if (2u == fieldSize)
        {
            result =  (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 1u));
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr     )) <<  8u;
        }

        if (4u == fieldSize)
        {
            result =  (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 3u));
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 2u)) <<  8u;
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 1u)) << 16u;
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr     )) << 24u;
        }

    #else   /* PSoC 4 and PSoC 5: Little-endian */

        if (2u == fieldSize)
        {
            result =  (uint32) CY_GET_XTND_REG8((volatile uint8 *) (fieldPtr     ));
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *) (fieldPtr + 1u)) <<  8u;
        }

        if (4u == fieldSize)
        {
            result =  (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr     ));
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 1u)) <<  8u;
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 2u)) << 16u;
            result |= (uint32) CY_GET_XTND_REG8((volatile uint8 *)(fieldPtr + 3u)) << 24u;
        }

    #endif /* (CY_PSOC3) */

    return (result);
}


/* [] END OF FILE */
