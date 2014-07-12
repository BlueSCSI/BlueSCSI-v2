/*******************************************************************************
* File Name: Cm3Start.c
* Version 4.0
*
*  Description:
*  Startup code for the ARM CM3.
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation. All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include <limits.h>
#include "cydevice_trm.h"
#include "cytypes.h"
#include "cyfitter_cfg.h"
#include "CyLib.h"
#include "CyDmac.h"
#include "cyfitter.h"

#define CY_NUM_INTERRUPTS           (32u)
#define CY_NUM_VECTORS              (CYINT_IRQ_BASE + CY_NUM_INTERRUPTS)
#define CY_NUM_ROM_VECTORS          (4u)
#define CY_NVIC_APINT_PTR           ((reg32 *) CYREG_NVIC_APPLN_INTR)
#define CY_NVIC_CFG_CTRL_PTR        ((reg32 *) CYREG_NVIC_CFG_CONTROL)
#define CY_NVIC_APINT_PRIGROUP_3_5  (0x00000400u)  /* Priority group 3.5 split */
#define CY_NVIC_APINT_VECTKEY       (0x05FA0000u)  /* This key is required in order to write the NVIC_APINT register */
#define CY_NVIC_CFG_STACKALIGN      (0x00000200u)  /* This specifies that the exception stack must be 8 byte aligned */


/* Extern functions */
extern void CyBtldr_CheckLaunch(void);

/* Function prototypes */
void initialize_psoc(void);
CY_ISR(IntDefaultHandler);
void Reset(void);
CY_ISR(IntDefaultHandler);

#if defined(__ARMCC_VERSION)
    #define INITIAL_STACK_POINTER ((cyisraddress)(uint32)&Image$$ARM_LIB_STACK$$ZI$$Limit)
#elif defined (__GNUC__)
    #define INITIAL_STACK_POINTER (&__cy_stack)
#elif defined (__ICCARM__)
    #pragma language=extended
    #pragma segment="CSTACK"
    #define INITIAL_STACK_POINTER  { .__ptr = __sfe( "CSTACK" ) }

    extern void __iar_program_start( void );
    extern void __iar_data_init3 (void);
#endif  /* (__ARMCC_VERSION) */

/* Global variables */
#if !defined (__ICCARM__)
    CY_NOINIT static uint32 cySysNoInitDataValid;
#endif  /* !defined (__ICCARM__) */


/*******************************************************************************
* Default Ram Interrupt Vector table storage area. Must be 256-byte aligned.
*******************************************************************************/
#if defined (__ICCARM__)
    #pragma location=".ramvectors"
    #pragma data_alignment=256
#else
    CY_SECTION(".ramvectors")
    CY_ALIGN(256)
#endif  /* defined (__ICCARM__) */
cyisraddress CyRamVectors[CY_NUM_VECTORS];


/*******************************************************************************
* Function Name: IntDefaultHandler
********************************************************************************
*
* Summary:
*  This function is called for all interrupts, other than reset, that get
*  called before the system is setup.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Theory:
*  Any value other than zero is acceptable.
*
*******************************************************************************/
CY_ISR(IntDefaultHandler)
{

    while(1)
    {
        /***********************************************************************
        * We should never get here. If we do, a serious problem occured, so go
        * into an infinite loop.
        ***********************************************************************/
    }
}


#if defined(__ARMCC_VERSION)

/* Local function for the device reset. */
extern void Reset(void);

/* Application entry point. */
extern void $Super$$main(void);

/* Linker-generated Stack Base addresses, Two Region and One Region */
extern uint32 Image$$ARM_LIB_STACK$$ZI$$Limit;

/* RealView C Library initialization. */
extern int __main(void);


/*******************************************************************************
* Function Name: Reset
********************************************************************************
*
* Summary:
*  This function handles the reset interrupt for the RVDS/MDK toolchains.
*  This is the first bit of code that is executed at startup.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void Reset(void)
{
    #if(CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE)

        /* For PSoC 5LP, debugging is enabled by default */
        #if(CYDEV_DEBUGGING_ENABLE == 0)
            *(reg32 *)(CYDEV_DEBUG_ENABLE_REGISTER) |= CYDEV_DEBUG_ENABLE_MASK;
        #endif /* (CYDEV_DEBUGGING_ENABLE) */

        /* Reset Status Register has Read-to-clear SW access mode.
        * Preserve current RESET_SR0 state to make it available for next reading.
        */
        *(reg32 *)(CYREG_PHUB_CFGMEM23_CFG1) = *(reg32 *)(CYREG_RESET_SR0);

    #endif  /* (CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE) */

    #if(CYDEV_BOOTLOADER_ENABLE)
        CyBtldr_CheckLaunch();
    #endif /* (CYDEV_BOOTLOADER_ENABLE) */

    __main();
}


/*******************************************************************************
* Function Name: $Sub$$main
********************************************************************************
*
* Summary:
*  This function is called imediatly before the users main
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void $Sub$$main(void)
{
    initialize_psoc();

    /* Call original main */
    $Super$$main();

    while (1)
    {
        /* If main returns it is undefined what we should do. */
    }
}

#elif defined(__GNUC__)

void Start_c(void);

/* Stack Base address */
extern void __cy_stack(void);

/* Application entry point. */
extern int main(void);

/* The static objects constructors initializer */
extern void __libc_init_array(void);

typedef unsigned char __cy_byte_align8 __attribute ((aligned (8)));

struct __cy_region
{
    __cy_byte_align8 *init; /* Initial contents of this region.  */
    __cy_byte_align8 *data; /* Start address of region.  */
    size_t init_size;       /* Size of initial data.  */
    size_t zero_size;       /* Additional size to be zeroed.  */
};

extern const struct __cy_region __cy_regions[];
extern const char __cy_region_num __attribute__((weak));
#define __cy_region_num ((size_t)&__cy_region_num)


/*******************************************************************************
* Function Name: Reset
********************************************************************************
*
* Summary:
*  This function handles the reset interrupt for the GCC toolchain. This is the
*  first bit of code that is executed at startup.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void Reset(void)
{
    #if(CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE)

        /* For PSoC 5LP, debugging is enabled by default */
        #if(CYDEV_DEBUGGING_ENABLE == 0)
            *(reg32 *)(CYDEV_DEBUG_ENABLE_REGISTER) |= CYDEV_DEBUG_ENABLE_MASK;
        #endif /* (CYDEV_DEBUGGING_ENABLE) */

        /* Reset Status Register has Read-to-clear SW access mode.
        * Preserve current RESET_SR0 state to make it available for next reading.
        */
        *(reg32 *)(CYREG_PHUB_CFGMEM23_CFG1) = *(reg32 *)(CYREG_RESET_SR0);

    #endif  /* (CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE) */

    #if(CYDEV_BOOTLOADER_ENABLE)
        CyBtldr_CheckLaunch();
    #endif /* (CYDEV_BOOTLOADER_ENABLE) */

    Start_c();
}

__attribute__((weak))
void _exit(int status)
{
    /* Cause a divide by 0 exception */
    int x = status / INT_MAX;
    x = 4 / x;

    while(1)
    {
    }
}

/*******************************************************************************
* Function Name: Start_c
********************************************************************************
*
* Summary:
*  This function handles initializing the .data and .bss sections in
*  preperation for running standard C code.  Once initialization is complete
*  it will call main(). This function will never return.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void Start_c(void)  __attribute__ ((noreturn));
void Start_c(void)
{
    unsigned regions = __cy_region_num;
    const struct __cy_region *rptr = __cy_regions;

    /* Initialize memory */
    for (regions = __cy_region_num, rptr = __cy_regions; regions--; rptr++)
    {
        uint32 *src = (uint32 *)rptr->init;
        uint32 *dst = (uint32 *)rptr->data;
        unsigned limit = rptr->init_size;
        unsigned count;

        for (count = 0u; count != limit; count += sizeof (uint32))
        {
            *dst++ = *src++;
        }
        limit = rptr->zero_size;
        for (count = 0u; count != limit; count += sizeof (uint32))
        {
            *dst++ = 0u;
        }
    }

    /* Invoke static objects constructors */
    __libc_init_array();
    (void) main();

    while (1)
    {
        /* If main returns, make sure we don't return. */
    }
}


#elif defined (__ICCARM__)

/*******************************************************************************
* Function Name: __low_level_init
********************************************************************************
*
* Summary:
*  This function perform early initializations for the IAR Embedded
*  Workbench IDE. It is executed in the context of reset interrupt handler
*  before the data sections are initialized.
*
* Parameters:
*  None
*
* Return:
*  The value that determines whether or not data sections should be initialized
*  by the system startup code:
*    0 - skip data sections initialization;
*    1 - initialize data sections;
*
*******************************************************************************/
int __low_level_init(void)
{
    #if(CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE)

        /* For PSoC 5LP, debugging is enabled by default */
        #if(CYDEV_DEBUGGING_ENABLE == 0)
            *(reg32 *)(CYDEV_DEBUG_ENABLE_REGISTER) |= CYDEV_DEBUG_ENABLE_MASK;
        #endif /* (CYDEV_DEBUGGING_ENABLE) */

        /* Reset Status Register has Read-to-clear SW access mode.
        * Preserve current RESET_SR0 state to make it available for next reading.
        */
        *(reg32 *)(CYREG_PHUB_CFGMEM23_CFG1) = *(reg32 *)(CYREG_RESET_SR0);

    #endif  /* (CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE) */

    #if (CYDEV_BOOTLOADER_ENABLE)
        CyBtldr_CheckLaunch();
    #endif /* CYDEV_BOOTLOADER_ENABLE */

    /* Initialize data sections */
    __iar_data_init3();

    initialize_psoc();

    return 0;
}

#endif /* __GNUC__ */


/*******************************************************************************
*
* Default Rom Interrupt Vector table.
*
*******************************************************************************/
#if defined(__ARMCC_VERSION)
    /* Suppress diagnostic message 1296-D: extended constant initialiser used */
    #pragma diag_suppress 1296
#endif  /* defined(__ARMCC_VERSION) */

#if defined (__ICCARM__)
    #pragma location=".romvectors"
    const intvec_elem __vector_table[CY_NUM_ROM_VECTORS] =
#else
    CY_SECTION(".romvectors")
    const cyisraddress RomVectors[CY_NUM_ROM_VECTORS] =
#endif  /* defined (__ICCARM__) */
{
    INITIAL_STACK_POINTER,   /* The initial stack pointer  0 */
    #if defined (__ICCARM__) /* The reset handler          1 */
        __iar_program_start,
    #else
        (cyisraddress)&Reset,
    #endif  /* defined (__ICCARM__) */
    &IntDefaultHandler,      /* The NMI handler            2 */
    &IntDefaultHandler,      /* The hard fault handler     3 */
};

#if defined(__ARMCC_VERSION)
    #pragma diag_default 1296
#endif  /* defined(__ARMCC_VERSION) */


/*******************************************************************************
* Function Name: initialize_psoc
********************************************************************************
*
* Summary:
*  This function used to initialize the PSoC chip before calling main.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
#if (defined(__GNUC__) && !defined(__ARMCC_VERSION))
__attribute__ ((constructor(101)))
#endif
void initialize_psoc(void)
{
    uint32 i;

    /* Set Priority group 5. */

    /* Writes to NVIC_APINT register require the VECTKEY in the upper half */
    *CY_NVIC_APINT_PTR = CY_NVIC_APINT_VECTKEY | CY_NVIC_APINT_PRIGROUP_3_5;
    *CY_NVIC_CFG_CTRL_PTR |= CY_NVIC_CFG_STACKALIGN;

    /* Set Ram interrupt vectors to default functions. */
    for (i = 0u; i < CY_NUM_VECTORS; i++)
    {
        #if defined (__ICCARM__)
            CyRamVectors[i] = (i < CY_NUM_ROM_VECTORS) ? __vector_table[i].__fun : &IntDefaultHandler;
        #else
            CyRamVectors[i] = (i < CY_NUM_ROM_VECTORS) ? RomVectors[i] : &IntDefaultHandler;
        #endif  /* defined (__ICCARM__) */
    }

    /* Was stored in CFGMEM to avoid being cleared while SRAM gets cleared */
    CyResetStatus = CY_GET_REG8(CYREG_PHUB_CFGMEM23_CFG1);

    /* Point NVIC at the RAM vector table. */
    *CYINT_VECT_TABLE = CyRamVectors;

    /* Initialize the configuration registers. */
    cyfitter_cfg();

    #if(0u != DMA_CHANNELS_USED__MASK0)

        /* Setup DMA - only necessary if the design contains a DMA component. */
        CyDmacConfigure();

    #endif  /* (0u != DMA_CHANNELS_USED__MASK0) */

    #if !defined (__ICCARM__)
        /* Actually, no need to clean this variable, just to make compiler happy. */
        cySysNoInitDataValid = 0u;
    #endif  /* !defined (__ICCARM__) */
}


/* [] END OF FILE */
