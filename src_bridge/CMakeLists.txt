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
        src/freertos_hook.c
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
)
target_include_directories(scsibridge PRIVATE
        include/
        src/scsi/
        BlueSCSI_Platform
        src/CUEParser/src/CUEParser/
        src/SCSI2SD/include/
        src/SCSI2SD/src/firmware/
        ${minini_SOURCE_DIR}/dev/
        # ${sdfat_SOURCE_DIR}/src/
        )


target_compile_options(scsibridge PRIVATE -Wall)

pico_generate_pio_header(scsibridge ${CMAKE_CURRENT_LIST_DIR}/BlueSCSI_platform/rp2040_sdio.pio)
pico_generate_pio_header(scsibridge ${CMAKE_CURRENT_LIST_DIR}/BlueSCSI_platform/scsi_accel_host.pio)

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
        FreeRTOS-Kernel-Heap1
)

pico_add_extra_outputs(scsibridge)


# # Add option to enable/disable tests
# option(BUILD_TESTS "Build unit tests" OFF)

# # Only build tests if enabled and not building for Pico
# if(BUILD_TESTS)
#     add_subdirectory(test)
# endif()