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

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = $program_size
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
    .boot2 : {
        __boot2_start__ = .;
        KEEP (*(.boot2))
        __boot2_end__ = .;
    } > FLASH
    ASSERT(__boot2_end__ - __boot2_start__ == 256,
        "ERROR: Pico second stage bootloader must be 256 bytes in size")

    /* If BlueSCSI SD card bootloader is included, it goes in first 128 kB */
    .text.bootloader : ALIGN(16) SUBALIGN(16)
    {
        KEEP(*(.text.btldr*))
        . = ALIGN(131072);
        CHECK_BOOTLOADER_SIZE = 1 / (. <= 131072);
    } > FLASH

    .text : {
        __logical_binary_start = .;
        __real_vectors_start = .;
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        __binary_info_header_end = .;
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
        . = ALIGN(4);

        /* The rules below aim to put non-performance-critical code in flash.
         * The functions that are *not* listed below will go to .data in RAM.
         *
         * RP2040 has 16 kB of cache for the flash, so functions that are large
         * and rarely used can be put in flash, even if they are important for
         * performance when they are in use. By having most of the constantly
         * executing code in RAM already, there is less contention for cache space.
         */
/*
        .pio/build/$project_name/src/BlueSCSI_initiator.cpp.o(.text .text*)
        .pio/build/$project_name/src/BlueSCSI_msc_initiator.cpp.o(.text .text*)
        .pio/build/$project_name/src/BlueSCSI_log.cpp.o(.text .text*)
        .pio/build/$project_name/src/BlueSCSI_log_trace.cpp.o(.text .text*)
        .pio/build/$project_name/src/BlueSCSI_settings.cpp.o(.text .text*)
        .pio/build/$project_name/src/QuirksCheck.cpp.o(.text .text*)

*/  
	/* Standard library floating point etc. functions */
        *libm*:(.text .text*)
        *libc*:(.text .text*)
        *libgcc*:*df*(.text .text*)
        *USB*(.text .text*)
        *SPI*(.text .text*)
        *Spi*(.text .text*)
        *spi*(.text .text*)
        *stdc*:(.text .text*)
        *supc*:(.text .text*)
        *nosys*:(.text .text*)
        *libc*:*printf*(.text .text*)
        *libc*:*toa*(.text .text*)

        /* Libraries that are not performance-critical */
        *libminIni.a:(.text .text*)
        *libCUEParser.a:(.text .text*)

        /* C++ constructors and destructors */
        *(.text*_ZN*C1*)
        *(.text*_ZN*C2*)
        *(.text*_ZN*D1*)
        *(.text*_ZN*D2*)

        /* Initialization functions go to flash */
        *(.text*platform_init*)
        *(.text*platform_late_init*)
        *(.text*platform_enter_msc*)
        *(.text*zuluscsi_setup*)
        *(.text*audio_setup*)
        *(.text*init_logfile*)
        *(.text*find_chs_capacity*)
        *(.text*irq_remove_handler*)
        *(.text*irq_add_shared_handler*)
        *(.text*image_config_t*clear*)
        *(.text*reinitSCSI*)
        *(.text*print_sd_info*)
        *(.text*kiosk_restore_images*)
        *(.text*scsi_accel_rp2040_init*)
        *(.text*rp2040_sdio_init*)
        *(.text*PIOProgram*prepare*)
        *(.text*createImage*)
        *(.text*s2s_configInit*)
        *(.text*SdioCard*begin*)
        *(.text*mountSDCard*)
        *(.text*set_timings_from_file*)
        *(.text*scsiInit*)

        /* Logging code
         * Small parts of this will be executed in normal, non-debug mode,
         * but those can go to cache.
         */
        *ZuluSCSI_log.cpp.o(.text .text*)
        *ZuluSCSI_log_trace.cpp.o(.text .text*)

        /* ROM drive setup and FW upgrade that is done only during init */
        *(.text*romDriveClear*)
        *(.text*romDriveCheckPresent*)
        *(.text*zipparser*)
        *(.text*firmware_update*)

        /* BlueSCSI_disk functions that are only used during init */
        *BlueSCSI_settings.cpp.o(.text .text*)
        *QuirksCheck.cpp.o(.text .text*)
        *(.text*scsiDiskFilenameValid*)
        *(.text*scsiDiskOpenHDDImage*)
        *(.text*scsiDiskGetNextImageName*)
        *(.text*scsiDiskProgramRomDrive*)
        *(.text*scsiDiskLoadConfig*)
        *(.text*scsiDiskActivateRomDrive*)
        *(.text*scsiDiskSetImageConfig*)
        *(.text*ImageBackingStore*open*)
        *(.text*ImageBackingStore*ImageBackingStore*)
        *(.text*findHDDImages*)
        *(.text*scsiDiskCloseSDCardImages*)
        *(.text*extractFileName*)
        *(.text*setNameFromImage*)
        *(.text*getBlockSize*)

        /* SCSI commands that don't require high performance */
        *(.text*scsiToolboxCommand*)
        *(.text*scsi*Diagnostic*)
        *(.text*Inquiry*)
        *(.text*modeSelect*)
        *(.text*modeSense*)
        *(.text*doModeSelect*)
        *(.text*doModeSense*)
        *(.text*scsiModeCommand*)
        *(.text*doPerformEject*)

        /* CDROM/Tape/Vendor commands. Most of these are non-performance-critical, and
         * the important parts will fit in cache when in use. */
        *(.text*scsiTapeCommand*)
        *(.text*scsiCDRomCommand*)
        *(.text*scsiMOCommand*)
        *(.text*doReadCD*)
        *(.text*doReadTOC*)
        *(.text*doReadHeader*)
        *(.text*doPlayAudio*)
        *(.text*audio_play*)
        *(.text*doReadSubchannel*)
        *(.text*doGetConfiguration*)
        *(.text*cdromPerformEject*)
        *(.text*CueSheet*)
        *(.text*switchNextImage*)
        *(.text*findNextImageAfter*)
        *(.text*DiscInfo*)
        *(.text*TrackInfo*)
        *(.text*doCountFiles*)
        *(.text*doReadFullTOC*)
        *(.text*scsiVendorCommand*)
        *(.text*onListFiles*)

        /* SCSI Initiator mode code. These will fit in cache when in use. */
        *BlueSCSI_initiator.cpp.o(.text .text*)
        *BlueSCSI_msc_initiator.cpp.o(.text .text*)
        *scsi_accel_host*(.text .text*)
        *scsiHostPhy*(.text .text*)

        /* Filesystem functions that are not performance-critical.
         * Mostly used during initialization only. */
        *(.text*File*open*)
        *(.text*File*init*)
        *(.text*File*remove*)
        *(.text*File*truncate*)
        *(.text*File*sync*)
        *(.text*File*mkdir*)
        *(.text*File*bitmap*)
        *(.text*File*Name*)
        *(.text*File*SFN*)
        *(.text*File*LFN*)
        *(.text*File*addCluster*)
        *(.text*Volume*attrib*)
        *(.text*Volume*begin*)

        /* USB functions that only run in specific modes.
           These will fit in cache when they are in use (USB connected). */
        *libpico*:*(.text*msc*)
        *libpico*:*(.text*hidd*)
        *(.text*tud_msc*)

        /* RP2040 breakpoints in RAM code don't always work very well
         * because the boot routine tends to overwrite them.
         * Uncommenting this line puts all code in flash.
         * You may have to delete "firmware.elf" for the next build
         * to use this linker file's changes
         *
         * Alternatively, use "hbreak" in gdb to force hardware
         * breakpoints instead of RAM breakpoints.
         */
        /* *(.text .text*) */
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
        *(.text)
        *(.text*)
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
