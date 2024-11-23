cmake_minimum_required(VERSION 3.14)

# Make project data accessible to compiler
add_compile_definitions(APP_NAME="${APP_4_NAME}")
add_compile_definitions(APP_VERSION="${APP_4_VERSION_NUMBER}")
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})

# Include app source code file(s)
add_executable(${APP_4_NAME}
    ${APP_4_SRC_DIRECTORY}/main.cpp
    ${COMMON_CODE_DIRECTORY}/utils.cpp
)

# Link to built libraries
target_link_libraries(${APP_4_NAME} LINK_PUBLIC
    pico_stdlib
    FreeRTOS)

# Enable/disable STDIO via USB and UART
pico_enable_stdio_usb(${APP_4_NAME} 1)
pico_enable_stdio_uart(${APP_4_NAME} 1)

# Enable extra build products
pico_add_extra_outputs(${APP_4_NAME})
