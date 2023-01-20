/*
BlueSCSI
Copyright (c) 2022-2023 the BlueSCSI contributors (CONTRIBUTORS.txt)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// As of 2022-11-16, the raspberrypi platformio package ships with old version
// of pico-sdk. This version lacks the flash_do_cmd() function in flash.h header.
// This file backports the function from new versions.

#pragma once

#ifndef RP2040_HAS_FLASH_DO_CMD

#include <hardware/flash.h>
#include "pico/bootrom.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"

#define BOOT2_SIZE_WORDS 64

static uint32_t boot2_copyout[BOOT2_SIZE_WORDS];
static bool boot2_copyout_valid = false;

static void __no_inline_not_in_flash_func(flash_init_boot2_copyout)(void) {
    if (boot2_copyout_valid)
        return;
    for (int i = 0; i < BOOT2_SIZE_WORDS; ++i)
        boot2_copyout[i] = ((uint32_t *)XIP_BASE)[i];
    __asm__ volatile ("" : : : "memory");
    boot2_copyout_valid = true;
}

static void __no_inline_not_in_flash_func(flash_enable_xip_via_boot2)(void) {
    ((void (*)(void))((char*)boot2_copyout+1))();
}

// Bitbanging the chip select using IO overrides, in case RAM-resident IRQs
// are still running, and the FIFO bottoms out. (the bootrom does the same)
static void __no_inline_not_in_flash_func(flash_cs_force)(bool high) {
    uint32_t field_val = high ?
        IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_HIGH :
        IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_LOW;
    hw_write_masked(&ioqspi_hw->io[1].ctrl,
        field_val << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS
    );
}

static void __no_inline_not_in_flash_func(flash_do_cmd)(const uint8_t *txbuf, uint8_t *rxbuf, size_t count) {
    void (*connect_internal_flash)(void) = (void(*)(void))rom_func_lookup(rom_table_code('I', 'F'));
    void (*flash_exit_xip)(void) = (void(*)(void))rom_func_lookup(rom_table_code('E', 'X'));
    void (*flash_flush_cache)(void) = (void(*)(void))rom_func_lookup(rom_table_code('F', 'C'));
    assert(connect_internal_flash && flash_exit_xip && flash_flush_cache);
    flash_init_boot2_copyout();
    __asm__ volatile ("" : : : "memory");
    connect_internal_flash();
    flash_exit_xip();

    flash_cs_force(0);
    size_t tx_remaining = count;
    size_t rx_remaining = count;
    // We may be interrupted -- don't want FIFO to overflow if we're distracted.
    const size_t max_in_flight = 16 - 2;
    while (tx_remaining || rx_remaining) {
        uint32_t flags = ssi_hw->sr;
        bool can_put = !!(flags & SSI_SR_TFNF_BITS);
        bool can_get = !!(flags & SSI_SR_RFNE_BITS);
        if (can_put && tx_remaining && rx_remaining - tx_remaining < max_in_flight) {
            ssi_hw->dr0 = *txbuf++;
            --tx_remaining;
        }
        if (can_get && rx_remaining) {
            *rxbuf++ = (uint8_t)ssi_hw->dr0;
            --rx_remaining;
        }
    }
    flash_cs_force(1);

    flash_flush_cache();
    flash_enable_xip_via_boot2();
}

#endif