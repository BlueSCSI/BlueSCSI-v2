/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * 
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version. 
 * 
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

/**
 * This has been removed from BlueSCSI_platform.cpp so this code can be
 * placed in SRAM while BlueSCSI_platform.cpp.o can be placed in flash
*/

#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include <hardware/flash.h>
#include <hardware/structs/xip_ctrl.h>
#include <hardware/structs/usb.h>
#include <hardware/structs/nvic.h>
#include <hardware/structs/scb.h>
#include <hardware/sync.h>

#ifndef PIO_FRAMEWORK_ARDUINO_NO_USB
#include <SerialUSB.h>
#include <class/cdc/cdc_device.h>
#endif


/*****************************************/
/* Flash reprogramming from bootloader   */
/*****************************************/

#ifdef PLATFORM_BOOTLOADER_SIZE

extern uint32_t __real_vectors_start;
extern uint32_t __StackTop;
static volatile void *g_bootloader_exit_req;

__attribute__((section(".time_critical.platform_rewrite_flash_page")))
bool platform_rewrite_flash_page(uint32_t offset, uint8_t buffer[PLATFORM_FLASH_PAGE_SIZE])
{
    if (offset == PLATFORM_BOOTLOADER_SIZE)
    {
        if (buffer[3] != 0x20 || buffer[7] != 0x10)
        {
            logmsg("Invalid firmware file, starts with: ", bytearray(buffer, 16));
            return false;
        }
    }


#ifdef BLUESCSI_MCU_RP23XX

    if (nvic_hw->iser[0] & 1 << 14)
    {
        logmsg("Disabling USB during firmware flashing");
        nvic_hw->icer[0] = 1 << 14;
        usb_hw->main_ctrl = 0;
    }
#else
    if (nvic_hw->iser & 1 << 14)
    {
        logmsg("Disabling USB during firmware flashing");
        nvic_hw->icer = 1 << 14;
        usb_hw->main_ctrl = 0;
    }
#endif

    dbgmsg("Writing flash at offset ", offset, " data ", bytearray(buffer, 4));
    assert(offset % PLATFORM_FLASH_PAGE_SIZE == 0);
    assert(offset >= PLATFORM_BOOTLOADER_SIZE);

    // Avoid any mbed timer interrupts triggering during the flashing.
    uint32_t saved_irq = save_and_disable_interrupts();

#ifndef ZULUSCSI_MCU_RP23XX
    // For some reason any code executed after flashing crashes
    // unless we disable the XIP cache on RP2040.
    // Not sure why this happens, as flash_range_program() is flushing
    // the cache correctly.
    // The cache is now enabled from bootloader start until it starts
    // flashing, and again after reset to main firmware.
    xip_ctrl_hw->ctrl = 0;
#endif

    flash_range_erase(offset, PLATFORM_FLASH_PAGE_SIZE);
    flash_range_program(offset, buffer, PLATFORM_FLASH_PAGE_SIZE);

    uint32_t *buf32 = (uint32_t*)buffer;
    uint32_t num_words = PLATFORM_FLASH_PAGE_SIZE / 4;
    for (int i = 0; i < num_words; i++)
    {
        uint32_t expected = buf32[i];
#ifdef BLUESCSI_MCU_RP23XX
        uint32_t actual = *(volatile uint32_t*)(XIP_NOCACHE_NOALLOC_BASE + offset + i * 4);
#else
        uint32_t actual = *(volatile uint32_t*)(XIP_NOCACHE_BASE + offset + i * 4);
#endif
        if (actual != expected)
        {
            logmsg("Flash verify failed at offset ", offset + i * 4, " got ", actual, " expected ", expected);
            restore_interrupts(saved_irq);
            return false;
        }
    }

    restore_interrupts(saved_irq);

    return true;
}


void platform_boot_to_main_firmware()
{
    // To ensure that the system state is reset properly, we perform
    // a SYSRESETREQ and jump straight from the reset vector to main application.
    g_bootloader_exit_req = &g_bootloader_exit_req;
    scb_hw->aircr = 0x05FA0004;
    while(1);
}

void btldr_reset_handler()
{
    uint32_t* application_base = &__real_vectors_start;
    if (g_bootloader_exit_req == &g_bootloader_exit_req)
    {
        // Boot to main application
        application_base = (uint32_t*)(XIP_BASE + PLATFORM_BOOTLOADER_SIZE);
    }

    scb_hw->vtor = (uint32_t)application_base;
    __asm__(
        "msr msp, %0\n\t"
        "bx %1" : : "r" (application_base[0]),
                    "r" (application_base[1]) : "memory");
}

// Replace the reset handler when building the bootloader
// The rp2040_btldr.ld places real vector table at an offset.
__attribute__((section(".btldr_vectors")))
const void * btldr_vectors[2] = {&__StackTop, (void*)&btldr_reset_handler};

#endif // PLATFORM_BOOTLOADER_SIZE
