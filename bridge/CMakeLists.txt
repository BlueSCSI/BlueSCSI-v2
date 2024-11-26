cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)

project(example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# TEMPORARY - Use the I2C pins for STDIO
target_compile_definitions(TARGET PRIVATE
	PICO_DEFAULT_UART_TX_PIN=16
	PICO_DEFAULT_UART_RX_PIN=17
	)


add_library(on_either_core_common INTERFACE)
target_sources(on_either_core_common INTERFACE
        main.c)
target_include_directories(on_either_core_common INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        )
target_link_libraries(on_either_core_common INTERFACE
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap1
        pico_stdlib
        pico_multicore)

add_executable(on_core_zero)
target_link_libraries(on_core_zero on_either_core_common)
pico_add_extra_outputs(on_core_zero)
pico_enable_stdio_usb(on_core_zero 1)

add_executable(on_core_one)
target_link_libraries(on_core_one on_either_core_common)
target_compile_definitions(on_core_one PRIVATE
        mainRUN_FREE_RTOS_ON_CORE=1
        PICO_STACK_SIZE=0x1000
)

target_compile_options( on_core_one PUBLIC
        ### Gnu/Clang C Options
        $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
        $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Werror>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>
        )

pico_add_extra_outputs(on_core_one)
pico_enable_stdio_usb(on_core_one 1)