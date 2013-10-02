/*******************************************************************************
* File Name: cyPm.c
* Version 3.40
*
* Description:
*  Provides an API for the power management.
*
* Note:
*  Documentation of the API's in this file is located in the
*  System Reference Guide provided with PSoC Creator.
*
********************************************************************************
* Copyright 2008-2013, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "cyPm.h"


/*******************************************************************
* Place your includes, defines and code here. Do not use merge
* region below unless any component datasheet suggest to do so.
*******************************************************************/
/* `#START CY_PM_HEADER_INCLUDE` */

/* `#END` */


static CY_PM_BACKUP_STRUCT          cyPmBackup;
static CY_PM_CLOCK_BACKUP_STRUCT    cyPmClockBackup;

/* Convertion table between register's values and frequency in MHz  */
static const uint8 CYCODE cyPmImoFreqReg2Mhz[7u] = {12u, 6u, 24u, 3u, 48u, 62u, 74u};

/* Function Prototypes */
static void CyPmHibSaveSet(void);
static void CyPmHibRestore(void) ;

static void CyPmSlpSaveSet(void) ;
static void CyPmSlpRestore(void) ;

static void CyPmHibSlpSaveSet(void) ;
static void CyPmHibSlpRestore(void) ;

static void CyPmHviLviSaveDisable(void) ;
static void CyPmHviLviRestore(void) ;

#if(CY_PSOC5A)

    /***************************************************************************
    * The PICU interupt event is not allowed to act as wakeup source for PSoC 5.
    * To prevent accidental wakeup all the PICU interrupts are disabled before
    * Sleep and Hibernate low power modes entry. In case of Sleep mode registers
    * values must be restored on wakeup, but in case of Hibernate low power mode
    * there is no sense in saving/restoring registers values as the only wakeup
    * source for this mode is external reset (XRES). For more information refer
    * to the PSoC 5 device TRM.
    ***************************************************************************/

    static void CyPmSavePicuInterrupts(void);
    static void CyPmDisablePicuInterrupts(void) ;
    static void CyPmRestorePicuInterrupts(void) ;

#endif  /* (CY_PSOC5A) */


/*******************************************************************************
* Function Name: CyPmSaveClocks
********************************************************************************
*
* Summary:
*  This function is called in preparation for entering sleep or hibernate low
*  power modes. Saves all state of the clocking system that does not persist
*  during sleep/hibernate or that needs to be altered in preparation for
*  sleep/hibernate. Shutdowns all the digital and analog clock dividers for the
*  active power mode configuration.
*
*  Switches the master clock over to the IMO and shuts down the PLL and MHz
*  Crystal. The IMO frequency is set to either 12 MHz or 48 MHz to match the
*  Design-Wide Resources System Editor "Enable Fast IMO During Startup" setting.
*  The ILO and 32 KHz oscillators are not impacted. The current Flash wait state
*  setting is saved and the Flash wait state setting is set for the current IMO
*  speed.
*
*  Note If the Master Clock source is routed through the DSI inputs, then it
*  must be set manually to another source before using the
*  CyPmSaveClocks()/CyPmRestoreClocks() functions.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Side Effects:
*  All peripheral clocks are going to be off after this API method call.
*
*******************************************************************************/
void CyPmSaveClocks(void) 
{
    /* Digital and analog clocks - save enable state and disable them all */
    cyPmClockBackup.enClkA = CY_PM_ACT_CFG1_REG & CY_PM_ACT_EN_CLK_A_MASK;
    cyPmClockBackup.enClkD = CY_PM_ACT_CFG2_REG;
    CY_PM_ACT_CFG1_REG &= ((uint8)(~CY_PM_ACT_EN_CLK_A_MASK));
    CY_PM_ACT_CFG2_REG &= ((uint8)(~CY_PM_ACT_EN_CLK_D_MASK));

    /* Save current flash wait cycles and set the maximum value */
    cyPmClockBackup.flashWaitCycles = CY_PM_CACHE_CR_CYCLES_MASK & CY_PM_CACHE_CR_REG;
    CyFlash_SetWaitCycles(CY_PM_MAX_FLASH_WAIT_CYCLES);

    /* IMO - save current IMO MHz OSC frequency and USB mode is on bit */
    cyPmClockBackup.imoFreq = CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_FREQ_MASK;
    cyPmClockBackup.imoUsbClk = CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_USB;

    /* IMO doubler - save enable state */
    if(0u != (CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_F2XON))
    {
        /* IMO doubler enabled - save and disable */
        cyPmClockBackup.imo2x = CY_PM_ENABLED;
    }
    else
    {
        /* IMO doubler disabled */
        cyPmClockBackup.imo2x = CY_PM_DISABLED;
    }

    /* IMO - set appropriate frequency for LPM */
    CyIMO_SetFreq(CY_PM_IMO_FREQ_LPM);

    /* IMO - save enable state and enable without wait to settle */
    if(0u != (CY_PM_ACT_CFG0_IMO & CY_PM_ACT_CFG0_REG))
    {
        /* IMO - save enabled state */
        cyPmClockBackup.imoEnable = CY_PM_ENABLED;
    }
    else
    {
        /* IMO - save disabled state */
        cyPmClockBackup.imoEnable = CY_PM_DISABLED;

        /* IMO - enable */
        CyIMO_Start(CY_PM_IMO_NO_WAIT_TO_SETTLE);
    }

    /* IMO - save the current IMOCLK source and set to IMO if not yet */
    if(0u != (CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_XCLKEN))
    {
        /* DSI or XTAL CLK */
        cyPmClockBackup.imoClkSrc =
            (0u == (CY_PM_CLKDIST_CR_REG & CY_PM_CLKDIST_IMO2X_SRC)) ? CY_IMO_SOURCE_DSI : CY_IMO_SOURCE_XTAL;

        /* IMO -  set IMOCLK source to MHz OSC */
        CyIMO_SetSource(CY_IMO_SOURCE_IMO);
    }
    else
    {
        /* IMO */
        cyPmClockBackup.imoClkSrc = CY_IMO_SOURCE_IMO;
    }

    /* Save clk_imo source */
    cyPmClockBackup.clkImoSrc = CY_PM_CLKDIST_CR_REG & CY_PM_CLKDIST_IMO_OUT_MASK;

    /* If IMOCLK2X or SPC OSC is source for clk_imo, set it to IMOCLK */
    if(CY_PM_CLKDIST_IMO_OUT_IMO != cyPmClockBackup.clkImoSrc)
    {
        /* Set IMOCLK to source for clk_imo */
        CY_PM_CLKDIST_CR_REG = (CY_PM_CLKDIST_CR_REG & ((uint8)(~CY_PM_CLKDIST_IMO_OUT_MASK))) |
                                CY_PM_CLKDIST_IMO_OUT_IMO;
    }    /* Need to change nothing if IMOCLK is source clk_imo */

    /* IMO doubler - disable it (saved above) */
    if(0u != (CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_F2XON))
    {
        CyIMO_DisableDoubler();
    }

    /* Master clock - save divider and set it to divide-by-one (if no yet) */
    cyPmClockBackup.clkSyncDiv = CY_PM_CLKDIST_MSTR0_REG;
    if(CY_PM_DIV_BY_ONE != cyPmClockBackup.clkSyncDiv)
    {
        CyMasterClk_SetDivider(CY_PM_DIV_BY_ONE);
    }    /* Need to change nothing if master clock divider is 1 */

    /* Master clock - save current source */
    cyPmClockBackup.masterClkSrc = CY_PM_CLKDIST_MSTR1_REG & CY_PM_MASTER_CLK_SRC_MASK;

    /* Master clock source - set it to IMO if not yet. */
    if(CY_MASTER_SOURCE_IMO != cyPmClockBackup.masterClkSrc)
    {
        CyMasterClk_SetSource(CY_MASTER_SOURCE_IMO);
    }    /* Need to change nothing if master clock source is IMO */

    /* Bus clock - save divider and set it, if needed, to divide-by-one */
    cyPmClockBackup.clkBusDiv = (uint16) ((uint16) CY_PM_CLK_BUS_MSB_DIV_REG << 8u) | CY_PM_CLK_BUS_LSB_DIV_REG;
    if(CY_PM_BUS_CLK_DIV_BY_ONE != cyPmClockBackup.clkBusDiv)
    {
        CyBusClk_SetDivider(CY_PM_BUS_CLK_DIV_BY_ONE);
    }    /* Do nothing if saved and actual values are equal */

    /* Set number of wait cycles for the flash according CPU frequency in MHz */
    CyFlash_SetWaitCycles((uint8)CY_PM_GET_CPU_FREQ_MHZ);

    /* PLL - check enable state, disable if needed */
    if(0u != (CY_PM_FASTCLK_PLL_CFG0_REG & CY_PM_PLL_CFG0_ENABLE))
    {
        /* PLL is enabled - save state and disable */
        cyPmClockBackup.pllEnableState = CY_PM_ENABLED;
        CyPLL_OUT_Stop();
    }
    else
    {
        /* PLL is disabled - save state */
        cyPmClockBackup.pllEnableState = CY_PM_DISABLED;
    }

    /* MHz ECO - check enable state and disable if needed */
    if(0u != (CY_PM_FASTCLK_XMHZ_CSR_REG & CY_PM_XMHZ_CSR_ENABLE))
    {
        /* MHz ECO is enabled - save state and disable */
        cyPmClockBackup.xmhzEnableState = CY_PM_ENABLED;
        CyXTAL_Stop();
    }
    else
    {
        /* MHz ECO is disabled - save state */
        cyPmClockBackup.xmhzEnableState = CY_PM_DISABLED;
    }


    /***************************************************************************
    * Save enable state of delay between the system bus clock and each of the
    * 4 individual analog clocks. This bit non-retention and it's value should
    * be restored on wakeup.
    ***************************************************************************/
    if(0u != (CY_PM_CLKDIST_DELAY_REG & CY_PM_CLKDIST_DELAY_EN))
    {
        cyPmClockBackup.clkDistDelay = CY_PM_ENABLED;
    }
    else
    {
        cyPmClockBackup.clkDistDelay = CY_PM_DISABLED;
    }
}


/*******************************************************************************
* Function Name: CyPmRestoreClocks
********************************************************************************
*
* Summary:
*  Restores any state that was preserved by the last call to CyPmSaveClocks().
*  The Flash wait state setting is also restored.
*
*  Note If the Master Clock source is routed through the DSI inputs, then it
*  must be set manually to another source before using the
*  CyPmSaveClocks()/CyPmRestoreClocks() functions.
*
*  PSoC 3 and PSoC 5LP:
*  The merge region could be used to process state when the megahertz crystal is
*  not ready after the hold-off timeout.
*
*  PSoC 5:
*  The 130 ms is given for the megahertz crystal to stabilize. It's readiness is
*  not verified after the hold-off timeout.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyPmRestoreClocks(void) 
{
    #if (!CY_PSOC5A)

        cystatus status = CYRET_TIMEOUT;
        uint16 i;

    #endif  /* (!CY_PSOC5A) */


    /* Convertion table between CyIMO_SetFreq() parameters and register's value */
    const uint8 CYCODE cyPmImoFreqMhz2Reg[7u] = {
        CY_IMO_FREQ_12MHZ, CY_IMO_FREQ_6MHZ,  CY_IMO_FREQ_24MHZ, CY_IMO_FREQ_3MHZ,
        CY_IMO_FREQ_48MHZ, 5u, 6u};

    /* Restore enable state of delay between the system bus clock and ACLKs. */
    if(CY_PM_ENABLED == cyPmClockBackup.clkDistDelay)
    {
        /* Delay for both the bandgap and the delay line to settle out */
        CyDelayCycles((uint32)(CY_PM_CLK_DELAY_BANDGAP_SETTLE_US + CY_PM_CLK_DELAY_BIAS_SETTLE_US) *
                        CY_PM_GET_CPU_FREQ_MHZ);

        CY_PM_CLKDIST_DELAY_REG |= CY_PM_CLKDIST_DELAY_EN;
    }

    /* MHz ECO restore state */
    if(CY_PM_ENABLED == cyPmClockBackup.xmhzEnableState)
    {
        /***********************************************************************
        * Enabling XMHZ XTAL. The actual CyXTAL_Start() with non zero wait
        * period uses FTW for period measurement. This could cause a problem
        * if CTW/FTW is used as a wake up time in the low power modes APIs.
        * So, the XTAL wait procedure is implemented with a software delay.
        ***********************************************************************/

        /* Enable XMHZ XTAL with no wait */
        (void) CyXTAL_Start(CY_PM_XTAL_MHZ_NO_WAIT);

        #if(CY_PSOC5A)

            /* Make a 130 milliseconds delay */
            CyDelayCycles((uint32)CY_PM_WAIT_200_US * CY_PM_GET_CPU_FREQ_MHZ * CY_PM_MHZ_XTAL_WAIT_NUM_OF_200_US);

        #else

            /* Read XERR bit to clear it */
            (void) CY_PM_FASTCLK_XMHZ_CSR_REG;

            /* Wait */
            for(i = CY_PM_MHZ_XTAL_WAIT_NUM_OF_200_US; i > 0u; i--)
            {
                /* Make a 200 microseconds delay */
                CyDelayCycles((uint32)CY_PM_WAIT_200_US * CY_PM_GET_CPU_FREQ_MHZ);

                /* High output indicates oscillator failure */
                if(0u == (CY_PM_FASTCLK_XMHZ_CSR_REG & CY_PM_XMHZ_CSR_XERR))
                {
                    status = CYRET_SUCCESS;
                    break;
                }
            }

            if(CYRET_TIMEOUT == status)
            {
                /*******************************************************************
                * Process the situation when megahertz crystal is not ready.
                * Time to stabialize value is crystal specific.
                *******************************************************************/

                /* `#START_MHZ_ECO_TIMEOUT` */

                /* `#END` */
            }

        #endif  /* (CY_PSOC5A) */

    }   /* (CY_PM_ENABLED == cyPmClockBackup.xmhzEnableState) */


    /* Temprorary set the maximum flash wait cycles */
    CyFlash_SetWaitCycles(CY_PM_MAX_FLASH_WAIT_CYCLES);

    /* The XTAL and DSI clocks are ready to be source for Master clock. */
    if((CY_PM_MASTER_CLK_SRC_XTAL == cyPmClockBackup.masterClkSrc) ||
       (CY_PM_MASTER_CLK_SRC_DSI  == cyPmClockBackup.masterClkSrc))
    {
        /* Restore Master clock's divider */
        if(CY_PM_CLKDIST_MSTR0_REG != cyPmClockBackup.clkSyncDiv)
        {
            /* Restore Master clock divider */
            CyMasterClk_SetDivider(cyPmClockBackup.clkSyncDiv);
        }

        /* Restore Master clock source */
        CyMasterClk_SetSource(cyPmClockBackup.masterClkSrc);
    }

    /* IMO - restore IMO frequency */
    if((0u != (cyPmClockBackup.imoUsbClk & CY_PM_FASTCLK_IMO_CR_USB)) &&
        (CY_IMO_FREQ_24MHZ == cyPmImoFreqMhz2Reg[cyPmClockBackup.imoFreq]))
    {
        /* Restore IMO frequency (24 MHz) and trim it for USB */
        CyIMO_SetFreq(CY_IMO_FREQ_USB);
    }
    else
    {
        /* Restore IMO frequency */
        CyIMO_SetFreq(cyPmImoFreqMhz2Reg[cyPmClockBackup.imoFreq]);

        if(0u != (cyPmClockBackup.imoUsbClk & CY_PM_FASTCLK_IMO_CR_USB))
        {
            CY_PM_FASTCLK_IMO_CR_REG |= CY_PM_FASTCLK_IMO_CR_USB;
        }
        else
        {
            CY_PM_FASTCLK_IMO_CR_REG &= ((uint8)(~CY_PM_FASTCLK_IMO_CR_USB));
        }
    }

    /* IMO - restore enable state if needed */
    if((CY_PM_ENABLED == cyPmClockBackup.imoEnable) &&
       (0u == (CY_PM_ACT_CFG0_IMO & CY_PM_ACT_CFG0_REG)))
    {
        /* IMO - restore enabled state */
        CyIMO_Start(CY_PM_IMO_NO_WAIT_TO_SETTLE);
    }

    /* IMO - restore disable state if needed */
    if((CY_PM_DISABLED == cyPmClockBackup.imoEnable) &&
       (0u != (CY_PM_ACT_CFG0_IMO & CY_PM_ACT_CFG0_REG)))
    {
        CyIMO_Stop();
    }

    /* IMO - restore IMOCLK source */
    CyIMO_SetSource(cyPmClockBackup.imoClkSrc);

    /* Restore IMO doubler enable state (turned off by CyPmSaveClocks()) */
    if(CY_PM_ENABLED == cyPmClockBackup.imo2x)
    {
        CyIMO_EnableDoubler();
    }

    /* IMO - restore clk_imo source, if needed */
    if(cyPmClockBackup.clkImoSrc != (CY_PM_CLKDIST_CR_REG & CY_PM_CLKDIST_IMO_OUT_MASK))
    {
        CY_PM_CLKDIST_CR_REG = (CY_PM_CLKDIST_CR_REG & ((uint8)(~CY_PM_CLKDIST_IMO_OUT_MASK))) |
                                cyPmClockBackup.clkImoSrc;
    }

    /* PLL restore state */
    if(CY_PM_ENABLED == cyPmClockBackup.pllEnableState)
    {
        /***********************************************************************
        * Enable PLL. The actual CyPLL_OUT_Start() without wait period uses FTW
        * for period measurement. This could cause a problem if CTW/FTW is used
        * as a wakeup time in the low power modes APIs. To omit this issue PLL
        * wait procedure is implemented with a software delay.
        ***********************************************************************/

        /* Enable PLL */
        (void) CyPLL_OUT_Start(CY_PM_PLL_OUT_NO_WAIT);

        /* Make a 250 us delay */
        CyDelayCycles((uint32)CY_PM_WAIT_250_US * CY_PM_GET_CPU_FREQ_MHZ);
    }   /* (CY_PM_ENABLED == cyPmClockBackup.pllEnableState) */


    /* PLL and IMO is ready to be source for Master clock */
    if((CY_PM_MASTER_CLK_SRC_IMO == cyPmClockBackup.masterClkSrc) ||
       (CY_PM_MASTER_CLK_SRC_PLL == cyPmClockBackup.masterClkSrc))
    {
        /* Restore Master clock divider */
        if(CY_PM_CLKDIST_MSTR0_REG != cyPmClockBackup.clkSyncDiv)
        {
            CyMasterClk_SetDivider(cyPmClockBackup.clkSyncDiv);
        }

        /* Restore Master clock source */
        CyMasterClk_SetSource(cyPmClockBackup.masterClkSrc);
    }

    /* Bus clock - restore divider, if needed */
    if(cyPmClockBackup.clkBusDiv != ((uint16)((uint16) CY_PM_CLK_BUS_MSB_DIV_REG << 8u) | CY_PM_CLK_BUS_LSB_DIV_REG))
    {
        CyBusClk_SetDivider(cyPmClockBackup.clkBusDiv);
    }

    /* Restore flash wait cycles */
    CY_PM_CACHE_CR_REG = ((CY_PM_CACHE_CR_REG & ((uint8)(~CY_PM_CACHE_CR_CYCLES_MASK))) |
                           cyPmClockBackup.flashWaitCycles);

    /* Digital and analog clocks - restore state */
    CY_PM_ACT_CFG1_REG = cyPmClockBackup.enClkA;
    CY_PM_ACT_CFG2_REG = cyPmClockBackup.enClkD;
}


/*******************************************************************************
* Function Name: CyPmAltAct
********************************************************************************
*
* Summary:
*  Puts the part into the Alternate Active (Standby) state. The Alternate Active
*  state can allow for any of the capabilities of the device to be active, but
*  the operation of this function is dependent on the CPU being disabled during
*  the Alternate Active state. The configuration code and the component APIs
*  will configure the template for the Alternate Active state to be the same as
*  the Active state with the exception that the CPU will be disabled during
*  Alternate Active.
*
*  Note Before calling this function, you must manually configure the power mode
*  of the source clocks for the timer that is used as the wakeup timer.
*
*  PSoC 3:
*  Before switching to Alternate Active, if a wakeupTime other than NONE is
*  specified, then the appropriate timer state is configured as specified with
*  the interrupt for that timer disabled.  The wakeup source will be the
*  combination of the values specified in the wakeupSource and any timer
*  specified in the wakeupTime argument.  Once the wakeup condition is
*  satisfied, then all saved state is restored and the function returns in the
*  Active state.
*
*  Note that if the wakeupTime is made with a different value, the period before
*  the wakeup occurs can be significantly shorter than the specified time.  If
*  the next call is made with the same wakeupTime value, then the wakeup will
*  occur the specified period after the previous wakeup occurred.
*
*  If a wakeupTime other than NONE is specified, then upon exit the state of the
*  specified timer will be left as specified by wakeupTime with the timer
*  enabled and the interrupt disabled.  If the CTW, FTW or One PPS is already
*  configured for wakeup, for example with the SleepTimer or RTC components,
*  then specify NONE for the wakeupTime and include the appropriate source for
*  wakeupSource.
*
*  PSoC 5:
*  This function is used to both enter the Alternate Active mode and halt the
*  processor.  For PSoC 3 these two actions must be paired together.  With
*  PSoC 5 the processor can be halted independently with the __WFI() function
*  from the CMSIS library that is included in Creator.  This function should be
*  used instead when the action required is just to halt the processor until an
*  enabled interrupt occurs.
*
*  Neither of the parameters to the CyPmAltAct() function are used. The
*  parameters must be set to 0 (PM_ALT_ACT_TIME_NONE and PM_ALT_ACT_SRC_NONE).
*  The wake up time configuration can be done by a separate component: the CTW
*  wakeup interval should be configured with the Sleep Timer component and one
*  second interval should be configured with the RTC component.
*
*  Upon function execution the device will be switched from Active to Alternate
*  Active mode and the CPU will be halted. When an enabled interrupt occurs the
*  device will be switched to Active mode and the CPU will be started. Note that
*  if a wakeup event occurs and the associated interrupt is not enabled, then
*  the device will switch to Active mode with the CPU still halted. The CPU will
*  remain halted until an enabled interrupt occurs.
*
*  PSoC 5LP:
*  This function is used to both enter the Alternate Active mode and halt the
*  processor.  For PSoC 3 these two actions must be paired together.  With PSoC
*  5LP the processor can be halted independently with the __WFI() function from
*  the CMSIS library that is included in Creator.  This function should be used
*  instead when the action required is just to halt the processor until an
*  enabled interrupt occurs.
*
*  The wakeupTime parameter is not used for this device. It must be set to zero
*  (PM_ALT_ACT_TIME_NONE). The wake up time configuration can be done by a
*  separate component: the CTW wakeup interval should be configured with the
*  Sleep Timer component and one second interval should be configured with the
*  RTC component.
*
*  The wakeup behavior depends on wakeupSource parameter in the following
*  manner: upon function execution the device will be switched from Active to
*  Alternate Active mode and then the CPU will be halted. When an enabled wakeup
*  event occurs the device will return to Active mode.  Similarly when an
*  enabled interrupt occurs the CPU will be started. These two actions will
*  occur together provided that the event that occurs is an enabled wakeup
*  source and also generates an interrupt. If just the wakeup event occurs then
*  the device will be in Active mode, but the CPU will remain halted waiting for
*  an interrupt. If an interrupt occurs from something other than a wakeup
*  source, then the CPU will restart with the device in Alternate Active mode
*  until a wakeup event occurs.
*
*  For example, if CyPmAltAct(PM_ALT_ACT_TIME_NONE, PM_ALT_ACT_SRC_PICU) is
*  called and PICU interrupt occurs, the CPU will be started and device will be
*  switched into Active mode. And if CyPmAltAct(PM_ALT_ACT_TIME_NONE,
*  PM_ALT_ACT_SRC_NONE) is called and PICU interrupt occurs, the CPU will be
*  started while device remains in Alternate Active mode.
*
* Parameters:
*  wakeupTime: Specifies a timer wakeup source and the frequency of that
*              source. For PSoC 5 and PSoC 5LP this parameter is ignored.
*
*           Define                      Time
*  PM_ALT_ACT_TIME_NONE             None
*  PM_ALT_ACT_TIME_ONE_PPS          One PPS: 1 second
*  PM_ALT_ACT_TIME_CTW_2MS          CTW: 2 ms
*  PM_ALT_ACT_TIME_CTW_4MS          CTW: 4 ms
*  PM_ALT_ACT_TIME_CTW_8MS          CTW: 8 ms
*  PM_ALT_ACT_TIME_CTW_16MS         CTW: 16 ms
*  PM_ALT_ACT_TIME_CTW_32MS         CTW: 32 ms
*  PM_ALT_ACT_TIME_CTW_64MS         CTW: 64 ms
*  PM_ALT_ACT_TIME_CTW_128MS        CTW: 128 ms
*  PM_ALT_ACT_TIME_CTW_256MS        CTW: 256 ms
*  PM_ALT_ACT_TIME_CTW_512MS        CTW: 512 ms
*  PM_ALT_ACT_TIME_CTW_1024MS       CTW: 1024 ms
*  PM_ALT_ACT_TIME_CTW_2048MS       CTW: 2048 ms
*  PM_ALT_ACT_TIME_CTW_4096MS       CTW: 4096 ms
*  PM_ALT_ACT_TIME_FTW(1-256)*       FTW: 10us to 2.56 ms
*
*  *Note:   PM_ALT_ACT_TIME_FTW() is a macro that takes an argument that
*           specifies how many increments of 10 us to delay.
            For PSoC 3 silicon the valid range of  values is 1 to 256.
*
*  wakeUpSource:    Specifies a bitwise mask of wakeup sources. In addition, if
*                   a wakeupTime has been specified the associated timer will be
*                   included as a wakeup source. For PSoC 5 this parameter is
*                   ignored.
*
*           Define                      Source
*  PM_ALT_ACT_SRC_NONE              None
*  PM_ALT_ACT_SRC_COMPARATOR0       Comparator 0
*  PM_ALT_ACT_SRC_COMPARATOR1       Comparator 1
*  PM_ALT_ACT_SRC_COMPARATOR2       Comparator 2
*  PM_ALT_ACT_SRC_COMPARATOR3       Comparator 3
*  PM_ALT_ACT_SRC_INTERRUPT         Interrupt
*  PM_ALT_ACT_SRC_PICU              PICU
*  PM_ALT_ACT_SRC_I2C               I2C
*  PM_ALT_ACT_SRC_BOOSTCONVERTER    Boost Converter
*  PM_ALT_ACT_SRC_FTW               Fast Timewheel*
*  PM_ALT_ACT_SRC_VD                High and Low Voltage Detection (HVI, LVI)*
*  PM_ALT_ACT_SRC_CTW               Central Timewheel**
*  PM_ALT_ACT_SRC_ONE_PPS           One PPS**
*  PM_ALT_ACT_SRC_LCD               LCD
*
*  *Note : FTW and HVI/LVI wakeup signals are in the same mask bit.
*  **Note: CTW and One PPS wakeup signals are in the same mask bit.
*
*  When specifying a Comparator as the wakeupSource an instance specific define
*  should be used that will track with the specific comparator that the instance
*  is placed into. As an example, for a Comparator instance named MyComp the
*  value to OR into the mask is: MyComp_ctComp__CMP_MASK.
*
*  When CTW, FTW or One PPS is used as a wakeup source, the CyPmReadStatus()
*  function must be called upon wakeup with corresponding parameter. Please
*  refer to the CyPmReadStatus() API in the System Reference Guide for more
*  information.
*
* Return:
*  None
*
* Reentrant:
*  No
*
* Side Effects:
*  If a wakeupTime other than NONE is specified, then upon exit the state of the
*  specified timer will be left as specified by wakeupTime with the timer
*  enabled and the interrupt disabled.  Also, the ILO 1 KHz (if CTW timer is
*  used as wakeup time) or ILO 100 KHz (if FTW timer is used as wakeup time)
*  will be left started.
*
*******************************************************************************/
void CyPmAltAct(uint16 wakeupTime, uint16 wakeupSource) 
{
    #if(CY_PSOC5)

        /* Arguments expected to be 0 */
        CYASSERT(PM_ALT_ACT_TIME_NONE == wakeupTime);

        #if(CY_PSOC5A)

            /* The wakeupSource argument expected to be 0 */
            CYASSERT(PM_ALT_ACT_SRC_NONE  == wakeupSource);

            if(0u != wakeupSource)
            {
                /* To remove unreferenced local variable warning */
            }

        #endif /* (CY_PSOC5A) */

        if(0u != wakeupTime)
        {
            /* To remove unreferenced local variable warning */
        }

    #endif /* (CY_PSOC5) */


    #if(CY_PSOC3)

        /* FTW - save current and set new configuration */
        if((wakeupTime >= PM_ALT_ACT_TIME_FTW(1u)) && (wakeupTime <= PM_ALT_ACT_TIME_FTW(256u)))
        {
            CyPmFtwSetInterval(PM_ALT_ACT_FTW_INTERVAL(wakeupTime));

            /* Include associated timer to the wakeupSource */
            wakeupSource |= PM_ALT_ACT_SRC_FTW;
        }

        /* CTW - save current and set new configuration */
        if((wakeupTime >= PM_ALT_ACT_TIME_CTW_2MS) && (wakeupTime <= PM_ALT_ACT_TIME_CTW_4096MS))
        {
            /* Save current CTW configuration and set new one */
            CyPmCtwSetInterval((uint8)(wakeupTime - 1u));

            /* Include associated timer to the wakeupSource */
            wakeupSource |= PM_ALT_ACT_SRC_CTW;
        }

        /* 1PPS - save current and set new configuration */
        if(PM_ALT_ACT_TIME_ONE_PPS == wakeupTime)
        {
            /* Save current 1PPS configuration and set new one */
            CyPmOppsSet();

            /* Include associated timer to the wakeupSource */
            wakeupSource |= PM_ALT_ACT_SRC_ONE_PPS;
        }

    #endif /* (CY_PSOC3) */


    #if(CY_PSOC3 || CY_PSOC5LP)

        /* Save and set new wake up configuration */

        /* Interrupt, PICU, I2C, Boost converter, CTW/1PPS */
        cyPmBackup.wakeupCfg0 = CY_PM_WAKEUP_CFG0_REG;
        CY_PM_WAKEUP_CFG0_REG = (uint8) (wakeupSource >> 4u);

        /* Comparators */
        cyPmBackup.wakeupCfg1 = CY_PM_WAKEUP_CFG1_REG;
        CY_PM_WAKEUP_CFG1_REG = (((uint8) wakeupSource) & CY_PM_WAKEUP_SRC_CMPS_MASK);

        /* LCD */
        cyPmBackup.wakeupCfg2 = CY_PM_WAKEUP_CFG2_REG;
        CY_PM_WAKEUP_CFG2_REG = ((uint8) ((wakeupSource >> 12u) & 0x01u));

    #endif  /* (CY_PSOC3 || CY_PSOC5LP) */


    /* Switch to the Alternate Active mode */
    CY_PM_MODE_CSR_REG = ((CY_PM_MODE_CSR_REG & ((uint8)(~CY_PM_MODE_CSR_MASK))) | CY_PM_MODE_CSR_ALT_ACT);

    /* Recommended readback. */
    (void) CY_PM_MODE_CSR_REG;

    /* Two recommended NOPs to get into the mode. */
    CY_NOP;
    CY_NOP;

    /* Execute WFI instruction (for ARM-based devices only) */
    CY_PM_WFI;

    /* Point of return from Alternate Active Mode */

    #if(CY_PSOC3 || CY_PSOC5LP)

        /* Restore wake up configuration */
        CY_PM_WAKEUP_CFG0_REG = cyPmBackup.wakeupCfg0;
        CY_PM_WAKEUP_CFG1_REG = cyPmBackup.wakeupCfg1;
        CY_PM_WAKEUP_CFG2_REG = cyPmBackup.wakeupCfg2;

    #endif  /* (CY_PSOC3 || CY_PSOC5LP) */
}


/*******************************************************************************
* Function Name: CyPmSleep
********************************************************************************
*
* Summary:
*  Puts the part into the Sleep state.
*
*  Note Before calling this function, you must manually configure the power
*  mode of the source clocks for the timer that is used as wakeup timer.
*
*  Note Before calling this function, you must prepare clock tree configuration
*  for the low power mode by calling CyPmSaveClocks(). And restore clock
*  configuration after CyPmSleep() execution by calling CyPmRestoreClocks(). See
*  Power Management section, Clock Configuration subsection of the System
*  Reference Guide for more information.
*
*  PSoC 3:
*  Before switching to Sleep, if a wakeupTime other than NONE is specified,
*  then the appropriate timer state is configured as specified with the
*  interrupt for that timer disabled.  The wakeup source will be the combination
*  of the values specified in the wakeupSource and any timer specified in the
*  wakeupTime argument.  Once the wakeup condition is satisfied, then all saved
*  state is restored and the function returns in the Active state.
*
*  Note that if the wakeupTime is made with a different value, the period before
*  the wakeup occurs can be significantly shorter than the specified time.  If
*  the next call is made with the same wakeupTime value, then the wakeup will
*  occur the specified period after the previous wakeup occurred.
*
*  If a wakeupTime other than NONE is specified, then upon exit the state of the
*  specified timer will be left as specified by wakeupTime with the timer
*  enabled and the interrupt disabled.  If the CTW or One PPS is already
*  configured for wakeup, for example with the SleepTimer or RTC components,
*  then specify NONE for the wakeupTime and include the appropriate source for
*  wakeupSource.
*
*  PSoC 5:
*  Neither parameter to this function is used for PSoC 5. The parameters must be
*  set to 0 (PM_SLEEP_TIME_NONE and PM_SLEEP_SRC_NONE).  The device will go
*  into Sleep mode until it is woken by an interrupt from the Central Time Wheel
*  (CTW). The CTW must already be configured to generate an interrupt. It is
*  configured using the SleepTimer component.  Only the CTW can be used to wake
*  the device from sleep mode.  The other wakeup sources, Once per second or
*  Port Interrupt Controller (PICU), cannot be used reliably with PSoC 5. This
*  function automatically disables these interrupt sources and then restores
*  them after the devices is woken by the CTW.
*
*  The duration of sleep needs to be controlled so that the device doesn't wake
*  up too soon after going to sleep or remain asleep for too long.  Reliable
*  sleep times of between 1 ms and 128 ms can be supported.  This requirement is
*  satisfied with CTW settings of 4, 8, 16, 32, 64, 128 or 256 ms.  To control
*  the sleep time the CTW is reset automatically just before putting the device
*  to sleep.  The resulting wakeup time is half the duration programmed into the
*  CTW with an uncertainty of 1 ms due to the arrival time of the first ILO
*  clock edge. For example, the setting of 4 ms will result in a sleep time
*  between 1 ms and 2 ms.
*
*  PSoC 5LP:
*  The wakeupTime parameter is not used and the only NONE can be specified.
*  The wakeup time must be configured with the component, SleepTimer for CTW
*  intervals and RTC for 1PPS interval. The component must be configured to
*  generate an interrrupt.
*
* Parameters:
*  wakeupTime:      Specifies a timer wakeup source and the frequency of that
*                   source. For PSoC 5 and PSoC 5LP, this parameter is ignored.
*
*           Define                      Time
*  PM_SLEEP_TIME_NONE               None
*  PM_SLEEP_TIME_ONE_PPS            One PPS: 1 second
*  PM_SLEEP_TIME_CTW_2MS            CTW: 2 ms
*  PM_SLEEP_TIME_CTW_4MS            CTW: 4 ms
*  PM_SLEEP_TIME_CTW_8MS            CTW: 8 ms
*  PM_SLEEP_TIME_CTW_16MS           CTW: 16 ms
*  PM_SLEEP_TIME_CTW_32MS           CTW: 32 ms
*  PM_SLEEP_TIME_CTW_64MS           CTW: 64 ms
*  PM_SLEEP_TIME_CTW_128MS          CTW: 128 ms
*  PM_SLEEP_TIME_CTW_256MS          CTW: 256 ms
*  PM_SLEEP_TIME_CTW_512MS          CTW: 512 ms
*  PM_SLEEP_TIME_CTW_1024MS         CTW: 1024 ms
*  PM_SLEEP_TIME_CTW_2048MS         CTW: 2048 ms
*  PM_SLEEP_TIME_CTW_4096MS         CTW: 4096 ms
*
*  wakeUpSource:    Specifies a bitwise mask of wakeup sources. In addition, if
*                   a wakeupTime has been specified the associated timer will be
*                   included as a wakeup source. For PSoC 5 this parameter is
*                   ignored.
*
*           Define                      Source
*  PM_SLEEP_SRC_NONE                None
*  PM_SLEEP_SRC_COMPARATOR0         Comparator 0
*  PM_SLEEP_SRC_COMPARATOR1         Comparator 1
*  PM_SLEEP_SRC_COMPARATOR2         Comparator 2
*  PM_SLEEP_SRC_COMPARATOR3         Comparator 3
*  PM_SLEEP_SRC_PICU                PICU
*  PM_SLEEP_SRC_I2C                 I2C
*  PM_SLEEP_SRC_BOOSTCONVERTER      Boost Converter
*  PM_SLEEP_SRC_VD                  High and Low Voltage Detection (HVI, LVI)
*  PM_SLEEP_SRC_CTW                 Central Timewheel*
*  PM_SLEEP_SRC_ONE_PPS             One PPS*
*  PM_SLEEP_SRC_LCD                 LCD
*
*  *Note:   CTW and One PPS wakeup signals are in the same mask bit.
*           For PSoC 5, these are in a different bit (value 1024).
*
*  When specifying a Comparator as the wakeupSource an instance specific define
*  should be used that will track with the specific comparator that the instance
*  is placed into. As an example for a Comparator instance named MyComp the
*  value to OR into the mask is: MyComp_ctComp__CMP_MASK.
*
*  When CTW or One PPS is used as a wakeup source, the CyPmReadStatus()
*  function must be called upon wakeup with corresponding parameter. Please
*  refer to the CyPmReadStatus() API in the System Reference Guide for more
*  information.
*
* Return:
*  None
*
* Reentrant:
*  No
*
* Side Effects and Restrictions:
*  For PSoC 5 silicon the wakeup source is not selectable. In this case the
*  wakeupSource argument is ignored and any of the available wakeup sources will
*  wake the device.
*
*  If a wakeupTime other than NONE is specified, then upon exit the state of the
*  specified timer will be left as specified by wakeupTime with the timer
*  enabled and the interrupt disabled. Also, the ILO 1 KHz (if CTW timer is
*  used as wake up time) will be left started.
*
*  The 1 kHz ILO clock is expected to be enabled for PSoC 3 and PSoC 5LP to
*  measure Hibernate/Sleep regulator settling time after a reset. The holdoff
*  delay is measured using rising edges of the 1 kHz ILO.
*
*  For PSoC 3 silicon hardware buzz should be disabled before entering a sleep
*  power mode. It is disabled by PSoC Creator during startup.
*  If a Low Voltage Interrupt (LVI), High Voltage Interrupt (HVI) or Brown Out
*  detect (power supply supervising capabilities) are required in a design
*  during sleep, use the Central Time Wheel (CTW) to periodically wake the
*  device, perform software buzz, and refresh the supervisory services. If LVI,
*  HVI, or Brown Out is not required, then use of the CTW is not required.
*  Refer to the device errata for more information.
*
*******************************************************************************/
void CyPmSleep(uint8 wakeupTime, uint16 wakeupSource) 
{
    uint8 interruptState;

    /* Save current global interrupt enable and disable it */
    interruptState = CyEnterCriticalSection();


    #if(CY_PSOC3 || CY_PSOC5LP)

        /***********************************************************************
        * The Hibernate/Sleep regulator has a settling time after a reset.
        * During this time, the system ignores requests to enter Sleep and
        * Hibernate modes. The holdoff delay is measured using rising edges of
        * the 1 kHz ILO.
        ***********************************************************************/
        if(0u == (CY_PM_MODE_CSR_REG & CY_PM_MODE_CSR_PWRUP_PULSE_Q))
        {
            /* Disable hold off - no action on restore */
            CY_PM_PWRSYS_SLP_TR_REG &= CY_PM_PWRSYS_SLP_TR_HIBSLP_HOLDOFF_MASK;
        }
        else
        {
            /* Abort, device is not ready for low power mode entry */

            /* Restore global interrupt enable state */
            CyExitCriticalSection(interruptState);

            return;
        }

    #endif /* (CY_PSOC3 || CY_PSOC5LP) */


    #if(CY_PSOC3)

        /* Silicon Revision ID is below TO6 */
        if(CYDEV_CHIP_REV_ACTUAL < 5u)
        {
            /* Hardware buzz expected to be disabled in Sleep mode */
            CYASSERT(0u == (CY_PM_PWRSYS_WAKE_TR2_REG & CY_PM_PWRSYS_WAKE_TR2_EN_BUZZ));

            /* LVI/HVI requires hardware buzz to be enabled */
            if(0u != (CY_PM_RESET_CR1_REG & (CY_PM_RESET_CR1_HVIA_EN |
                  CY_PM_RESET_CR1_LVIA_EN | CY_PM_RESET_CR1_LVID_EN)))
            {
                CYASSERT(0u != 0u);
            }
        }

    #endif /* (CY_PSOC3) */


    /*******************************************************************************
    * For ARM-based devices, an interrupt is required for the CPU to wake up. The
    * Power Management implementation assumes that wakeup time is configured with a
    * separate component (component-based wakeup time configuration) for an
    * interrupt to be issued on terminal count. For more information, refer to the
    * Wakeup Time Configuration section of System Reference Guide.
    *******************************************************************************/
    #if(CY_PSOC5)

        /* Arguments expected to be 0 */
        CYASSERT(PM_SLEEP_TIME_NONE == wakeupTime);

        #if(CY_PSOC5A)

            /* The wakeupSource argument expected to be 0 */
            CYASSERT(PM_SLEEP_SRC_NONE  == wakeupSource);

            if(0u != wakeupSource)
            {
                /* To remove unreferenced local variable warning */
            }

        #endif /* (CY_PSOC5A) */

        if(0u != wakeupTime)
        {
            /* To remove unreferenced local variable warning */
        }

    #endif /* (CY_PSOC5) */


    /* Prepare hardware for Sleep mode */
    CyPmSlpSaveSet();


    #if(CY_PSOC3)

        /* CTW - save current and set new configuration */
        if((wakeupTime >= PM_SLEEP_TIME_CTW_2MS) && (wakeupTime <= PM_SLEEP_TIME_CTW_4096MS))
        {
            /* Save current and set new configuration of the CTW */
            CyPmCtwSetInterval((uint8)(wakeupTime - 1u));

            /* Include associated timer to the wakeupSource */
            wakeupSource |= PM_SLEEP_SRC_CTW;
        }

        /* 1PPS - save current and set new configuration */
        if(PM_SLEEP_TIME_ONE_PPS == wakeupTime)
        {
            /* Save current and set new configuration of the 1PPS */
            CyPmOppsSet();

            /* Include associated timer to the wakeupSource */
            wakeupSource |= PM_SLEEP_SRC_ONE_PPS;
        }

    #endif /* (CY_PSOC3) */


    #if(!CY_PSOC5A)

        /* Save and set new wake up configuration */

        /* Interrupt, PICU, I2C, Boost converter, CTW/1PPS */
        cyPmBackup.wakeupCfg0 = CY_PM_WAKEUP_CFG0_REG;
        CY_PM_WAKEUP_CFG0_REG = (uint8) (wakeupSource >> 4u);

        /* Comparators */
        cyPmBackup.wakeupCfg1 = CY_PM_WAKEUP_CFG1_REG;
        CY_PM_WAKEUP_CFG1_REG = (((uint8) wakeupSource) & CY_PM_WAKEUP_SRC_CMPS_MASK);

        /* LCD */
        cyPmBackup.wakeupCfg2 = CY_PM_WAKEUP_CFG2_REG;
        CY_PM_WAKEUP_CFG2_REG = ((uint8) ((wakeupSource >> 12u) & 0x01u));

    #endif /* (!CY_PSOC5A) */


    /*******************************************************************
    * Do not use merge region below unless any component datasheet
    * suggest to do so.
    *******************************************************************/
    /* `#START CY_PM_JUST_BEFORE_SLEEP` */

    /* `#END` */


    /* Last moment IMO frequency change */
    if(0u == (CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_FREQ_MASK))
    {
        /* IMO frequency is 12 MHz */
        cyPmBackup.imoActFreq12Mhz = CY_PM_ENABLED;
    }
    else
    {
        /* IMO frequency is not 12 MHz */
        cyPmBackup.imoActFreq12Mhz = CY_PM_DISABLED;

        /* Save IMO frequency */
        cyPmBackup.imoActFreq = CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_FREQ_MASK;

        /* Set IMO frequency to 12 MHz */
        CY_PM_FASTCLK_IMO_CR_REG &= ((uint8) (~CY_PM_FASTCLK_IMO_CR_FREQ_MASK));
    }

    /* Switch to the Sleep mode */
    CY_PM_MODE_CSR_REG = ((CY_PM_MODE_CSR_REG & ((uint8)(~CY_PM_MODE_CSR_MASK))) | CY_PM_MODE_CSR_SLEEP);

    /* Recommended readback. */
    (void) CY_PM_MODE_CSR_REG;

    /* Two recommended NOPs to get into the mode. */
    CY_NOP;
    CY_NOP;

    /* Execute WFI instruction (for ARM-based devices only) */
    CY_PM_WFI;

    /* Point of return from Sleep Mode */

    /* Restore last moment IMO frequency change */
    if(CY_PM_ENABLED != cyPmBackup.imoActFreq12Mhz)
    {
        CY_PM_FASTCLK_IMO_CR_REG  = (CY_PM_FASTCLK_IMO_CR_REG & ((uint8)(~CY_PM_FASTCLK_IMO_CR_FREQ_MASK))) |
                                    cyPmBackup.imoActFreq;
    }


    /*******************************************************************
    * Do not use merge region below unless any component datasheet
    * suggest to do so.
    *******************************************************************/
    /* `#START CY_PM_JUST_AFTER_WAKEUP_FROM_SLEEP` */

    /* `#END` */


    /* Restore hardware configuration */
    CyPmSlpRestore();


    #if(!CY_PSOC5A)

        /* Restore current wake up configuration */
        CY_PM_WAKEUP_CFG0_REG = cyPmBackup.wakeupCfg0;
        CY_PM_WAKEUP_CFG1_REG = cyPmBackup.wakeupCfg1;
        CY_PM_WAKEUP_CFG2_REG = cyPmBackup.wakeupCfg2;

    #endif /* (!CY_PSOC5A) */

    /* Restore global interrupt enable state */
    CyExitCriticalSection(interruptState);
}


/*******************************************************************************
* Function Name: CyPmHibernate
********************************************************************************
*
* Summary:
*  Puts the part into the Hibernate state.
*
*  PSoC 3 and PSoC 5LP:
*  Before switching to Hibernate, the current status of the PICU wakeup source
*  bit is saved and then set. This configures the device to wake up from the
*  PICU. Make sure you have at least one pin configured to generate a PICU
*  interrupt. For pin Px.y, the register "PICU_INTTYPE_PICUx_INTTYPEy" controls
*  the PICU behavior. In the TRM, this register is "PICU[0..15]_INTTYPE[0..7]."
*  In the Pins component datasheet, this register is referred to as the IRQ
*  option. Once the wakeup occurs, the PICU wakeup source bit is restored and
*  the PSoC returns to the Active state.
*
*  PSoC 5:
*  The only method supported for waking up from the Hibernate state is a
*  hardware reset of the device.  The PICU wakeup source cannot be used
*  reliably, so the PICU interrupt sources are automatically disabled by this
*  function before putting the device into the Hibernate state.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Reentrant:
*  No
*
* Side Effects:
*  Applications must wait 20 us before re-entering hibernate or sleep after
*  waking up from hibernate. The 20 us allows the sleep regulator time to
*  stabilize before the next hibernate / sleep event occurs. The 20 us
*  requirement begins when the device wakes up. There is no hardware check that
*  this requirement is met. The specified delay should be done on ISR entry.
*
*  After wakeup PICU interrupt occurs, the Pin_ClearInterrupt() (where Pin is
*  instance name of the Pins component) function must be called to clear the
*  latched pin events to allow proper Hibernate mode entry andd to enable
*  detection of future events.
*
*  The 1 kHz ILO clock is expected to be enabled for PSoC 3 and PSoC 5LP to
*  measure Hibernate/Sleep regulator settling time after a reset. The holdoff
*  delay is measured using rising edges of the 1 kHz ILO.
*
*******************************************************************************/
void CyPmHibernate(void) 
{
    uint8 interruptState;

    /* Save current global interrupt enable and disable it */
    interruptState = CyEnterCriticalSection();

    #if(CY_PSOC3 || CY_PSOC5LP)

        /***********************************************************************
        * The Hibernate/Sleep regulator has a settling time after a reset.
        * During this time, the system ignores requests to enter Sleep and
        * Hibernate modes. The holdoff delay is measured using rising edges of
        * the 1 kHz ILO.
        ***********************************************************************/
        if(0u == (CY_PM_MODE_CSR_REG & CY_PM_MODE_CSR_PWRUP_PULSE_Q))
        {
            /* Disable hold off - no action on restore */
            CY_PM_PWRSYS_SLP_TR_REG &= CY_PM_PWRSYS_SLP_TR_HIBSLP_HOLDOFF_MASK;
        }
        else
        {
            /* Abort, device is not ready for low power mode entry */

            /* Restore global interrupt enable state */
            CyExitCriticalSection(interruptState);

            return;
        }

    #endif /* (CY_PSOC3 || CY_PSOC5LP) */

    /* Prepare device for proper Hibernate mode entry */
    CyPmHibSaveSet();


    #if(!CY_PSOC5A)

        /* Save and enable only wakeup on PICU */
        cyPmBackup.wakeupCfg0 = CY_PM_WAKEUP_CFG0_REG;
        CY_PM_WAKEUP_CFG0_REG = CY_PM_WAKEUP_PICU;

        cyPmBackup.wakeupCfg1 = CY_PM_WAKEUP_CFG1_REG;
        CY_PM_WAKEUP_CFG1_REG = 0x00u;

        cyPmBackup.wakeupCfg2 = CY_PM_WAKEUP_CFG2_REG;
        CY_PM_WAKEUP_CFG2_REG = 0x00u;

    #endif  /* (!CY_PSOC5A) */


    /* Last moment IMO frequency change */
    if(0u == (CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_FREQ_MASK))
    {
        /* IMO frequency is 12 MHz */
        cyPmBackup.imoActFreq12Mhz = CY_PM_ENABLED;
    }
    else
    {
        /* IMO frequency is not 12 MHz */
        cyPmBackup.imoActFreq12Mhz = CY_PM_DISABLED;

        /* Save IMO frequency */
        cyPmBackup.imoActFreq = CY_PM_FASTCLK_IMO_CR_REG & CY_PM_FASTCLK_IMO_CR_FREQ_MASK;

        /* Set IMO frequency to 12 MHz */
        CY_PM_FASTCLK_IMO_CR_REG &= ((uint8) (~CY_PM_FASTCLK_IMO_CR_FREQ_MASK));
    }


    /* Switch to Hibernate Mode */
    CY_PM_MODE_CSR_REG = (CY_PM_MODE_CSR_REG & ((uint8) (~CY_PM_MODE_CSR_MASK))) | CY_PM_MODE_CSR_HIBERNATE;

    /* Recommended readback. */
    (void) CY_PM_MODE_CSR_REG;

    /* Two recommended NOPs to get into the mode. */
    CY_NOP;
    CY_NOP;

    /* Execute WFI instruction (for ARM-based devices only) */
    CY_PM_WFI;

    /* Point of return from Hibernate mode */


    /* Restore last moment IMO frequency change */
    if(CY_PM_ENABLED != cyPmBackup.imoActFreq12Mhz)
    {
        CY_PM_FASTCLK_IMO_CR_REG  = (CY_PM_FASTCLK_IMO_CR_REG & ((uint8)(~CY_PM_FASTCLK_IMO_CR_FREQ_MASK))) |
                                    cyPmBackup.imoActFreq;
    }


    /* Restore device for proper Hibernate mode exit*/
    CyPmHibRestore();


    #if(!CY_PSOC5A)

        /* Restore current wake up configuration */
        CY_PM_WAKEUP_CFG0_REG = cyPmBackup.wakeupCfg0;
        CY_PM_WAKEUP_CFG1_REG = cyPmBackup.wakeupCfg1;
        CY_PM_WAKEUP_CFG2_REG = cyPmBackup.wakeupCfg2;

    #endif /* (!CY_PSOC5A) */

    /* Restore global interrupt enable state */
    CyExitCriticalSection(interruptState);
}


/*******************************************************************************
* Function Name: CyPmReadStatus
********************************************************************************
*
* Summary:
*  Manages the Power Manager Interrupt Status Register.  This register has the
*  interrupt status for the one pulse per second, central timewheel and fast
*  timewheel timers.  This hardware register clears on read.  To allow for only
*  clearing the bits of interest and preserving the other bits, this function
*  uses a shadow register that retains the state.  This function reads the
*  status register and ORs that value with the shadow register.  That is the
*  value that is returned.  Then the bits in the mask that are set are cleared
*  from this value and written back to the shadow register.
*
*  Note You must call this function within 1 ms (1 clock cycle of the ILO)
*  after a CTW event has occurred.
*
* Parameters:
*  mask: Bits in the shadow register to clear.
*
*       Define                      Source
*  CY_PM_FTW_INT                Fast Timewheel
*  CY_PM_CTW_INT                Central Timewheel
*  CY_PM_ONEPPS_INT             One Pulse Per Second
*
* Return:
*  Status.  Same bits values as the mask parameter.
*
*******************************************************************************/
uint8 CyPmReadStatus(uint8 mask) 
{
    static uint8 interruptStatus;
    uint8 interruptState;
    uint8 tmpStatus;

    /* Enter critical section */
    interruptState = CyEnterCriticalSection();

    /* Save value of the register, copy it and clear desired bit */
    interruptStatus |= CY_PM_INT_SR_REG;
    tmpStatus = interruptStatus;
    interruptStatus &= ((uint8)(~mask));

    /* Exit critical section */
    CyExitCriticalSection(interruptState);

    return(tmpStatus);
}


/*******************************************************************************
* Function Name: CyPmHibSaveSet
********************************************************************************
*
* Summary:
*  Prepare device for proper Hibernate low power mode entry:
*  - Disables I2C backup regulator
*  - Save state of I2C backup regulator (PSoC 5)
*  - Saves ILO power down mode state and enable it (all but PSoC 5)
*  - Saves state of 1 kHz and 100 kHz ILO and disable them (all but PSoC 5)
*  - Disables sleep regulator and shorts vccd to vpwrsleep (all but PSoC 5)
*  - Save LVI/HVI configuration and disable them - CyPmHviLviSaveDisable()
*  - CyPmHibSlpSaveSet() function is called
*
* Parameters:
*  None
*
* Return:
*  None
*
* Reentrant:
*  No
*
*******************************************************************************/
static void CyPmHibSaveSet(void) 
{
    /* I2C backup reg must be off when the sleep regulator is unavailable */
    if(0u != (CY_PM_PWRSYS_CR1_REG & CY_PM_PWRSYS_CR1_I2CREG_BACKUP))
    {
        /***********************************************************************
        * If I2C backup regulator is enabled, all the fixed-function registers
        * store their values while device is in low power mode, otherwise their
        * configuration is lost. The I2C API makes a decision to restore or not
        * to restore I2C registers based on this. If this regulator will be
        * disabled and then enabled, I2C API will suppose that I2C block
        * registers preserved their values, while this is not true. So, the
        * backup regulator is disabled. And its value is restored only for
        * and PSoC 5 devices. The I2C sleep APIs is responsible for restoration.
        ***********************************************************************/

        #if(CY_PSOC5A)

            cyPmBackup.i2cRegBackup = CY_PM_ENABLED;

        #endif /* (CY_PSOC5A) */

        /* Disable I2C backup register */
        CY_PM_PWRSYS_CR1_REG &= ((uint8)(~CY_PM_PWRSYS_CR1_I2CREG_BACKUP));
    }
    else
    {
        #if(CY_PSOC5A)

            /* Save disabled state of the I2C backup regulator */
            cyPmBackup.i2cRegBackup = CY_PM_DISABLED;

        #endif /* (CY_PSOC5A) */
    }


    #if(!CY_PSOC5A)

        /* Save current ILO power mode and ensure low power mode */
        cyPmBackup.iloPowerMode = CyILO_SetPowerMode(CY_PM_POWERDOWN_MODE);

        /* Save current 1kHz ILO enable state. Disabled automatically. */
        cyPmBackup.ilo1kEnable = (0u == (CY_PM_SLOWCLK_ILO_CR0_REG & CY_PM_ILO_CR0_EN_1K)) ?
                                    CY_PM_DISABLED : CY_PM_ENABLED;

        /* Save current 100kHz ILO enable state. Disabled automatically. */
        cyPmBackup.ilo100kEnable = (0u == (CY_PM_SLOWCLK_ILO_CR0_REG & CY_PM_ILO_CR0_EN_100K)) ?
                                    CY_PM_DISABLED : CY_PM_ENABLED;


        /* Disable the sleep regulator and shorts vccd to vpwrsleep */
        if(0u == (CY_PM_PWRSYS_SLP_TR_REG & CY_PM_PWRSYS_SLP_TR_BYPASS))
        {
            /* Save current bypass state */
            cyPmBackup.slpTrBypass = CY_PM_DISABLED;
            CY_PM_PWRSYS_SLP_TR_REG |= CY_PM_PWRSYS_SLP_TR_BYPASS;
        }
        else
        {
            cyPmBackup.slpTrBypass = CY_PM_ENABLED;
        }

        /* LPCOMPs are always enabled (even when BOTH ext_vccd=1 and ext_vcca=1)*/

    #endif /* (!CY_PSOC5A) */


    /* Device is PSoC 5 and the revision is ES1 or earlier. */
    #if(CY_PSOC5A)

        /* Disable all the PICU interrupts */
        CyPmDisablePicuInterrupts();

    #endif  /* (CY_PSOC5A) */


    /***************************************************************************
    * LVI/HVI must be disabled in Hibernate
    ***************************************************************************/

    /* Save LVI/HVI configuration and disable them */
    CyPmHviLviSaveDisable();


    /* Make the same preparations for Hibernate and Sleep modes */
    CyPmHibSlpSaveSet();


    /***************************************************************************
    * Save and set power mode wakeup trim registers
    ***************************************************************************/
    #if(CY_PSOC3 || CY_PSOC5LP)

        cyPmBackup.wakeupTrim0 = CY_PM_PWRSYS_WAKE_TR0_REG;
        cyPmBackup.wakeupTrim1 = CY_PM_PWRSYS_WAKE_TR1_REG;

        CY_PM_PWRSYS_WAKE_TR0_REG = CY_PM_PWRSYS_WAKE_TR0;
        CY_PM_PWRSYS_WAKE_TR1_REG = CY_PM_PWRSYS_WAKE_TR1;

    #endif /* (CY_PSOC3 || CY_PSOC5LP) */

}


/*******************************************************************************
* Function Name: CyPmHibRestore
********************************************************************************
*
* Summary:
*  Restore device for proper Hibernate mode exit:
*  - Restore LVI/HVI configuration - call CyPmHviLviRestore()
*  - CyPmHibSlpSaveRestore() function is called
*  - Restores state of I2C backup regulator (PSoC 5)
*  - Restores ILO power down mode state and enable it (all but PSoC 5)
*  - Restores state of 1 kHz and 100 kHz ILO and disable them (all but PSoC 5)
*  - Restores sleep regulator settings (all but PSoC 5)
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void CyPmHibRestore(void) 
{
    /* Restore LVI/HVI configuration */
    CyPmHviLviRestore();

    /* Restore the same configuration for Hibernate and Sleep modes */
    CyPmHibSlpRestore();

    #if(CY_PSOC5A)

        /* Restore I2C backup regulator configuration */
        if(CY_PM_ENABLED == cyPmBackup.i2cRegBackup)
        {
            /* Enable I2C backup regulator state */
            CY_PM_PWRSYS_CR1_REG |= CY_PM_PWRSYS_CR1_I2CREG_BACKUP;
        }

    #endif /* (CY_PSOC5A) */


    #if(!CY_PSOC5A)

        /* Restore 1kHz ILO enable state */
        if(CY_PM_ENABLED == cyPmBackup.ilo1kEnable)
        {
            /* Enable 1kHz ILO */
            CyILO_Start1K();
        }

        /* Restore 100kHz ILO enable state */
        if(CY_PM_ENABLED == cyPmBackup.ilo100kEnable)
        {
            /* Enable 100kHz ILO */
            CyILO_Start100K();
        }

        /* Restore ILO power mode */
        (void) CyILO_SetPowerMode(cyPmBackup.iloPowerMode);


        if(CY_PM_DISABLED == cyPmBackup.slpTrBypass)
        {
            /* Enable the sleep regulator */
            CY_PM_PWRSYS_SLP_TR_REG &= ((uint8)(~CY_PM_PWRSYS_SLP_TR_BYPASS));
        }

    #endif /* (!CY_PSOC5A) */


    /***************************************************************************
    * Restore power mode wakeup trim registers
    ***************************************************************************/
    #if(CY_PSOC3 || CY_PSOC5LP)

        CY_PM_PWRSYS_WAKE_TR0_REG = cyPmBackup.wakeupTrim0;
        CY_PM_PWRSYS_WAKE_TR1_REG = cyPmBackup.wakeupTrim1;

    #endif /* (CY_PSOC3 || CY_PSOC5LP) */
}


/*******************************************************************************
* Function Name: CyPmCtwSetInterval
********************************************************************************
*
* Summary:
*  Performs CTW configuration:
*  - Disables CTW interrupt
*  - Enables 1 kHz ILO
*  - Sets new CTW interval
*
* Parameters:
*  ctwInterval: the CTW interval to be set.
*
* Return:
*  None
*
* Side Effects:
*  Enables ILO 1 KHz clock and leaves it enabled.
*
*******************************************************************************/
void CyPmCtwSetInterval(uint8 ctwInterval) 
{
    /* Disable CTW interrupt enable */
    CY_PM_TW_CFG2_REG &= ((uint8)(~CY_PM_CTW_IE));

    /* Enable 1kHz ILO (required for CTW operation) */
    CyILO_Start1K();

    /* Interval could be set only while CTW is disabled */
    if(0u != (CY_PM_TW_CFG2_REG & CY_PM_CTW_EN))
    {
        /* Set CTW interval if needed */
        if(CY_PM_TW_CFG1_REG != ctwInterval)
        {
            /* Disable the CTW, set new CTW interval and enable it again */
            CY_PM_TW_CFG2_REG &= ((uint8)(~CY_PM_CTW_EN));
            CY_PM_TW_CFG1_REG = ctwInterval;
            CY_PM_TW_CFG2_REG |= CY_PM_CTW_EN;
        }   /* Required interval is already set */
    }
    else
    {
        /* Set CTW interval if needed */
        if(CY_PM_TW_CFG1_REG != ctwInterval)
        {
            /* Set the new CTW interval. Could be changed if CTW is disabled */
            CY_PM_TW_CFG1_REG = ctwInterval;
        }   /* Required interval is already set */

        /* Enable the CTW */
        CY_PM_TW_CFG2_REG |= CY_PM_CTW_EN;
    }
}


/*******************************************************************************
* Function Name: CyPmOppsSet
********************************************************************************
*
* Summary:
*  Performs 1PPS configuration:
*  - Starts 32 KHz XTAL
*  - Disables 1PPS interupts
*  - Enables 1PPS
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void CyPmOppsSet(void) 
{
    /* Enable 32kHz XTAL if needed */
    if(0u == (CY_PM_SLOWCLK_X32_CR_REG & CY_PM_X32_CR_X32EN))
    {
        /* Enable 32kHz XTAL */
        CyXTAL_32KHZ_Start();
    }

    /* Disable 1PPS interrupt enable */
    CY_PM_TW_CFG2_REG &= ((uint8)(~CY_PM_1PPS_IE));

    /* Enable 1PPS operation */
    CY_PM_TW_CFG2_REG |= CY_PM_1PPS_EN;
}


/*******************************************************************************
* Function Name: CyPmFtwSetInterval
********************************************************************************
*
* Summary:
*  Performs FTW configuration:
*  - Disables FTW interrupt
*  - Enables 100 kHz ILO
*  - Sets new FTW interval.
*
* Parameters:
*  ftwInterval - FTW counter interval.
*
* Return:
*  None
*
* Side Effects:
*  Enables ILO 100 KHz clock and leaves it enabled.
*
*******************************************************************************/
void CyPmFtwSetInterval(uint8 ftwInterval) 
{
    /* Disable FTW interrupt enable */
    CY_PM_TW_CFG2_REG &= ((uint8)(~CY_PM_FTW_IE));

    /* Enable 100kHz ILO */
    CyILO_Start100K();

    /* Iterval could be set only while FTW is disabled */
    if(0u != (CY_PM_TW_CFG2_REG & CY_PM_FTW_EN))
    {
        /* Disable FTW, set new FTW interval if needed and enable it again */
        if(CY_PM_TW_CFG0_REG != ftwInterval)
        {
            /* Disable the CTW, set new CTW interval and enable it again */
            CY_PM_TW_CFG2_REG &= ((uint8)(~CY_PM_FTW_EN));
            CY_PM_TW_CFG0_REG = ftwInterval;
            CY_PM_TW_CFG2_REG |= CY_PM_FTW_EN;
        }   /* Required interval is already set */
    }
    else
    {
        /* Set new FTW counter interval if needed. FTW is disabled. */
        if(CY_PM_TW_CFG0_REG != ftwInterval)
        {
            /* Set the new CTW interval. Could be changed if CTW is disabled */
            CY_PM_TW_CFG0_REG = ftwInterval;
        }   /* Required interval is already set */

        /* Enable the FTW */
        CY_PM_TW_CFG2_REG |= CY_PM_FTW_EN;
    }
}


#if(CY_PSOC5A)

    /*******************************************************************************
    * Function Name: CyPmSavePicuInterrupts
    ********************************************************************************
    *
    * Summary:
    *  Saves PICU interrupt type registers to the backup structure.
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    * Reentrant:
    *  No
    *
    *******************************************************************************/
    static void CyPmSavePicuInterrupts(void) 
    {
        /* Save all the PICU interrupt type */
        (void) memcpy((void *) &cyPmBackup.picuIntType[0u],
                      (void *) CY_PM_PICU_0_6_INT_BASE,
                      CY_PM_PICU_0_6_INT_SIZE);

        (void) memcpy((void *) &cyPmBackup.picuIntType[CY_PM_PICU_0_6_INT_SIZE],
                      (void *) CY_PM_PICU_12_INT_BASE,
                      CY_PM_PICU_12_INT_SIZE);

        (void) memcpy((void *) &cyPmBackup.picuIntType[CY_PM_PICU_0_6_INT_SIZE + CY_PM_PICU_12_INT_SIZE],
                      (void *) CY_PM_PICU_15_INT_BASE,
                      CY_PM_PICU_15_INT_SIZE);

    }


    /*******************************************************************************
    * Function Name: CyPmDisablePicuInterrupts
    ********************************************************************************
    *
    * Summary:
    *  Disableds PICU interrupts.
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    *******************************************************************************/
    static void CyPmDisablePicuInterrupts(void) 
    {
        /* Disable all the PICU interrupts */
        (void) memset((void *) CY_PM_PICU_0_6_INT_BASE, 0, CY_PM_PICU_0_6_INT_SIZE);
        (void) memset((void *) CY_PM_PICU_12_INT_BASE,  0, CY_PM_PICU_12_INT_SIZE );
        (void) memset((void *) CY_PM_PICU_15_INT_BASE,  0, CY_PM_PICU_15_INT_SIZE );
    }


    /*******************************************************************************
    * Function Name: CyPmRestorePicuInterrupts
    ********************************************************************************
    *
    * Summary:
    *  Restores PICU interrupt type registers from the backup structure.
    *
    * Parameters:
    *  None
    *
    * Return:
    *  None
    *
    *******************************************************************************/
    static void CyPmRestorePicuInterrupts(void) 
    {
        /* Save all the PICU interrupt type */
        (void) memcpy((void *) CY_PM_PICU_0_6_INT_BASE,
                      (void *) &cyPmBackup.picuIntType[0u],
                      CY_PM_PICU_0_6_INT_SIZE);

        (void) memcpy((void *) CY_PM_PICU_12_INT_BASE,
                      (void *) &cyPmBackup.picuIntType[CY_PM_PICU_0_6_INT_SIZE],
                      CY_PM_PICU_12_INT_SIZE);

        (void) memcpy((void *) CY_PM_PICU_15_INT_BASE,
                      (void *) &cyPmBackup.picuIntType[CY_PM_PICU_0_6_INT_SIZE + CY_PM_PICU_12_INT_SIZE],
                      CY_PM_PICU_15_INT_SIZE);

    }

#endif  /* (CY_PSOC5A) */


/*******************************************************************************
* Function Name: CyPmSlpSaveSet
********************************************************************************
*
* Summary:
*  Prepare device for proper Sleep low power mode entry:
*  - Prepare CTW for Sleep mode entry (PSoC 5)
*    * Save timewheels configuration
*    * Disable FTW and 1PPS (enable and interrupt)
*    * Reset CTW
*    * Save and disable PICU interrupts
*    * Save and disable PRES-A and PRES-D
*  - Save and disable LVI/HVI configuration (PSoC 5)
*  - Save and set to max buzz interval (PSoC 5)
*  - CyPmHibSlpSaveSet() function is called
*
* Parameters:
*  None
*
* Return:
*  None
*
* Reentrant:
*  No
*
*******************************************************************************/
static void CyPmSlpSaveSet(void) 
{
    #if(CY_PSOC5A)

        /* Preserve the Timewheel Configuration Register 2 */
        cyPmBackup.pmTwCfg2 = CY_PM_TW_CFG2_REG;

        /*  Clear the enable and interrupt enables for the FTW and ONEPPS */
        CY_PM_TW_CFG2_REG &= ((uint8)(~(CY_PM_FTW_IE | CY_PM_FTW_EN | CY_PM_1PPS_EN | CY_PM_1PPS_IE)));

        /* Reset free-running CTW counter to 0 and held it there */
        CY_PM_WDT_CFG_REG |= CY_PM_WDT_CFG_CTW_RESET;

        /* Exit CTW counter reset state */
        CY_PM_WDT_CFG_REG &= ((uint8)(~CY_PM_WDT_CFG_CTW_RESET));

        /* Save and disable PICU interrupts  */
        CyPmSavePicuInterrupts();
        CyPmDisablePicuInterrupts();

        /* Save and disable PRES-A and PRES-D */
        cyPmBackup.pres1 = CY_PM_RESET_CR1_REG & CY_PM_RESET_CR1_DIS_PRES1;
        cyPmBackup.pres2 = CY_PM_RESET_CR3_REG & CY_PM_RESET_CR3_DIS_PRES2;
        CY_PM_RESET_CR1_REG &= ((uint8)(~CY_PM_RESET_CR1_DIS_PRES1));
        CY_PM_RESET_CR3_REG &= ((uint8)(~CY_PM_RESET_CR3_DIS_PRES2));

    #endif /* (CY_PSOC5A) */


    #if(CY_PSOC5A)

        /***************************************************************************
        * LVI/HVI must be disabled as it doesn't work during buzzing.
        *
        * Using hardware buzz in conjunction with other device wakeup sources
        * can cause the device to lockup, halting further code execution. The
        * hardware buzz provides power supply supervising capability in sleep.
        * It is enabled by default and there is no way to disable it. So the buzz
        * interval is set to maximum (512 ms). The CTW must be configured to wake up
        * at a rate less than hardware buzz interval.
        ***************************************************************************/

        /* Save and disable LVI/HVI */
        CyPmHviLviSaveDisable();

        /* Save buzz trim value */
        cyPmBackup.buzzSleepTrim = CY_PM_PWRSYS_BUZZ_TR_REG & ((uint8)(~CY_PM_PWRSYS_BUZZ_TR_MASK));

        /* Set buzz interval to maximum */
        CY_PM_PWRSYS_BUZZ_TR_REG = CY_PM_PWRSYS_BUZZ_TR_512_TICKS |
                                  (CY_PM_PWRSYS_BUZZ_TR_REG & CY_PM_PWRSYS_BUZZ_TR_MASK);

    #endif  /* (CY_PSOC5A) */


    /* Apply configuration that are same for Sleep and Hibernate */
    CyPmHibSlpSaveSet();
}


/*******************************************************************************
* Function Name: CyPmSlpRestore
********************************************************************************
*
* Summary:
*  Restore device for proper Sleep mode exit:
*  - Restore timewheel configuration (PSoC 5)
*  - Restore PRES-A and PRES-D (PSoC 5)
*  - Restore PICU interrupts (PSoC 5)
*  - Restore buzz sleep trim value (PSoC 5)
*  - Call to CyPmHibSlpSaveRestore()
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void CyPmSlpRestore(void) 
{
    #if(CY_PSOC5A)

        /* Restore the Timewheel Configuration Register 2 */
        CY_PM_TW_CFG2_REG = cyPmBackup.pmTwCfg2;

        /* Restore PICU interrupts */
        CyPmRestorePicuInterrupts();

        /* Restore PRES-A and PRES-D (assumed they were disabled) */
        CY_PM_RESET_CR1_REG |= cyPmBackup.pres1;
        CY_PM_RESET_CR3_REG |= cyPmBackup.pres2;

    #endif /* (CY_PSOC5A) */


    #if(CY_PSOC5A)

        /* Restore LVI/HVI configuration */
        CyPmHviLviRestore();

        /* Restore buzz sleep trim value */
        CY_PM_PWRSYS_BUZZ_TR_REG = cyPmBackup.buzzSleepTrim |
                                  (CY_PM_PWRSYS_BUZZ_TR_REG & CY_PM_PWRSYS_BUZZ_TR_MASK);

    #endif  /* (CY_PSOC5A) */


    /* Restore configuration that are same for Sleep and Hibernate */
    CyPmHibSlpRestore();
}


/*******************************************************************************
* Function Name: CyPmHibSlpSaveSet
********************************************************************************
*
* Summary:
*  This API is used for preparing device for Sleep and Hibernate low power
*  modes entry:
*  - Saves COMP, VIDAC, DSM and SAR routing connections (PSoC 5)
*  - Saves SC/CT routing connections (PSoC 3/5/5LP)
*  - Disables Serial Wire Viewer (SWV) (PSoC 3)
*  - Save boost reference selection and set it to internal
*
* Parameters:
*  None
*
* Return:
*  None
*
* Reentrant:
*  No
*
*******************************************************************************/
static void CyPmHibSlpSaveSet(void) 
{
    #if(CY_PSOC5A)

        /* Save CMP routing registers */
        cyPmBackup.cmpData[0u]  = CY_GET_REG8(CYREG_CMP0_SW0);
        cyPmBackup.cmpData[1u]  = CY_GET_REG8(CYREG_CMP0_SW2);
        cyPmBackup.cmpData[2u]  = CY_GET_REG8(CYREG_CMP0_SW3);
        cyPmBackup.cmpData[3u]  = CY_GET_REG8(CYREG_CMP0_SW4);
        cyPmBackup.cmpData[4u]  = CY_GET_REG8(CYREG_CMP0_SW6);

        cyPmBackup.cmpData[5u]  = CY_GET_REG8(CYREG_CMP1_SW0);
        cyPmBackup.cmpData[6u]  = CY_GET_REG8(CYREG_CMP1_SW2);
        cyPmBackup.cmpData[7u]  = CY_GET_REG8(CYREG_CMP1_SW3);
        cyPmBackup.cmpData[8u]  = CY_GET_REG8(CYREG_CMP1_SW4);
        cyPmBackup.cmpData[9u]  = CY_GET_REG8(CYREG_CMP1_SW6);

        cyPmBackup.cmpData[10u] = CY_GET_REG8(CYREG_CMP2_SW0);
        cyPmBackup.cmpData[11u] = CY_GET_REG8(CYREG_CMP2_SW2);
        cyPmBackup.cmpData[12u] = CY_GET_REG8(CYREG_CMP2_SW3);
        cyPmBackup.cmpData[13u] = CY_GET_REG8(CYREG_CMP2_SW4);
        cyPmBackup.cmpData[14u] = CY_GET_REG8(CYREG_CMP2_SW6);

        cyPmBackup.cmpData[15u] = CY_GET_REG8(CYREG_CMP3_SW0);
        cyPmBackup.cmpData[16u] = CY_GET_REG8(CYREG_CMP3_SW2);
        cyPmBackup.cmpData[17u] = CY_GET_REG8(CYREG_CMP3_SW3);
        cyPmBackup.cmpData[18u] = CY_GET_REG8(CYREG_CMP3_SW4);
        cyPmBackup.cmpData[19u] = CY_GET_REG8(CYREG_CMP3_SW6);


        /* Clear CMP routing registers */
        CY_SET_REG8(CYREG_CMP0_SW0 , 0u);
        CY_SET_REG8(CYREG_CMP0_SW2 , 0u);
        CY_SET_REG8(CYREG_CMP0_SW3 , 0u);
        CY_SET_REG8(CYREG_CMP0_SW4 , 0u);
        CY_SET_REG8(CYREG_CMP0_SW6 , 0u);

        CY_SET_REG8(CYREG_CMP1_SW0 , 0u);
        CY_SET_REG8(CYREG_CMP1_SW2 , 0u);
        CY_SET_REG8(CYREG_CMP1_SW3 , 0u);
        CY_SET_REG8(CYREG_CMP1_SW4 , 0u);
        CY_SET_REG8(CYREG_CMP1_SW6 , 0u);

        CY_SET_REG8(CYREG_CMP2_SW0 , 0u);
        CY_SET_REG8(CYREG_CMP2_SW2 , 0u);
        CY_SET_REG8(CYREG_CMP2_SW3 , 0u);
        CY_SET_REG8(CYREG_CMP2_SW4 , 0u);
        CY_SET_REG8(CYREG_CMP2_SW6 , 0u);

        CY_SET_REG8(CYREG_CMP3_SW0 , 0u);
        CY_SET_REG8(CYREG_CMP3_SW2 , 0u);
        CY_SET_REG8(CYREG_CMP3_SW3 , 0u);
        CY_SET_REG8(CYREG_CMP3_SW4 , 0u);
        CY_SET_REG8(CYREG_CMP3_SW6 , 0u);


        /* Save DAC routing registers */
        cyPmBackup.dacData[0u]  = CY_GET_REG8(CYREG_DAC0_SW0);
        cyPmBackup.dacData[1u]  = CY_GET_REG8(CYREG_DAC0_SW2);
        cyPmBackup.dacData[2u]  = CY_GET_REG8(CYREG_DAC0_SW3);
        cyPmBackup.dacData[3u]  = CY_GET_REG8(CYREG_DAC0_SW4);

        cyPmBackup.dacData[4u]  = CY_GET_REG8(CYREG_DAC1_SW0);
        cyPmBackup.dacData[5u]  = CY_GET_REG8(CYREG_DAC1_SW2);
        cyPmBackup.dacData[6u]  = CY_GET_REG8(CYREG_DAC1_SW3);
        cyPmBackup.dacData[7u]  = CY_GET_REG8(CYREG_DAC1_SW4);

        cyPmBackup.dacData[8u]  = CY_GET_REG8(CYREG_DAC2_SW0);
        cyPmBackup.dacData[9u]  = CY_GET_REG8(CYREG_DAC2_SW2);
        cyPmBackup.dacData[10u] = CY_GET_REG8(CYREG_DAC2_SW3);
        cyPmBackup.dacData[11u] = CY_GET_REG8(CYREG_DAC2_SW4);

        cyPmBackup.dacData[12u] = CY_GET_REG8(CYREG_DAC3_SW0);
        cyPmBackup.dacData[13u] = CY_GET_REG8(CYREG_DAC3_SW2);
        cyPmBackup.dacData[14u] = CY_GET_REG8(CYREG_DAC3_SW3);
        cyPmBackup.dacData[15u] = CY_GET_REG8(CYREG_DAC3_SW4);

        /* Clear DAC routing registers */
        CY_SET_REG8(CYREG_DAC0_SW0 , 0u);
        CY_SET_REG8(CYREG_DAC0_SW2 , 0u);
        CY_SET_REG8(CYREG_DAC0_SW3 , 0u);
        CY_SET_REG8(CYREG_DAC0_SW4 , 0u);

        CY_SET_REG8(CYREG_DAC1_SW0 , 0u);
        CY_SET_REG8(CYREG_DAC1_SW2 , 0u);
        CY_SET_REG8(CYREG_DAC1_SW3 , 0u);
        CY_SET_REG8(CYREG_DAC1_SW4 , 0u);

        CY_SET_REG8(CYREG_DAC2_SW0 , 0u);
        CY_SET_REG8(CYREG_DAC2_SW2 , 0u);
        CY_SET_REG8(CYREG_DAC2_SW3 , 0u);
        CY_SET_REG8(CYREG_DAC2_SW4 , 0u);

        CY_SET_REG8(CYREG_DAC3_SW0 , 0u);
        CY_SET_REG8(CYREG_DAC3_SW2 , 0u);
        CY_SET_REG8(CYREG_DAC3_SW3 , 0u);
        CY_SET_REG8(CYREG_DAC3_SW4 , 0u);


        /* Save DSM routing registers */
        cyPmBackup.dsmData[0u]  = CY_GET_REG8(CYREG_DSM0_SW0);
        cyPmBackup.dsmData[1u]  = CY_GET_REG8(CYREG_DSM0_SW2);
        cyPmBackup.dsmData[2u]  = CY_GET_REG8(CYREG_DSM0_SW3);
        cyPmBackup.dsmData[3u]  = CY_GET_REG8(CYREG_DSM0_SW4);
        cyPmBackup.dsmData[4u]  = CY_GET_REG8(CYREG_DSM0_SW6);

        /* Clear DSM routing registers */
        CY_SET_REG8(CYREG_DSM0_SW0 , 0u);
        CY_SET_REG8(CYREG_DSM0_SW2 , 0u);
        CY_SET_REG8(CYREG_DSM0_SW3 , 0u);
        CY_SET_REG8(CYREG_DSM0_SW4 , 0u);
        CY_SET_REG8(CYREG_DSM0_SW6 , 0u);


        /* Save SAR routing registers */
        cyPmBackup.sarData[0u]  = CY_GET_REG8(CYREG_SAR0_SW0);
        cyPmBackup.sarData[1u]  = CY_GET_REG8(CYREG_SAR0_SW2);
        cyPmBackup.sarData[2u]  = CY_GET_REG8(CYREG_SAR0_SW3);
        cyPmBackup.sarData[3u]  = CY_GET_REG8(CYREG_SAR0_SW4);
        cyPmBackup.sarData[4u]  = CY_GET_REG8(CYREG_SAR0_SW6);

        cyPmBackup.sarData[5u]  = CY_GET_REG8(CYREG_SAR1_SW0);
        cyPmBackup.sarData[6u]  = CY_GET_REG8(CYREG_SAR1_SW2);
        cyPmBackup.sarData[7u]  = CY_GET_REG8(CYREG_SAR1_SW3);
        cyPmBackup.sarData[8u]  = CY_GET_REG8(CYREG_SAR1_SW4);
        cyPmBackup.sarData[9u]  = CY_GET_REG8(CYREG_SAR1_SW6);


        /* Clear SAR routing registers */
        CY_SET_REG8(CYREG_SAR0_SW0 , 0u);
        CY_SET_REG8(CYREG_SAR0_SW2 , 0u);
        CY_SET_REG8(CYREG_SAR0_SW3 , 0u);
        CY_SET_REG8(CYREG_SAR0_SW4 , 0u);
        CY_SET_REG8(CYREG_SAR0_SW6 , 0u);

        CY_SET_REG8(CYREG_SAR1_SW0 , 0u);
        CY_SET_REG8(CYREG_SAR1_SW2 , 0u);
        CY_SET_REG8(CYREG_SAR1_SW3 , 0u);
        CY_SET_REG8(CYREG_SAR1_SW4 , 0u);
        CY_SET_REG8(CYREG_SAR1_SW6 , 0u);

    #endif  /* (CY_PSOC5A) */


    #if(CY_PSOC3 || CY_PSOC5A || CY_PSOC5LP)

        /* Save SC/CT routing registers */
        cyPmBackup.scctData[0u]   = CY_GET_REG8(CYREG_SC0_SW0 );
        cyPmBackup.scctData[1u]   = CY_GET_REG8(CYREG_SC0_SW2 );
        cyPmBackup.scctData[2u]   = CY_GET_REG8(CYREG_SC0_SW3 );
        cyPmBackup.scctData[3u]   = CY_GET_REG8(CYREG_SC0_SW4 );
        cyPmBackup.scctData[4u]   = CY_GET_REG8(CYREG_SC0_SW6 );
        cyPmBackup.scctData[5u]   = CY_GET_REG8(CYREG_SC0_SW8 );
        cyPmBackup.scctData[6u]   = CY_GET_REG8(CYREG_SC0_SW10);

        cyPmBackup.scctData[7u]   = CY_GET_REG8(CYREG_SC1_SW0 );
        cyPmBackup.scctData[8u]   = CY_GET_REG8(CYREG_SC1_SW2 );
        cyPmBackup.scctData[9u]   = CY_GET_REG8(CYREG_SC1_SW3 );
        cyPmBackup.scctData[10u]  = CY_GET_REG8(CYREG_SC1_SW4 );
        cyPmBackup.scctData[11u]  = CY_GET_REG8(CYREG_SC1_SW6 );
        cyPmBackup.scctData[12u]  = CY_GET_REG8(CYREG_SC1_SW8 );
        cyPmBackup.scctData[13u]  = CY_GET_REG8(CYREG_SC1_SW10);

        cyPmBackup.scctData[14u]  = CY_GET_REG8(CYREG_SC2_SW0 );
        cyPmBackup.scctData[15u]  = CY_GET_REG8(CYREG_SC2_SW2 );
        cyPmBackup.scctData[16u]  = CY_GET_REG8(CYREG_SC2_SW3 );
        cyPmBackup.scctData[17u]  = CY_GET_REG8(CYREG_SC2_SW4 );
        cyPmBackup.scctData[18u]  = CY_GET_REG8(CYREG_SC2_SW6 );
        cyPmBackup.scctData[19u]  = CY_GET_REG8(CYREG_SC2_SW8 );
        cyPmBackup.scctData[20u]  = CY_GET_REG8(CYREG_SC2_SW10);

        cyPmBackup.scctData[21u]  = CY_GET_REG8(CYREG_SC3_SW0 );
        cyPmBackup.scctData[22u]  = CY_GET_REG8(CYREG_SC3_SW2 );
        cyPmBackup.scctData[23u]  = CY_GET_REG8(CYREG_SC3_SW3 );
        cyPmBackup.scctData[24u]  = CY_GET_REG8(CYREG_SC3_SW4 );
        cyPmBackup.scctData[25u]  = CY_GET_REG8(CYREG_SC3_SW6 );
        cyPmBackup.scctData[26u]  = CY_GET_REG8(CYREG_SC3_SW8 );
        cyPmBackup.scctData[27u]  = CY_GET_REG8(CYREG_SC3_SW10);

        CY_SET_REG8(CYREG_SC0_SW0 , 0u);
        CY_SET_REG8(CYREG_SC0_SW2 , 0u);
        CY_SET_REG8(CYREG_SC0_SW3 , 0u);
        CY_SET_REG8(CYREG_SC0_SW4 , 0u);
        CY_SET_REG8(CYREG_SC0_SW6 , 0u);
        CY_SET_REG8(CYREG_SC0_SW8 , 0u);
        CY_SET_REG8(CYREG_SC0_SW10, 0u);

        CY_SET_REG8(CYREG_SC1_SW0 , 0u);
        CY_SET_REG8(CYREG_SC1_SW2 , 0u);
        CY_SET_REG8(CYREG_SC1_SW3 , 0u);
        CY_SET_REG8(CYREG_SC1_SW4 , 0u);
        CY_SET_REG8(CYREG_SC1_SW6 , 0u);
        CY_SET_REG8(CYREG_SC1_SW8 , 0u);
        CY_SET_REG8(CYREG_SC1_SW10, 0u);

        CY_SET_REG8(CYREG_SC2_SW0 , 0u);
        CY_SET_REG8(CYREG_SC2_SW2 , 0u);
        CY_SET_REG8(CYREG_SC2_SW3 , 0u);
        CY_SET_REG8(CYREG_SC2_SW4 , 0u);
        CY_SET_REG8(CYREG_SC2_SW6 , 0u);
        CY_SET_REG8(CYREG_SC2_SW8 , 0u);
        CY_SET_REG8(CYREG_SC2_SW10, 0u);

        CY_SET_REG8(CYREG_SC3_SW0 , 0u);
        CY_SET_REG8(CYREG_SC3_SW2 , 0u);
        CY_SET_REG8(CYREG_SC3_SW3 , 0u);
        CY_SET_REG8(CYREG_SC3_SW4 , 0u);
        CY_SET_REG8(CYREG_SC3_SW6 , 0u);
        CY_SET_REG8(CYREG_SC3_SW8 , 0u);
        CY_SET_REG8(CYREG_SC3_SW10, 0u);

    #endif  /* (CY_PSOC3 || CY_PSOC5A || CY_PSOC5LP) */


    #if(CY_PSOC3)

        /* Serial Wire Viewer (SWV) workaround */

        /* Disable SWV before entering low power mode */
        if(0u != (CY_PM_MLOGIC_DBG_REG & CY_PM_MLOGIC_DBG_SWV_CLK_EN))
        {
            /* Save SWV clock enabled state */
            cyPmBackup.swvClkEnabled = CY_PM_ENABLED;

            /* Save current ports drive mode settings */
            cyPmBackup.prt1Dm = CY_PM_PRT1_PC3_REG & ((uint8)(~CY_PM_PRT1_PC3_DM_MASK));

            /* Set drive mode to strong output */
            CY_PM_PRT1_PC3_REG = (CY_PM_PRT1_PC3_REG & CY_PM_PRT1_PC3_DM_MASK) |
                                CY_PM_PRT1_PC3_DM_STRONG;

            /* Disable SWV clocks */
            CY_PM_MLOGIC_DBG_REG &= ((uint8)(~CY_PM_MLOGIC_DBG_SWV_CLK_EN));
        }
        else
        {
            /* Save SWV clock disabled state */
            cyPmBackup.swvClkEnabled = CY_PM_DISABLED;
        }

    #endif  /* (CY_PSOC3) */


    /***************************************************************************
    * Save boost reference and set it to boost's internal by clearing the bit.
    * External (chip bandgap) reference is not available in Sleep and Hibernate.
    ***************************************************************************/
    if(0u != (CY_PM_BOOST_CR2_REG & CY_PM_BOOST_CR2_EREFSEL_EXT))
    {
        cyPmBackup.boostRefExt = CY_PM_ENABLED;
        CY_PM_BOOST_CR2_REG &= ((uint8)(~CY_PM_BOOST_CR2_EREFSEL_EXT));
    }
    else
    {
        cyPmBackup.boostRefExt = CY_PM_DISABLED;
    }
}


/*******************************************************************************
* Function Name: CyPmHibSlpRestore
********************************************************************************
*
* Summary:
*  This API is used for restoring device configurations after wakeup from Sleep
*  and Hibernate low power modes:
*  - Restores COMP, VIDAC, DSM and SAR routing connections (PSoC 5)
*  - Restores SC/CT routing connections (PSoC 3/5/5LP)
*  - Restores enable state of Serial Wire Viewer (SWV) (PSoC 3)
*  - Restores initial buzz rate (PSoC 5)
*  - Restore boost reference selection
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void CyPmHibSlpRestore(void) 
{
    #if(CY_PSOC5A)

        /* Restore COMP routing registers */
        CY_SET_REG8(CYREG_CMP0_SW0 , cyPmBackup.cmpData[0u] );
        CY_SET_REG8(CYREG_CMP0_SW2 , cyPmBackup.cmpData[1u] );
        CY_SET_REG8(CYREG_CMP0_SW3 , cyPmBackup.cmpData[2u] );
        CY_SET_REG8(CYREG_CMP0_SW4 , cyPmBackup.cmpData[3u] );
        CY_SET_REG8(CYREG_CMP0_SW6 , cyPmBackup.cmpData[4u] );

        CY_SET_REG8(CYREG_CMP1_SW0 , cyPmBackup.cmpData[5u] );
        CY_SET_REG8(CYREG_CMP1_SW2 , cyPmBackup.cmpData[6u] );
        CY_SET_REG8(CYREG_CMP1_SW3 , cyPmBackup.cmpData[7u] );
        CY_SET_REG8(CYREG_CMP1_SW4 , cyPmBackup.cmpData[8u] );
        CY_SET_REG8(CYREG_CMP1_SW6 , cyPmBackup.cmpData[9u] );

        CY_SET_REG8(CYREG_CMP2_SW0 , cyPmBackup.cmpData[10u]);
        CY_SET_REG8(CYREG_CMP2_SW2 , cyPmBackup.cmpData[11u]);
        CY_SET_REG8(CYREG_CMP2_SW3 , cyPmBackup.cmpData[12u]);
        CY_SET_REG8(CYREG_CMP2_SW4 , cyPmBackup.cmpData[13u]);
        CY_SET_REG8(CYREG_CMP2_SW6 , cyPmBackup.cmpData[14u]);

        CY_SET_REG8(CYREG_CMP3_SW0 , cyPmBackup.cmpData[15u]);
        CY_SET_REG8(CYREG_CMP3_SW2 , cyPmBackup.cmpData[16u]);
        CY_SET_REG8(CYREG_CMP3_SW3 , cyPmBackup.cmpData[17u]);
        CY_SET_REG8(CYREG_CMP3_SW4 , cyPmBackup.cmpData[18u]);
        CY_SET_REG8(CYREG_CMP3_SW6 , cyPmBackup.cmpData[19u]);

        /* Restore DAC routing registers */
        CY_SET_REG8(CYREG_DAC0_SW0 , cyPmBackup.dacData[0u] );
        CY_SET_REG8(CYREG_DAC0_SW2 , cyPmBackup.dacData[1u] );
        CY_SET_REG8(CYREG_DAC0_SW3 , cyPmBackup.dacData[2u] );
        CY_SET_REG8(CYREG_DAC0_SW4 , cyPmBackup.dacData[3u] );

        CY_SET_REG8(CYREG_DAC1_SW0 , cyPmBackup.dacData[4u] );
        CY_SET_REG8(CYREG_DAC1_SW2 , cyPmBackup.dacData[5u] );
        CY_SET_REG8(CYREG_DAC1_SW3 , cyPmBackup.dacData[6u] );
        CY_SET_REG8(CYREG_DAC1_SW4 , cyPmBackup.dacData[7u] );

        CY_SET_REG8(CYREG_DAC2_SW0 , cyPmBackup.dacData[8u] );
        CY_SET_REG8(CYREG_DAC2_SW2 , cyPmBackup.dacData[9u] );
        CY_SET_REG8(CYREG_DAC2_SW3 , cyPmBackup.dacData[10u]);
        CY_SET_REG8(CYREG_DAC2_SW4 , cyPmBackup.dacData[11u]);

        CY_SET_REG8(CYREG_DAC3_SW0 , cyPmBackup.dacData[12u]);
        CY_SET_REG8(CYREG_DAC3_SW2 , cyPmBackup.dacData[13u]);
        CY_SET_REG8(CYREG_DAC3_SW3 , cyPmBackup.dacData[14u]);
        CY_SET_REG8(CYREG_DAC3_SW4 , cyPmBackup.dacData[15u]);


        /* Restore DSM routing registers */
        CY_SET_REG8(CYREG_DSM0_SW0 , cyPmBackup.dsmData[0u]);
        CY_SET_REG8(CYREG_DSM0_SW2 , cyPmBackup.dsmData[1u]);
        CY_SET_REG8(CYREG_DSM0_SW3 , cyPmBackup.dsmData[2u]);
        CY_SET_REG8(CYREG_DSM0_SW4 , cyPmBackup.dsmData[3u]);
        CY_SET_REG8(CYREG_DSM0_SW6 , cyPmBackup.dsmData[4u]);


        /* Restore SAR routing registers */
        CY_SET_REG8(CYREG_SAR0_SW0 , cyPmBackup.sarData[0u]);
        CY_SET_REG8(CYREG_SAR0_SW2 , cyPmBackup.sarData[1u]);
        CY_SET_REG8(CYREG_SAR0_SW3 , cyPmBackup.sarData[2u]);
        CY_SET_REG8(CYREG_SAR0_SW4 , cyPmBackup.sarData[3u]);
        CY_SET_REG8(CYREG_SAR0_SW6 , cyPmBackup.sarData[4u]);

        CY_SET_REG8(CYREG_SAR1_SW0 , cyPmBackup.sarData[5u]);
        CY_SET_REG8(CYREG_SAR1_SW2 , cyPmBackup.sarData[6u]);
        CY_SET_REG8(CYREG_SAR1_SW3 , cyPmBackup.sarData[7u]);
        CY_SET_REG8(CYREG_SAR1_SW4 , cyPmBackup.sarData[8u]);
        CY_SET_REG8(CYREG_SAR1_SW6 , cyPmBackup.sarData[9u]);

    #endif  /* (CY_PSOC5A) */

    #if(CY_PSOC3 || CY_PSOC5A || CY_PSOC5LP)

        /* Restore SC/CT routing registers */
        CY_SET_REG8(CYREG_SC0_SW0 , cyPmBackup.scctData[0u] );
        CY_SET_REG8(CYREG_SC0_SW2 , cyPmBackup.scctData[1u] );
        CY_SET_REG8(CYREG_SC0_SW3 , cyPmBackup.scctData[2u] );
        CY_SET_REG8(CYREG_SC0_SW4 , cyPmBackup.scctData[3u] );
        CY_SET_REG8(CYREG_SC0_SW6 , cyPmBackup.scctData[4u] );
        CY_SET_REG8(CYREG_SC0_SW8 , cyPmBackup.scctData[5u] );
        CY_SET_REG8(CYREG_SC0_SW10, cyPmBackup.scctData[6u] );

        CY_SET_REG8(CYREG_SC1_SW0 , cyPmBackup.scctData[7u] );
        CY_SET_REG8(CYREG_SC1_SW2 , cyPmBackup.scctData[8u] );
        CY_SET_REG8(CYREG_SC1_SW3 , cyPmBackup.scctData[9u] );
        CY_SET_REG8(CYREG_SC1_SW4 , cyPmBackup.scctData[10u]);
        CY_SET_REG8(CYREG_SC1_SW6 , cyPmBackup.scctData[11u]);
        CY_SET_REG8(CYREG_SC1_SW8 , cyPmBackup.scctData[12u]);
        CY_SET_REG8(CYREG_SC1_SW10, cyPmBackup.scctData[13u]);

        CY_SET_REG8(CYREG_SC2_SW0 , cyPmBackup.scctData[14u]);
        CY_SET_REG8(CYREG_SC2_SW2 , cyPmBackup.scctData[15u]);
        CY_SET_REG8(CYREG_SC2_SW3 , cyPmBackup.scctData[16u]);
        CY_SET_REG8(CYREG_SC2_SW4 , cyPmBackup.scctData[17u]);
        CY_SET_REG8(CYREG_SC2_SW6 , cyPmBackup.scctData[18u]);
        CY_SET_REG8(CYREG_SC2_SW8 , cyPmBackup.scctData[19u]);
        CY_SET_REG8(CYREG_SC2_SW10, cyPmBackup.scctData[20u]);

        CY_SET_REG8(CYREG_SC3_SW0 , cyPmBackup.scctData[21u]);
        CY_SET_REG8(CYREG_SC3_SW2 , cyPmBackup.scctData[22u]);
        CY_SET_REG8(CYREG_SC3_SW3 , cyPmBackup.scctData[23u]);
        CY_SET_REG8(CYREG_SC3_SW4 , cyPmBackup.scctData[24u]);
        CY_SET_REG8(CYREG_SC3_SW6 , cyPmBackup.scctData[25u]);
        CY_SET_REG8(CYREG_SC3_SW8 , cyPmBackup.scctData[26u]);
        CY_SET_REG8(CYREG_SC3_SW10, cyPmBackup.scctData[27u]);

    #endif  /* (CY_PSOC3 || CY_PSOC5A || CY_PSOC5LP) */


    #if(CY_PSOC3)

        /* Serial Wire Viewer (SWV) workaround */
        if(CY_PM_ENABLED == cyPmBackup.swvClkEnabled)
        {
            /* Restore ports drive mode */
            CY_PM_PRT1_PC3_REG = (CY_PM_PRT1_PC3_REG & CY_PM_PRT1_PC3_DM_MASK) |
                                    cyPmBackup.prt1Dm;

            /* Enable SWV clocks */
            CY_PM_MLOGIC_DBG_REG |= CY_PM_MLOGIC_DBG_SWV_CLK_EN;
        }

    #endif /* (CY_PSOC3) */


    #if(CY_PSOC5A)

        /* Restore buzz sleep trim value */
        CY_PM_PWRSYS_BUZZ_TR_REG = cyPmBackup.buzzSleepTrim | \
                                  (CY_PM_PWRSYS_BUZZ_TR_REG & CY_PM_PWRSYS_BUZZ_TR_MASK);

    #endif  /* (CY_PSOC5A) */


    /* Restore boost reference */
    if(CY_PM_ENABLED == cyPmBackup.boostRefExt)
    {
        CY_PM_BOOST_CR2_REG |= CY_PM_BOOST_CR2_EREFSEL_EXT;
    }
}


/*******************************************************************************
* Function Name: CyPmHviLviSaveDisable
********************************************************************************
*
* Summary:
*  Saves analog and digital LVI and HVI configuration and disables them.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Reentrant:
*  No
*
*******************************************************************************/
static void CyPmHviLviSaveDisable(void) 
{
    if(0u != (CY_VD_LVI_HVI_CONTROL_REG & CY_VD_LVID_EN))
    {
        cyPmBackup.lvidEn = CY_PM_ENABLED;
        cyPmBackup.lvidTrip = CY_VD_LVI_TRIP_REG & CY_VD_LVI_TRIP_LVID_MASK;

        /* Save state of reset device at a specified Vddd threshold */
        #if(CY_PSOC5A)

            /* Not applicable PSoC 5 */
            cyPmBackup.lvidRst = CY_PM_DISABLED;

        #else

            cyPmBackup.lvidRst = (0u == (CY_VD_PRES_CONTROL_REG & CY_VD_PRESD_EN)) ? \
                                    CY_PM_DISABLED : CY_PM_ENABLED;

        #endif  /* (CY_PSOC5A) */

        CyVdLvDigitDisable();
    }
    else
    {
        cyPmBackup.lvidEn = CY_PM_DISABLED;
    }

    if(0u != (CY_VD_LVI_HVI_CONTROL_REG & CY_VD_LVIA_EN))
    {
        cyPmBackup.lviaEn = CY_PM_ENABLED;
        cyPmBackup.lviaTrip = CY_VD_LVI_TRIP_REG >> 4u;

        /* Save state of reset device at a specified Vdda threshold */
        #if(CY_PSOC5A)

            /* Not applicable PSoC 5 */
            cyPmBackup.lviaRst = CY_PM_DISABLED;

        #else

            cyPmBackup.lviaRst = (0u == (CY_VD_PRES_CONTROL_REG & CY_VD_PRESA_EN)) ? \
                                    CY_PM_DISABLED : CY_PM_ENABLED;

        #endif  /* (CY_PSOC5A) */

        CyVdLvAnalogDisable();
    }
    else
    {
        cyPmBackup.lviaEn = CY_PM_DISABLED;
    }

    if(0u != (CY_VD_LVI_HVI_CONTROL_REG & CY_VD_HVIA_EN))
    {
        cyPmBackup.hviaEn = CY_PM_ENABLED;
        CyVdHvAnalogDisable();
    }
    else
    {
        cyPmBackup.hviaEn = CY_PM_DISABLED;
    }
}


/*******************************************************************************
* Function Name: CyPmHviLviRestore
********************************************************************************
*
* Summary:
*  Restores analog and digital LVI and HVI configuration.
*
* Parameters:
*  None
*
* Return:
*  None
*
* Reentrant:
*  No
*
*******************************************************************************/
static void CyPmHviLviRestore(void) 
{
    /* Restore LVI/HVI configuration */
    if(CY_PM_ENABLED == cyPmBackup.lvidEn)
    {
        CyVdLvDigitEnable(cyPmBackup.lvidRst, cyPmBackup.lvidTrip);
    }

    if(CY_PM_ENABLED == cyPmBackup.lviaEn)
    {
        CyVdLvAnalogEnable(cyPmBackup.lviaRst, cyPmBackup.lviaTrip);
    }

    if(CY_PM_ENABLED == cyPmBackup.hviaEn)
    {
        CyVdHvAnalogEnable();
    }
}


/* [] END OF FILE */
