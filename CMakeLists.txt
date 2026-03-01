# Copyright (C) 2026 Eric Helgeson
#
# This file is part of BlueSCSI
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.13)

# Initialize Pico SDK from environment variable
if(NOT DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    else()
        message(FATAL_ERROR "PICO_SDK_PATH not defined. Set it in environment or pass -DPICO_SDK_PATH=...")
    endif()
endif()

message(STATUS "Using PICO_SDK_PATH: ${PICO_SDK_PATH}")

# Custom board headers for Ultra/Ultra_Wide (RP2350B with 48 GPIOs)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/include)

# Pull in Pico SDK (must be before project())
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(BlueSCSI C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# =============================================================================
# Board/Target Selection
# =============================================================================
# Use BLUESCSI_TARGET to select which firmware variant to build:
#   -DBLUESCSI_TARGET=Pico              - RP2040 Pico
#   -DBLUESCSI_TARGET=Pico_DaynaPORT    - RP2040 Pico W with network
#   -DBLUESCSI_TARGET=Pico_Audio_SPDIF  - RP2040 Pico W with network + SPDIF audio
#   -DBLUESCSI_TARGET=Pico_2            - RP2350 Pico 2
#   -DBLUESCSI_TARGET=Pico_2_DaynaPORT  - RP2350 Pico 2 W with network
#   -DBLUESCSI_TARGET=Pico_2_Audio_SPDIF - RP2350 Pico 2 W with network + SPDIF audio
#   -DBLUESCSI_TARGET=Ultra             - RP2350B Ultra with I2S audio + network
#   -DBLUESCSI_TARGET=Ultra_Wide        - RP2350B Ultra Wide SCSI (I2S audio)
#
# PICO_BOARD must be set at configure time to match the target hardware.
#
# Example build commands:
#   cmake .. -DPICO_BOARD=pico -DBLUESCSI_TARGET=Pico
#   cmake .. -DPICO_BOARD=pico_w -DBLUESCSI_TARGET=Pico_DaynaPORT
#   cmake .. -DPICO_BOARD=pico_w -DBLUESCSI_TARGET=Pico_Audio_SPDIF
#   cmake .. -DPICO_BOARD=pico2 -DBLUESCSI_TARGET=Pico_2
#   cmake .. -DPICO_BOARD=pico2_w -DBLUESCSI_TARGET=Pico_2_DaynaPORT
#   cmake .. -DPICO_BOARD=pico2_w -DBLUESCSI_TARGET=Pico_2_Audio_SPDIF
#   cmake .. -DPICO_BOARD=bluescsi_ultra -DBLUESCSI_TARGET=Ultra
#   cmake .. -DPICO_BOARD=bluescsi_ultra_wide -DBLUESCSI_TARGET=Ultra_Wide
# =============================================================================

set(BLUESCSI_TARGET "Pico" CACHE STRING "BlueSCSI target variant to build")
set_property(CACHE BLUESCSI_TARGET PROPERTY STRINGS
    Pico Pico_DaynaPORT Pico_Audio_SPDIF
    Pico_2 Pico_2_DaynaPORT Pico_2_Audio_SPDIF
    Ultra Ultra_Wide
)

message(STATUS "Building BlueSCSI target: ${BLUESCSI_TARGET}")

# =============================================================================
# Source file lists
# =============================================================================

# Common application sources
set(COMMON_SOURCES
    src/BlueSCSI_main.cpp
    src/BlueSCSI.cpp
    src/BlueSCSI_settings.cpp
    src/BlueSCSI_disk.cpp
    src/BlueSCSI_cdrom.cpp
    src/BlueSCSI_tape.cpp
    src/BlueSCSI_log.cpp
    src/BlueSCSI_log_trace.cpp
    src/BlueSCSI_blink.cpp
    src/BlueSCSI_mode.cpp
    src/BlueSCSI_initiator.cpp
    src/BlueSCSI_msc.cpp
    src/BlueSCSI_msc_initiator.cpp
    src/BlueSCSI_Toolbox.cpp
    src/ImageBackingStore.cpp
    src/ROMDrive.cpp
    src/QuirksCheck.cpp
    src/usb_descriptors.c
)

# Platform sources
set(PLATFORM_SOURCES
    lib/BlueSCSI_platform_RP2MCU/BlueSCSI_platform.cpp
    lib/BlueSCSI_platform_RP2MCU/BlueSCSI_platform_msc.cpp
    lib/BlueSCSI_platform_RP2MCU/scsiPhy.cpp
    lib/BlueSCSI_platform_RP2MCU/scsi_accel_target.cpp
    lib/BlueSCSI_platform_RP2MCU/scsi_accel_target_wide.cpp
    lib/BlueSCSI_platform_RP2MCU/scsi_accel_host.cpp
    lib/BlueSCSI_platform_RP2MCU/scsiHostPhy.cpp
    lib/BlueSCSI_platform_RP2MCU/sdio.cpp
    lib/BlueSCSI_platform_RP2MCU/sd_card_sdio.cpp
    lib/BlueSCSI_platform_RP2MCU/sd_card_spi.cpp
    lib/BlueSCSI_platform_RP2MCU/program_flash.cpp
    lib/BlueSCSI_platform_RP2MCU/custom_timings.cpp
    lib/BlueSCSI_platform_RP2MCU/scsi2sd_timings.c
    lib/BlueSCSI_platform_RP2MCU/timings_RP2MCU.c
)

# Network sources (for DaynaPORT variants)
set(NETWORK_SOURCES
    lib/BlueSCSI_platform_RP2MCU/BlueSCSI_platform_network.cpp
    src/lwip_stubs.c
)

# Audio sources
set(AUDIO_I2S_SOURCES
    lib/BlueSCSI_platform_RP2MCU/audio_i2s.cpp
    lib/BlueI2S/BlueI2S.cpp
)
set(AUDIO_SPDIF_SOURCES
    lib/BlueSCSI_platform_RP2MCU/audio_spdif.cpp
)

# SCSI2SD library sources
set(SCSI2SD_SOURCES
    lib/SCSI2SD/src/firmware/scsi.c
    lib/SCSI2SD/src/firmware/inquiry.c
    lib/SCSI2SD/src/firmware/mode.c
    lib/SCSI2SD/src/firmware/diagnostic.c
    lib/SCSI2SD/src/firmware/network.c
    lib/SCSI2SD/src/firmware/vendor.c
    lib/SCSI2SD/src/firmware/geometry.c
    lib/SCSI2SD/src/firmware/mo.c
    lib/SCSI2SD/src/firmware/AmigaWIFI/AmigaWIFI.c
)

# External dependencies fetched from GitHub
include(FetchContent)

# SdFat library (we only use its sources, not its CMake targets)
FetchContent_Declare(
    SdFat
    GIT_REPOSITORY https://github.com/BlueSCSI/SdFat.git
    GIT_TAG bluescsi-fastseek
    SOURCE_SUBDIR _none
)
FetchContent_MakeAvailable(SdFat)
file(GLOB_RECURSE SDFAT_SOURCES ${sdfat_SOURCE_DIR}/src/*.cpp)

# minIni library
set(MININI_SOURCES
    lib/minIni/minIni.cpp
    lib/minIni/minIni_cache.cpp
)

# CUEParser library
FetchContent_Declare(
    CUEParser
    GIT_REPOSITORY https://github.com/BlueSCSI/CUEParser.git
    GIT_TAG 8c765596f1f8771c1b66d3063d13e33b0efab896
)
FetchContent_MakeAvailable(CUEParser)
set(CUEPARSER_SOURCES
    ${cueparser_SOURCE_DIR}/src/CUEParser.cpp
)

# ZipParser library
set(ZIPPARSER_SOURCES
    lib/ZipParser/zip_parser.cpp
)

# Include directories
set(INCLUDE_DIRS
    src
    lib/BlueSCSI_platform_RP2MCU
    lib/SCSI2SD/src
    lib/SCSI2SD/src/firmware
    lib/SCSI2SD/include
    ${sdfat_SOURCE_DIR}/src
    lib/minIni
    lib/ZipParser
    lib/BlueI2S
)

# Common compile definitions (from platformio.ini)
set(COMMON_DEFINES
    PICO_FLASH_SPI_CLKDIV=2
    SPI_DRIVER_SELECT=3
    SD_CHIP_SELECT_MODE=2
    ENABLE_DEDICATED_SPI=1
    HAS_SDIO_CLASS
    ENABLE_ARDUINO_FEATURES=0
    ENABLE_ARDUINO_SERIAL=0
    ENABLE_ARDUINO_STRING=0
    PLATFORM_MASS_STORAGE
    FILE_COPY_CONSTRUCTOR_SELECT=FILE_COPY_CONSTRUCTOR_PUBLIC
    DISABLE_FS_H_WARNING
    RECLOCKING_SUPPORTED
    PLATFORM_HAS_INITIATOR_MODE
    DISABLE_SWO
    PICO_CYW43_ARCH_POLL=1
    CYW43_PIN_WL_DYNAMIC=1
    CYW43_LWIP=0
    CYW43_USE_OTP_MAC=0
)

# Common compiler flags (optimization level set per-target below)
set(COMMON_FLAGS
    -ggdb -g3
    -Wall -Wno-sign-compare -Wno-ignored-qualifiers
    -Wno-stringop-truncation -Wno-unused-function -Wno-attributes
)
# C++-only warning flags (avoid "not valid for C" warnings)
set(CXX_ONLY_FLAGS
    -Wno-overloaded-virtual
)

# Use -Os to match PlatformIO precompiled SDK library optimization.
list(APPEND COMMON_FLAGS -Os)

# =============================================================================
# Main firmware executable
# =============================================================================

add_executable(BlueSCSI
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
    ${SCSI2SD_SOURCES}
    ${SDFAT_SOURCES}
    ${MININI_SOURCES}
    ${CUEPARSER_SOURCES}
    ${ZIPPARSER_SOURCES}
)

target_include_directories(BlueSCSI PRIVATE
    ${INCLUDE_DIRS}
    ${cueparser_SOURCE_DIR}/src
)
target_compile_definitions(BlueSCSI PRIVATE ${COMMON_DEFINES})
target_compile_options(BlueSCSI PRIVATE ${COMMON_FLAGS} $<$<COMPILE_LANGUAGE:CXX>:${CXX_ONLY_FLAGS}>)

# Suppress warnings for third-party code
set_source_files_properties(${SDFAT_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${MININI_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${CUEPARSER_SOURCES} PROPERTIES COMPILE_FLAGS "-w")

# Link Pico SDK libraries
# Note: We don't use pico_stdio_usb — we provide our own USB descriptors for CDC+MSC
# tinyusb_board provides board_init() for USB hardware initialization
target_link_libraries(BlueSCSI PRIVATE
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_timer
    hardware_watchdog
    hardware_flash
    hardware_adc
    hardware_clocks
    tinyusb_device
    tinyusb_board
)

# Disable SDK's stdio_usb — we provide our own USB descriptors for CDC+MSC composite device
pico_enable_stdio_usb(BlueSCSI 0)
pico_enable_stdio_uart(BlueSCSI 0)

# Use compiler/newlib printf instead of Pico SDK's pico_printf.
# Saves ~4.5KB RAM on RP2040 by avoiding float-capable printf in RAM.
# PlatformIO builds also use newlib printf.
pico_set_printf_implementation(BlueSCSI compiler)

# Generate PIO headers from .pio files
pico_generate_pio_header(BlueSCSI ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/scsi_accel_target_RP2MCU.pio)
pico_generate_pio_header(BlueSCSI ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/scsi_accel_target_RP2350_wide.pio)
pico_generate_pio_header(BlueSCSI ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/scsi_accel_host_RP2MCU.pio)
pico_generate_pio_header(BlueSCSI ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/sdio_RP2MCU.pio)
pico_generate_pio_header(BlueSCSI ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/sdio_RP2_Ultra.pio)
pico_generate_pio_header(BlueSCSI ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueI2S/blue_pio_i2s.pio)

# Generate UF2 output
pico_add_extra_outputs(BlueSCSI)

# BUILD_ENV define — used by FIRMWARE_NAME_PREFIX in BlueSCSI_config.h
target_compile_definitions(BlueSCSI PRIVATE BUILD_ENV=BlueSCSI_${BLUESCSI_TARGET})

# =============================================================================
# Target-specific configuration
# =============================================================================

if(BLUESCSI_TARGET STREQUAL "Pico")
    # BlueSCSI Pico (RP2040)
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_V2
        BLUESCSI_MCU_RP20XX
    )
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Pico")

elseif(BLUESCSI_TARGET STREQUAL "Pico_DaynaPORT")
    # BlueSCSI Pico with DaynaPORT (Pico W)
    target_sources(BlueSCSI PRIVATE ${NETWORK_SOURCES})
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_V2
        BLUESCSI_MCU_RP20XX
        BLUESCSI_NETWORK
        BLUESCSI_DAYNAPORT
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
    )
    target_link_libraries(BlueSCSI PRIVATE pico_cyw43_arch_poll pico_async_context_poll)
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Pico_DaynaPORT")

elseif(BLUESCSI_TARGET STREQUAL "Pico_Audio_SPDIF")
    # BlueSCSI Pico with DaynaPORT + SPDIF audio (Pico W)
    target_sources(BlueSCSI PRIVATE ${NETWORK_SOURCES} ${AUDIO_SPDIF_SOURCES})
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_V2
        BLUESCSI_MCU_RP20XX
        BLUESCSI_NETWORK
        BLUESCSI_DAYNAPORT
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
        ENABLE_AUDIO_OUTPUT
        ENABLE_AUDIO_OUTPUT_SPDIF
        NETWORK_PACKET_QUEUE_SIZE=12
    )
    target_link_libraries(BlueSCSI PRIVATE pico_cyw43_arch_poll pico_async_context_poll)
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Pico_Audio_SPDIF")

elseif(BLUESCSI_TARGET STREQUAL "Pico_2")
    # BlueSCSI Pico 2 (RP2350)
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_V2
        BLUESCSI_MCU_RP23XX
    )
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Pico_2")

elseif(BLUESCSI_TARGET STREQUAL "Pico_2_DaynaPORT")
    # BlueSCSI Pico 2 with DaynaPORT (Pico 2 W)
    target_sources(BlueSCSI PRIVATE ${NETWORK_SOURCES})
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_V2
        BLUESCSI_MCU_RP23XX
        BLUESCSI_PICO_2_DAYNAPORT
        BLUESCSI_NETWORK
        BLUESCSI_DAYNAPORT
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
    )
    target_link_libraries(BlueSCSI PRIVATE pico_cyw43_arch_poll pico_async_context_poll)
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Pico_2_DaynaPORT")

elseif(BLUESCSI_TARGET STREQUAL "Pico_2_Audio_SPDIF")
    # BlueSCSI Pico 2 with DaynaPORT + SPDIF audio (Pico 2 W)
    target_sources(BlueSCSI PRIVATE ${NETWORK_SOURCES} ${AUDIO_SPDIF_SOURCES})
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_V2
        BLUESCSI_MCU_RP23XX
        BLUESCSI_PICO_2_DAYNAPORT
        BLUESCSI_NETWORK
        BLUESCSI_DAYNAPORT
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
        ENABLE_AUDIO_OUTPUT
        ENABLE_AUDIO_OUTPUT_SPDIF
    )
    target_link_libraries(BlueSCSI PRIVATE pico_cyw43_arch_poll pico_async_context_poll)
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Pico_2_Audio_SPDIF")

elseif(BLUESCSI_TARGET STREQUAL "Ultra")
    # BlueSCSI Ultra (RP2350B with I2S audio + network)
    target_sources(BlueSCSI PRIVATE ${NETWORK_SOURCES} ${AUDIO_I2S_SOURCES})
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_ULTRA
        BLUESCSI_MCU_RP23XX
        ENABLE_AUDIO_OUTPUT
        ENABLE_AUDIO_OUTPUT_I2S
        BLUESCSI_NETWORK
        BLUESCSI_DAYNAPORT
        BLUESCSI_RM2
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
        LOGBUFSIZE=65536
    )
    target_link_libraries(BlueSCSI PRIVATE pico_cyw43_arch_poll pico_async_context_poll)
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Ultra")

elseif(BLUESCSI_TARGET STREQUAL "Ultra_Wide")
    # BlueSCSI Ultra Wide (RP2350B with wide SCSI + I2S audio)
    target_sources(BlueSCSI PRIVATE ${AUDIO_I2S_SOURCES})
    target_compile_definitions(BlueSCSI PRIVATE
        BLUESCSI_ULTRA_WIDE
        BLUESCSI_MCU_RP23XX
        ENABLE_AUDIO_OUTPUT
        ENABLE_AUDIO_OUTPUT_I2S
        RP2MCU_USE_CPU_PARITY
        RP2MCU_SCSI_ACCEL_WIDE
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
        LOGBUFSIZE=32768
    )
    set_target_properties(BlueSCSI PROPERTIES OUTPUT_NAME "BlueSCSI_Ultra_Wide")

else()
    message(FATAL_ERROR "Unknown BLUESCSI_TARGET: ${BLUESCSI_TARGET}")
endif()

# =============================================================================
# Linker Script Configuration
# =============================================================================

# Determine MCU type and set appropriate linker scripts and flash allocation
if(BLUESCSI_TARGET MATCHES "^Pico$|^Pico_DaynaPORT$|^Pico_Audio_SPDIF$")
    set(LINKER_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/rp2040-template.ld")
    set(BOOTLOADER_LINKER "${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/rp2040_btldr.ld")
    set(PROGRAM_FLASH_ALLOCATION 1273856)
    set(MCU_TYPE "RP2040")
elseif(BLUESCSI_TARGET MATCHES "^Pico_2|^Ultra")
    set(LINKER_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/rp23xx-template.ld")
    set(BOOTLOADER_LINKER "${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/rp23xx_btldr.ld")
    if(BLUESCSI_TARGET MATCHES "^Ultra")
        set(PROGRAM_FLASH_ALLOCATION 4194304)
    else()
        set(PROGRAM_FLASH_ALLOCATION 1273856)
    endif()
    set(MCU_TYPE "RP2350")
else()
    message(FATAL_ERROR "Unknown target for linker script: ${BLUESCSI_TARGET}")
endif()

# Add ROMDRIVE_OFFSET define
target_compile_definitions(BlueSCSI PRIVATE
    ROMDRIVE_OFFSET=${PROGRAM_FLASH_ALLOCATION}
)

# Generate processed linker script (substitute $program_size and $bootloader_size)
get_target_property(OUTPUT_NAME BlueSCSI OUTPUT_NAME)
set(GENERATED_LINKER "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}_linker.ld")
set(BOOTLOADER_FLASH_ALLOCATION 131072)

file(READ ${LINKER_TEMPLATE} LINKER_CONTENT)
string(REPLACE "\$program_size" "${PROGRAM_FLASH_ALLOCATION}" LINKER_CONTENT "${LINKER_CONTENT}")
string(REPLACE "\${program_size}" "${PROGRAM_FLASH_ALLOCATION}" LINKER_CONTENT "${LINKER_CONTENT}")
string(REPLACE "\$bootloader_size" "${BOOTLOADER_FLASH_ALLOCATION}" LINKER_CONTENT "${LINKER_CONTENT}")
string(REPLACE "\${bootloader_size}" "${BOOTLOADER_FLASH_ALLOCATION}" LINKER_CONTENT "${LINKER_CONTENT}")
file(WRITE ${GENERATED_LINKER} "${LINKER_CONTENT}")

message(STATUS "Generated linker script: ${GENERATED_LINKER}")
message(STATUS "  Program flash allocation: ${PROGRAM_FLASH_ALLOCATION} bytes")

# Use custom linker script instead of Pico SDK default
pico_set_linker_script(BlueSCSI ${GENERATED_LINKER})
target_link_options(BlueSCSI PRIVATE
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.map
)

# RP2040 RAM is too tight for CRT0 spacers (they add ~1KB to .stack/.heap sections).
# The linker scripts use KEEP() to satisfy SDK >2.1.1 checks, but we disable the
# actual spacer allocation on RP2040 to stay within the 264KB RAM budget.
if(PICO_PLATFORM STREQUAL "rp2040")
    target_compile_definitions(BlueSCSI PRIVATE PICO_CRT0_ALLOCATE_SPACERS=0)
endif()

# =============================================================================
# Bootloader Build
# =============================================================================

# Bootloader sources — subset of main firmware
set(BOOTLOADER_SOURCES
    src/BlueSCSI_main.cpp
    src/BlueSCSI.cpp
    src/BlueSCSI_bootloader.cpp
    src/BlueSCSI_settings.cpp
    src/BlueSCSI_log.cpp
    src/BlueSCSI_log_trace.cpp
    src/BlueSCSI_initiator.cpp
    src/BlueSCSI_msc.cpp
    src/BlueSCSI_msc_initiator.cpp
    src/ImageBackingStore.cpp
    src/ROMDrive.cpp
    ${PLATFORM_SOURCES}
    ${SCSI2SD_SOURCES}
    ${SDFAT_SOURCES}
    ${MININI_SOURCES}
    ${ZIPPARSER_SOURCES}
)

# Create bootloader executable
add_executable(BlueSCSI_bootloader ${BOOTLOADER_SOURCES})

target_include_directories(BlueSCSI_bootloader PRIVATE
    ${INCLUDE_DIRS}
    ${cueparser_SOURCE_DIR}/src
)

# Get main target's compile definitions, filter out network-related ones for bootloader
get_target_property(MAIN_DEFINES BlueSCSI COMPILE_DEFINITIONS)
set(BOOTLOADER_DEFINES ${MAIN_DEFINES})
list(FILTER BOOTLOADER_DEFINES EXCLUDE REGEX "BLUESCSI_NETWORK|BLUESCSI_DAYNAPORT|BLUESCSI_RM2|CYW43_PIO_CLOCK_DIV_DYNAMIC")
target_compile_definitions(BlueSCSI_bootloader PRIVATE
    ${BOOTLOADER_DEFINES}
    BLUESCSI_BOOTLOADER_MAIN
    PICO_CRT0_ALLOCATE_SPACERS=0
)
target_compile_options(BlueSCSI_bootloader PRIVATE ${COMMON_FLAGS} $<$<COMPILE_LANGUAGE:CXX>:${CXX_ONLY_FLAGS}>)

# Link bootloader with required libraries (no tinyusb_board — bootloader doesn't init USB)
target_link_libraries(BlueSCSI_bootloader PRIVATE
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_timer
    hardware_watchdog
    hardware_flash
    hardware_adc
    hardware_clocks
    tinyusb_device
)

# Generate PIO headers for bootloader
pico_generate_pio_header(BlueSCSI_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/scsi_accel_target_RP2MCU.pio)
pico_generate_pio_header(BlueSCSI_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/scsi_accel_target_RP2350_wide.pio)
pico_generate_pio_header(BlueSCSI_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/scsi_accel_host_RP2MCU.pio)
pico_generate_pio_header(BlueSCSI_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/sdio_RP2MCU.pio)
pico_generate_pio_header(BlueSCSI_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueSCSI_platform_RP2MCU/sdio_RP2_Ultra.pio)
pico_generate_pio_header(BlueSCSI_bootloader ${CMAKE_CURRENT_SOURCE_DIR}/lib/BlueI2S/blue_pio_i2s.pio)

# Use bootloader linker script
pico_set_linker_script(BlueSCSI_bootloader ${BOOTLOADER_LINKER})
target_link_options(BlueSCSI_bootloader PRIVATE
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/bootloader.map
    -Wl,--gc-sections
)

# Disable USB stdio for bootloader
pico_enable_stdio_usb(BlueSCSI_bootloader 0)
pico_enable_stdio_uart(BlueSCSI_bootloader 0)

# Convert bootloader ELF to binary
set(BOOTLOADER_BIN "${CMAKE_CURRENT_BINARY_DIR}/bootloader.bin")
add_custom_command(
    OUTPUT ${BOOTLOADER_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:BlueSCSI_bootloader> ${BOOTLOADER_BIN}
    DEPENDS BlueSCSI_bootloader
    COMMENT "Creating bootloader binary"
)

# Create assembly file to embed bootloader binary
set(BOOTLOADER_ASM "${CMAKE_CURRENT_BINARY_DIR}/bootloader_embed.S")
file(WRITE ${BOOTLOADER_ASM}.in
".section .text.btldr, \"ax\"
.global _btldr_start
_btldr_start:
.incbin \"${BOOTLOADER_BIN}\"
.global _btldr_end
_btldr_end:
")
add_custom_command(
    OUTPUT ${BOOTLOADER_ASM}
    COMMAND ${CMAKE_COMMAND} -E copy ${BOOTLOADER_ASM}.in ${BOOTLOADER_ASM}
    DEPENDS ${BOOTLOADER_BIN}
    COMMENT "Creating bootloader embed assembly"
)

# Create a library from the embedded bootloader
add_library(bootloader_embed OBJECT ${BOOTLOADER_ASM})
set_source_files_properties(${BOOTLOADER_ASM} PROPERTIES LANGUAGE ASM)

# Link embedded bootloader into main firmware
target_link_libraries(BlueSCSI PRIVATE bootloader_embed)
add_dependencies(BlueSCSI bootloader_embed)

# =============================================================================
# Build info
# =============================================================================

find_program(ARM_SIZE arm-none-eabi-size)

if(ARM_SIZE)
    add_custom_command(TARGET BlueSCSI_bootloader POST_BUILD
        COMMAND ${ARM_SIZE} $<TARGET_FILE:BlueSCSI_bootloader>
        COMMAND ${CMAKE_COMMAND} -E echo "Bootloader binary: ${BOOTLOADER_BIN}"
    )
    add_custom_command(TARGET BlueSCSI POST_BUILD
        COMMAND ${ARM_SIZE} $<TARGET_FILE:BlueSCSI>
        COMMAND ${CMAKE_COMMAND} -E echo "Firmware: ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.uf2"
    )
else()
    add_custom_command(TARGET BlueSCSI_bootloader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Bootloader binary: ${BOOTLOADER_BIN}"
    )
    add_custom_command(TARGET BlueSCSI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Firmware: ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.uf2"
    )
endif()

message(STATUS "BlueSCSI CMake build configured")
message(STATUS "  PICO_SDK_PATH: ${PICO_SDK_PATH}")
message(STATUS "  PICO_BOARD: ${PICO_BOARD}")
message(STATUS "  BLUESCSI_TARGET: ${BLUESCSI_TARGET}")
message(STATUS "  MCU_TYPE: ${MCU_TYPE}")
message(STATUS "  Bootloader linker: ${BOOTLOADER_LINKER}")
