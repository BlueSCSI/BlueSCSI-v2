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
set(CMAKE_BUILD_TYPE "Debug")

cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos)
include(FreeRTOS_Kernel_import.cmake)

# Fetch MiniINI testing framework
include(FetchContent)
FetchContent_Declare(
    minini
    GIT_REPOSITORY https://github.com/compuphase/minIni.git
    GIT_TAG v1.5
)
FetchContent_MakeAvailable(minini)
# .... doesn't seem to work without Arduino framework.... revisit this later
# FetchContent_GetProperties(sdfat)
# FetchContent_Declare(
#     sdfat
#     GIT_REPOSITORY https://github.com/akuker/SdFat.git
#     GIT_TAG a8a6cb042fb1fb3fa4e7e2aa05bb3c68163ce4d1
# )
# FetchContent_MakeAvailable(sdfat)
# FetchContent_GetProperties(sdfat)
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

pico_sdk_init()

if (${PICO_SDK_VERSION_MAJOR} LESS 2)
	message(SEND_ERROR "Version 2 of the Pico SDK is required to compile this project. Please update your installation at ${PICO_SDK_PATH}")
endif ()

add_executable(scsibridge
        src/led.c
        src/main.c
        src/usb_descriptors.c
        src/cdc_uart.c
        src/get_serial.c
        src/sw_dp_pio.c
        src/msc_disk.c
        src/command_line.cpp
        src/freertos_cli_commands.cpp
        src/freertos_hook.c
        src/stdio_tinyusb_cdc.cpp
        src/ringbuffer.cpp
        src/usb/configuration_descriptor.cpp
        src/usb/device_descriptor.cpp
        src/usb/endpoint_descriptor.cpp
        src/usb/interface_descriptor.cpp
        src/scsi/BlueSCSI_Config.cpp
        src/scsi/BlueSCSI_log.cpp
        src/scsi/BlueSCSI_log_trace.cpp
        src/scsi/BlueSCSI_initiator.cpp
        src/CUEParser/src/CUEParser.cpp
        src/scsi/BlueSCSI_usbbridge.cpp
        BlueSCSI_platform/BlueSCSI_platform.cpp
        BlueSCSI_platform/scsiHostPhy.cpp
        BlueSCSI_platform/scsi_accel_host.cpp
        ${freertos-plus-cli-source_SOURCE_DIR}/FreeRTOS_CLI.c
)

target_include_directories(scsibridge PRIVATE
        include/
        src/scsi/
        BlueSCSI_Platform
        src/CUEParser/src/CUEParser/
        src/SCSI2SD/include/
        src/SCSI2SD/src/firmware/
        ${minini_SOURCE_DIR}/dev/
        ${freertos-plus-cli-header_SOURCE_DIR}/
        # ${sdfat_SOURCE_DIR}/src/
        )


target_compile_options(scsibridge PRIVATE -Wall)

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
    INI_ANSIONLY=1
    INI_READONLY=1
    # Disable arduino junk in sdfat
    ENABLE_ARDUINO_FEATURES=0
    DISABLE_ARDUINO_USB=1
    ENABLE_ARDUINO_STRING=0
    # Select the generic spi driver in sdfat
    SPI_DRIVER_SELECT=3
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
        FreeRTOS-Kernel-Heap3
)

pico_add_extra_outputs(scsibridge)
