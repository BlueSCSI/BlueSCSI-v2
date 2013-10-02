/*******************************************************************************
* File Name: Cm3Start.c
* Version 3.40
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

#include "cydevice_trm.h"
#include "cytypes.h"
#include "cyfitter_cfg.h"
#include "CyLib.h"
#include "CyDmac.h"
#include "cyfitter.h"

#define NUM_INTERRUPTS              32u
#define NUM_VECTORS                 (CYINT_IRQ_BASE+NUM_INTERRUPTS)
#define NUM_ROM_VECTORS             4u
#define NVIC_APINT                  ((reg32 *) CYREG_NVIC_APPLN_INTR)
#define NVIC_CFG_CTRL               ((reg32 *) CYREG_NVIC_CFG_CONTROL)
#define NVIC_APINT_PRIGROUP_3_5     0x00000400u  /* Priority group 3.5 split */
#define NVIC_APINT_VECTKEY          0x05FA0000u  /* This key is required in order to write the NVIC_APINT register */
#define NVIC_CFG_STACKALIGN         0x00000200u  /* This specifies that the exception stack must be 8 byte aligned */

/* Extern functions */
extern void CyBtldr_CheckLaunch(void);

/* Function prototypes */
void initialize_psoc(void);
CY_ISR(IntDefaultHandler);
void Reset(void);
CY_ISR(IntDefaultHandler);

#if defined(__ARMCC_VERSION)
    #define INITIAL_STACK_POINTER (cyisraddress)(uint32)&Image$$ARM_LIB_STACK$$ZI$$Limit
#elif defined (__GNUC__)
    #define INITIAL_STACK_POINTER __cs3_stack
#endif  /* (__ARMCC_VERSION) */

/* Global variables */
CY_NOINIT static uint32 cySysNoInitDataValid;


/*******************************************************************************
* Default Ram Interrupt Vector table storage area. Must be 256-byte aligned.
*******************************************************************************/

__attribute__ ((section(".ramvectors")))
#if defined(__ARMCC_VERSION)
__align(256)
#elif defined (__GNUC__)
__attribute__ ((aligned(256)))
#endif
cyisraddress CyRamVectors[NUM_VECTORS];


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
__asm void Reset(void)
{
    PRESERVE8
    EXTERN __main
    EXTERN CyResetStatus

    #if(CYDEV_BOOTLOADER_ENABLE)
        EXTERN CyBtldr_CheckLaunch
    #endif  /* (CYDEV_BOOTLOADER_ENABLE) */


    #if(CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE)
        #if(CYDEV_DEBUGGING_ENABLE)
            ldr  r3, =0x400046e8 /* CYDEV_DEBUG_ENABLE_REGISTER */
            ldrb r4, [r3, #0]
            orr  r4, r4, #01
            strb r4, [r3, #0]
debugEnabled
        #endif    /* (CYDEV_DEBUGGING_ENABLE) */

        ldr  r3, =0x400046fa /* CYREG_RESET_SR0 */
        ldrb r2, [r3, #0]

    #endif  /* (CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE) */

    ldr  r3, =0x400076BC /* CYREG_PHUB_CFGMEM23_CFG1 */
    strb r2, [r3, #0]

    #if(CYDEV_BOOTLOADER_ENABLE)
        bl CyBtldr_CheckLaunch
    #endif /* (CYDEV_BOOTLOADER_ENABLE) */

    /* Let RealView setup the libraries. */
    bl __main

    ALIGN
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

    /* If main returns it is undefined what we should do. */
    while (1);
}

#elif defined(__GNUC__)

extern void __cs3_stack(void);
extern void __cs3_start_c(void);


/*******************************************************************************
* Function Name: Reset
********************************************************************************
*
* Summary:
*  This function handles the reset interrupt for the GCC toolchain.  This is the
*  first bit of code that is executed at startup.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
__attribute__ ((naked))
void Reset(void)
{
    __asm volatile(
#if(CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE)

  #if(CYDEV_DEBUGGING_ENABLE)
    "    ldr  r3, =%0\n"
    "    ldrb r4, [r3, #0]\n"
    "    orr  r4, r4, #01\n"
    "    strb r4, [r3, #0]\n"
    "debugEnabled:\n"
  #endif    /* (CYDEV_DEBUGGING_ENABLE) */

    "    ldr  r3, =%1\n"
    "    ldrb r2, [r3, #0]\n"
    "    uxtb r2, r2\n"
#endif  /* (CYDEV_PROJ_TYPE != CYDEV_PROJ_TYPE_LOADABLE) */

    "    ldr  r3, =%2\n"
    "    strb r2, [r3, #0]\n"

#if(CYDEV_BOOTLOADER_ENABLE)
    "    bl CyBtldr_CheckLaunch\n"
#endif /* (CYDEV_BOOTLOADER_ENABLE) */

    /*  Switch to C initialization phase */
    "    bl __cs3_start_c\n" : : "i" (CYDEV_DEBUG_ENABLE_REGISTER), "i" (CYREG_RESET_SR0), "i" (CYREG_PHUB_CFGMEM23_CFG1));
}

#endif /* __GNUC__ */


/*******************************************************************************
*
* Default Rom Interrupt Vector table.
*
*******************************************************************************/
#if defined(__ARMCC_VERSION)
    #pragma diag_suppress 1296
#endif
__attribute__ ((section(".romvectors")))
const cyisraddress RomVectors[NUM_ROM_VECTORS] =
{
    #if defined(__ARMCC_VERSION)
        INITIAL_STACK_POINTER,           /* The initial stack pointer  0 */
    #elif defined (__GNUC__)
        &INITIAL_STACK_POINTER,          /* The initial stack pointer  0 */
    #endif  /* (__ARMCC_VERSION) */
    (cyisraddress)&Reset,    /* The reset handler          1 */
    &IntDefaultHandler,      /* The NMI handler            2 */
    &IntDefaultHandler,      /* The hard fault handler     3 */
};


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
    *NVIC_APINT = NVIC_APINT_VECTKEY | NVIC_APINT_PRIGROUP_3_5;
    *NVIC_CFG_CTRL |= NVIC_CFG_STACKALIGN;

    /* Set Ram interrupt vectors to default functions. */
    for(i = 0u; i < NUM_VECTORS; i++)
    {
        CyRamVectors[i] = (i < NUM_ROM_VECTORS) ? RomVectors[i] : &IntDefaultHandler;
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
    
    /* Actually, no need to clean this variable, just to make compiler happy. */
    cySysNoInitDataValid = 0u;
}


/* [] END OF FILE */
