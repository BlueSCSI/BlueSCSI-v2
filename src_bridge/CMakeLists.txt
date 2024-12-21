# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ====================================================================================
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

##### Uncommend the following for development:
# set(CMAKE_BUILD_TYPE "Debug")

cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !! If cmake fails at this point, you probably don't have the
# !! FreeRTOS git submodule checked out. Run the following
# !! command to populate the ./freertos directory.
# !!       git submodule update --init --recursive
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos)
include(FreeRTOS_Kernel_import.cmake)

#-------------
# Fetch the FreeRTOS CLI library
#    Currently, the Plus-CLI code is only included in the full
#    FreeRTOS repository. In the future, we could try to just
#    download the subdirectory FreeRTOS-Plus/Source/FreeRTOS-Plus-CLI
# FetchContent_Declare(
#     freertos-plus-cli
#     URL      https://download-directory.github.io/?url=https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS-Plus/Source/FreeRTOS-Plus-CLI
#     URL_HASH MD5=14c4bff04ee2a4f55ec510967dc97c28
# )
# We only need two files.... so manually pulling those instead of the whole FreeRTOS repo
include(FetchContent)
FetchContent_Declare(
    freertos-plus-cli-source
    URL      https://raw.githubusercontent.com/FreeRTOS/FreeRTOS/refs/tags/V10.4.1/FreeRTOS-Plus/Source/FreeRTOS-Plus-CLI/FreeRTOS_CLI.c
    URL_HASH MD5=41570a37ac99b996ccbca5f5eeeea794
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(freertos-plus-cli-source)
FetchContent_Declare(
    freertos-plus-cli-header
    URL      https://raw.githubusercontent.com/FreeRTOS/FreeRTOS/refs/tags/V10.4.1/FreeRTOS-Plus/Source/FreeRTOS-Plus-CLI/FreeRTOS_CLI.h
    URL_HASH MD5=bec1e2660f8a4589a99a1ddfa92be9b1
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(freertos-plus-cli-header)

project(scsibridge C CXX ASM)

# These must be kept in this order, since libminini depends on libsdfat
include(libsdfat-config.cmake)
include(libminini-config.cmake)

pico_sdk_init()

if (${PICO_SDK_VERSION_MAJOR} LESS 2)
	message(SEND_ERROR "Version 2 of the Pico SDK is required to compile this project. Please update your installation at ${PICO_SDK_PATH}")
endif ()

add_executable(scsibridge
        src/cmd_console_task.cpp
        src/cmd_console_examples.cpp
        src/freertos_error_cbs.c
        src/main.cpp
        src/ringbuffer.cpp
        src/scsi/BlueSCSI.cpp
        src/scsi/BlueSCSI_Config.cpp
        src/scsi/BlueSCSI_log.cpp
        src/scsi/BlueSCSI_main.cpp
        src/scsi/BlueSCSI_log_trace.cpp
        src/scsi/BlueSCSI_initiator.cpp
        src/scsi/BlueSCSI_disk.cpp
        src/scsi/BlueSCSI_cdrom.cpp
        src/scsi/BlueSCSI_mode.cpp
        src/scsi/BlueSCSI_presets.cpp
        src/scsi/BlueSCSI_Toolbox.cpp
        src/scsi/ImageBackingStore.cpp
        src/scsi/ROMDrive.cpp
        src/scsi/BlueSCSI_tape.cpp
        src/usb/configuration_descriptor.cpp
        src/usb/device_descriptor.cpp
        src/usb/endpoint_descriptor.cpp
        src/usb/interface_descriptor.cpp
        src/usb/msc_base_disk.cpp
        src/usb/msc_disk_cbs.cpp
        src/usb/msc_disk_commands.cpp
        src/usb/msc_ram_disk.cpp
        src/usb/msc_scsi_disk.cpp
        src/usb/stdio_tinyusb_cdc.cpp
        src/usb/string_descriptor.cpp
        src/usb/usb_descriptor_cbs.cpp
        src/usb/usb_task.cpp
        src/SCSI2SD/src/firmware/mode.c
        src/SCSI2SD/src/firmware/vendor.c
        src/SCSI2SD/src/firmware/inquiry.c
        src/SCSI2SD/src/firmware/geometry.c
        src/SCSI2SD/src/firmware/scsi.c
        src/SCSI2SD/src/firmware/mo.c
        src/SCSI2SD/src/firmware/diagnostic.c
        src/CUEParser/src/CUEParser.cpp
        BlueSCSI_platform/scsiPhy.cpp
        BlueSCSI_platform/BlueSCSI_platform.cpp
        BlueSCSI_platform/BlueSCSI_platform_config_hook.cpp
        BlueSCSI_platform/BlueSCSI_platform_network.cpp
        BlueSCSI_platform/BlueSCSI_platform_commands.cpp
        BlueSCSI_platform/scsiHostPhy.cpp
        BlueSCSI_platform/scsi_accel_host.cpp
        BlueSCSI_platform/sd_card_sdio.cpp
        BlueSCSI_platform/sd_card_spi.cpp
        BlueSCSI_platform/rp2040_sdio.cpp
        BlueSCSI_platform/scsi_accel_rp2040.cpp
        BlueSCSI_platform/scsiPhy.cpp
        ${freertos-plus-cli-source_SOURCE_DIR}/FreeRTOS_CLI.c
)

target_include_directories(scsibridge PRIVATE
        include/
        src/scsi/
        BlueSCSI_Platform
        src/CUEParser/src/CUEParser/
        src/SCSI2SD/include/
        src/SCSI2SD/src/firmware/
        src/CUEParser/src/
        ${freertos-plus-cli-header_SOURCE_DIR}/
        ${MININI_LIB_PATH}/
        ${SDFAT_LIB_PATH}/
        )

#sdfat throws an "overloaded-virtual" warning. Inhibit it
target_compile_options(scsibridge PRIVATE 
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wno-overloaded-virtual>
    $<$<COMPILE_LANGUAGE:C>:-Wall>)

pico_generate_pio_header(scsibridge ${CMAKE_CURRENT_LIST_DIR}/BlueSCSI_platform/rp2040_sdio.pio)
pico_generate_pio_header(scsibridge ${CMAKE_CURRENT_LIST_DIR}/BlueSCSI_platform/scsi_accel_host.pio)

# We are going to use our own STDIO handler
set(PICO_STDIO_UART 0)
set(PICO_STDIO_USB 0)
set(PICO_DEFAULT_UART_RX_PIN -1)
set(PICO_DEFAULT_UART_TX_PIN -1)

target_include_directories(scsibridge PRIVATE src)

target_compile_definitions (scsibridge PUBLIC
	PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
    PLATFORM_HAS_INITIATOR_MODE
    HAS_SDIO_CLASS
    # minini configuration
    INI_ANSIONLY=1
    INI_READONLY=1
    # UART_TX_PIN=-1
    # UART_RX_PIN=-1
    SPI_DRIVER_SELECT=3
    SD_CHIP_SELECT_MODE=2
    ENABLE_DEDICATED_SPI=1
)


target_link_libraries(scsibridge PRIVATE
        pico_multicore
        pico_stdlib
        pico_unique_id
        hardware_dma
        hardware_spi
        hardware_pio
        hardware_adc
        hardware_i2c
        tinyusb_device
        tinyusb_board
        FreeRTOS-Kernel
        # TODO: This should be changed back to Heap1 at a later date
        # Heap1 is basically a simple allocate-only heap manager, but
        # is much more efficient and deterministic.
        # Currently, vTaskList() is trying to "free" memory
        FreeRTOS-Kernel-Heap4
        libminini
        libsdfat
)

pico_add_extra_outputs(scsibridge)
