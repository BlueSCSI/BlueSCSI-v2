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


/*
 *
 * Customized linker script for building bootloader
 *
 */

 MEMORY
{
    /* The bootloader is linked to begin at 0x12000100.
     * First 256 bytes are reserved for RP2040 second stage bootloader,
     * which comes as part of the main firmware.elf and is never overwritten.
     */
    FLASH(rx) : ORIGIN = 0x10000100, LENGTH = 128k-256
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 256k  /* Leave space for pico-debug */
    SCRATCH_X(rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}
ENTRY(_entry_point)
SECTIONS
{
    .flash_begin : {
        __flash_binary_start = .;
    } > FLASH

    /* Discard code that is not used in bootloader mode to
    * reduce the flash usage.
    */
    /DISCARD/ : {

        *scanf*(*)
        *strtod*(*)
        *libipv4.a:*(*)
        *liblwip*.a:*(*)
        *libBlueSCSI_audio_RP2MCU.a:*(*)
        *libAdafruit*.a:*(*)
        *libWire.a:*(*)
        *libSPI.a:*(*)
        *libBlueSCSI_platform_RP2MCU.a:BlueSCSI_platform_network.cpp.o*(*)
        *libFrameworkArduino.a:cyw43_wrappers.cpp.o*(*)

        /* gt */
        *BlueSCSI_tape.cpp.o*(*)
        *BlueSCSI_mode.cpp.o*(*)
        *BlueSCSI_blink.cpp.o*(*)
        *BlueSCSI_buffer_control.cpp.o*(*)
        *uiDiskUtils.cpp.o*(*)
        *SDNavigator.cpp.o*(*)
    }

    .text : {
        cyw43_digitalWrite = 0;
        cyw43_digitalRead = 0;
        cyw43_pinMode = 0;
        init_cyw43_wifi = bootloader_main;
        platform_poll = 0;

        __logical_binary_start = .;
        KEEP (*(.btldr_vectors))
        KEEP (*(.binary_info_header))
        __binary_info_header_end = .;
        . = ALIGN(256);
        __real_vectors_start = .;
        KEEP (*(.vectors))
        KEEP (*(.reset))
        KEEP (*(.init))
        *(.fini)
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)
        *(.eh_frame*)
        *(.text .text*)
        . = ALIGN(4);
    } > FLASH
    .rodata : {
        . = ALIGN(4);
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.flashdata*)))
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH
    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;
    . = ALIGN(4);
    __binary_info_start = .;
    .binary_info :
    {
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
    } > FLASH
    __binary_info_end = .;
    . = ALIGN(4);
    __etext = .;
   .ram_vector_table (COPY): {
        *(.ram_vector_table)
    } > RAM
    .data : {
        __data_start__ = .;
        *(vtable)

        /* Time critical code will go here to avoid external flash latency */
        *(.time_critical*)

        . = ALIGN(4);
        *(.data*)
        . = ALIGN(4);
        *(.after_data.*)
        . = ALIGN(4);
        PROVIDE_HIDDEN (__mutex_array_start = .);
        KEEP(*(SORT(.mutex_array.*)))
        KEEP(*(.mutex_array))
        PROVIDE_HIDDEN (__mutex_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(SORT(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);
        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        *(SORT(.fini_array.*))
        *(.fini_array)
        PROVIDE_HIDDEN (__fini_array_end = .);
        *(.jcr)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT> FLASH
    .uninitialized_data (COPY): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM
    .scratch_x : {
        __scratch_x_start__ = .;
        *(.scratch_x.*)
        . = ALIGN(4);
        __scratch_x_end__ = .;
    } > SCRATCH_X AT > FLASH
    __scratch_x_source__ = LOADADDR(.scratch_x);
    .scratch_y : {
        __scratch_y_start__ = .;
        *(.scratch_y.*)
        . = ALIGN(4);
        __scratch_y_end__ = .;
    } > SCRATCH_Y AT > FLASH
    __scratch_y_source__ = LOADADDR(.scratch_y);
    .bss : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM
    .heap (COPY):
    {
        __end__ = .;
        PROVIDE(end = .);
        *(.heap*)
        . = ORIGIN(RAM) + LENGTH(RAM) - 0x400;
        __HeapLimit = .;
    } > RAM
    .stack1_dummy (COPY):
    {
        *(.stack1*)
    } > SCRATCH_X
    .stack_dummy (COPY):
    {
        *(.stack*)
    } > RAM
    .flash_end : {
        __flash_binary_end = .;
    } > FLASH
    __StackTop = ORIGIN(RAM) + LENGTH(RAM);
    __StackLimit = __StackTop - 0x400;
    __StackOneTop = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X);
    __StackOneBottom = __StackOneTop - SIZEOF(.stack1_dummy);
    __StackBottom = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);
    ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed")
    ASSERT( __binary_info_header_end - __logical_binary_start <= 256, "Binary info must be in first 256 bytes of the binary")
}
