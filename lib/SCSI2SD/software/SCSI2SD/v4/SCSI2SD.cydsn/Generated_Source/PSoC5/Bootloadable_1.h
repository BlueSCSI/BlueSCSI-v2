/*******************************************************************************
* File Name: Bootloadable_1.h
* Version 1.30
*
*  Description:
*   Provides an API for the Bootloadable application. The API includes a
*   single function for starting bootloader.
*
********************************************************************************
* Copyright 2008-2014, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/


#ifndef CY_BOOTLOADABLE_Bootloadable_1_H
#define CY_BOOTLOADABLE_Bootloadable_1_H

#include "cydevice_trm.h"
#include "CyFlash.h"


/* Check to see if required defines such as CY_PSOC5LP are available */
/* They are defined starting with cy_boot v3.0 */
#if !defined (CY_PSOC5LP)
    #error Component Bootloadable_v1_30 requires cy_boot v3.0 or later
#endif /* !defined (CY_PSOC5LP) */


#ifndef CYDEV_FLASH_BASE
    #define CYDEV_FLASH_BASE                            CYDEV_FLS_BASE
    #define CYDEV_FLASH_SIZE                            CYDEV_FLS_SIZE
#endif /* CYDEV_FLASH_BASE */

#if(CY_PSOC3)
    #define Bootloadable_1_GET_CODE_DATA(idx)         (*((uint8  CYCODE *) (idx)))
#else
    #define Bootloadable_1_GET_CODE_DATA(idx)         (*((uint8  *)(CYDEV_FLASH_BASE + (idx))))
#endif /* (CY_PSOC3) */


/*******************************************************************************
* This variable is used by Bootloader/Bootloadable components to schedule what
* application will be started after software reset.
*******************************************************************************/
#if (CY_PSOC4)
    #if defined(__ARMCC_VERSION)
        __attribute__ ((section(".bootloaderruntype"), zero_init))
    #elif defined (__GNUC__)
        __attribute__ ((section(".bootloaderruntype")))
   #elif defined (__ICCARM__)
        #pragma location=".bootloaderruntype"
    #endif  /* defined(__ARMCC_VERSION) */
    extern volatile uint32 cyBtldrRunType;
#endif  /* (CY_PSOC4) */


/*******************************************************************************
* Get the reason of the device reset
*******************************************************************************/
#if(CY_PSOC4)
    #define Bootloadable_1_RES_CAUSE_RESET_SOFT   (0x10u)
    #define Bootloadable_1_GET_RUN_TYPE           \
                        (((CY_GET_REG32(CYREG_RES_CAUSE) & Bootloadable_1_RES_CAUSE_RESET_SOFT) > 0u) \
                            ? (cyBtldrRunType) \
                            : 0u)
#else
    #define Bootloadable_1_GET_RUN_TYPE           (CY_GET_REG8(CYREG_RESET_SR0) & \
                                                    (Bootloadable_1_START_BTLDR | Bootloadable_1_START_APP))
#endif  /* (CY_PSOC4) */


/*******************************************************************************
* Schedule Bootloader/Bootloadable to be run after software reset
*******************************************************************************/
#if(CY_PSOC4)
    #define Bootloadable_1_SET_RUN_TYPE(x)        (cyBtldrRunType = (x))
#else
    #define Bootloadable_1_SET_RUN_TYPE(x)        CY_SET_REG8(CYREG_RESET_SR0, (x))
#endif  /* (CY_PSOC4) */



/***************************************
*     Function Prototypes
***************************************/
extern void Bootloadable_1_Load(void) ;


/*******************************************************************************
* The following code is OBSOLETE and must not be used starting from version 1.10
*******************************************************************************/
#define CYBTDLR_SET_RUN_TYPE(x)     Bootloadable_1_SET_RUN_TYPE(x)


/*******************************************************************************
* The following code is OBSOLETE and must not be used starting from version 1.20
*******************************************************************************/
#define Bootloadable_1_START_APP                      (0x80u)
#define Bootloadable_1_START_BTLDR                    (0x40u)
#define Bootloadable_1_META_DATA_SIZE                 (64u)
#define Bootloadable_1_META_APP_CHECKSUM_OFFSET       (0u)

#if(CY_PSOC3)

    #define Bootloadable_1_APP_ADDRESS                    uint16
    #define Bootloadable_1_GET_CODE_WORD(idx)             (*((uint32 CYCODE *) (idx)))

    /* Offset by 2 from 32 bit start because only need 16 bits */
    #define Bootloadable_1_META_APP_ADDR_OFFSET           (3u)
    #define Bootloadable_1_META_APP_BL_LAST_ROW_OFFSET    (7u)
    #define Bootloadable_1_META_APP_BYTE_LEN_OFFSET       (11u)
    #define Bootloadable_1_META_APP_RUN_TYPE_OFFSET       (15u)

#else

    #define Bootloadable_1_APP_ADDRESS                    uint32
    #define Bootloadable_1_GET_CODE_WORD(idx)             (*((uint32 *)(CYDEV_FLASH_BASE + (idx))))

    #define Bootloadable_1_META_APP_ADDR_OFFSET           (1u)
    #define Bootloadable_1_META_APP_BL_LAST_ROW_OFFSET    (5u)
    #define Bootloadable_1_META_APP_BYTE_LEN_OFFSET       (9u)
    #define Bootloadable_1_META_APP_RUN_TYPE_OFFSET       (13u)

#endif /* (CY_PSOC3) */

#define Bootloadable_1_META_APP_ACTIVE_OFFSET             (16u)
#define Bootloadable_1_META_APP_VERIFIED_OFFSET           (17u)

#define Bootloadable_1_META_APP_BL_BUILD_VER_OFFSET       (18u)
#define Bootloadable_1_META_APP_ID_OFFSET                 (20u)
#define Bootloadable_1_META_APP_VER_OFFSET                (22u)
#define Bootloadable_1_META_APP_CUST_ID_OFFSET            (24u)

#define Bootloadable_1_SetFlashRunType(runType)           \
                        Bootloadable_1_SetFlashByte(Bootloadable_1_MD_APP_RUN_ADDR(0), (runType))


/*******************************************************************************
* The following code is OBSOLETE and must not be used.
*
* If the obsoleted macro definitions intended for use in the application use the
* following scheme, redefine your own versions of these definitions:
*    #ifdef <OBSOLETED_DEFINE>
*        #undef  <OBSOLETED_DEFINE>
*        #define <OBSOLETED_DEFINE>      (<New Value>)
*    #endif
*
* Note: Redefine obsoleted macro definitions with caution. They might still be
*       used in the application and their modification might lead to unexpected
*       consequences.
*******************************************************************************/
void Bootloadable_1_SetFlashByte(uint32 address, uint8 runType) ;
#if(CY_PSOC4)
    #define Bootloadable_1_SOFTWARE_RESET         CySoftwareReset()
#else
    #define Bootloadable_1_SOFTWARE_RESET         CySoftwareReset()
#endif  /* (CY_PSOC4) */

#if(CY_PSOC4)
    extern uint8 appRunType;
#endif  /* (CY_PSOC4) */


#endif /* CY_BOOTLOADABLE_Bootloadable_1_H */


/* [] END OF FILE */
