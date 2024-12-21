# BlueSCSI has a custom version of MiniINI, so we need to use that instead of
# the mainline version.

# # Fetch MiniINI testing framework
# include(FetchContent)
# FetchContent_Declare(
#     minini
#     GIT_REPOSITORY https://github.com/compuphase/minIni.git
#     GIT_TAG v1.5
# )
# FetchContent_MakeAvailable(minini)

# # Collect all source files recursively
# file(GLOB_RECURSE SD_FAT_LIB_SOURCES 
#     "${libminini_SOURCE_DIR}/src/*.cpp"
# )
# find_package(libsdfat REQUIRED) 

set(MININI_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/minIni")


# Define the library
add_library(libminini 
    ${MININI_LIB_PATH}/minIni.cpp
    ${MININI_LIB_PATH}/minIni_cache.cpp
)


# Include directories
target_include_directories(libminini
    PUBLIC 
        ${MININI_LIB_PATH}
    PRIVATE
        ${MININI_LIB_PATH}
        ${SDFAT_LIB_PATH}
)

# Compiler options
# target_compile_features(libminini PUBLIC cxx_std_11)  # Set C++ standard
target_compile_options(libminini PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wno-overloaded-virtual>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
)
target_compile_definitions (libminini PUBLIC
    INI_ANSIONLY=1
    INI_READONLY=1
    # Disable arduino junk in sdfat
    SDFAT_NOARDUINO=1
    SPI_DRIVER_SELECT=3
    SD_CHIP_SELECT_MODE=2
    ENABLE_DEDICATED_SPI=1
    HAS_SDIO_CLASS
)