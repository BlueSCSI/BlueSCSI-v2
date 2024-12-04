include(FetchContent)
# FetchContent_GetProperties(sdfat)
FetchContent_Declare(
    sdfat
    GIT_REPOSITORY https://github.com/rabbitholecomputing/SdFat.git
    GIT_TAG 2.2.3-gpt
)
FetchContent_MakeAvailable(sdfat)
FetchContent_GetProperties(sdfat)
set(SDFAT_LIB_PATH "${sdfat_SOURCE_DIR}/src")


# Collect all source files recursively
file(GLOB_RECURSE SD_FAT_LIB_SOURCES 
    "${sdfat_SOURCE_DIR}/src/*.cpp"
)

# Define the library
add_library(libsdfat ${SD_FAT_LIB_SOURCES})


# Include directories
target_include_directories(libsdfat
    PUBLIC 
        ${SDFAT_LIB_PATH}
    PRIVATE
        ${SDFAT_LIB_PATH}
)

# Compiler options
target_compile_features(libsdfat PUBLIC cxx_std_11)  # Set C++ standard
target_compile_options(libsdfat PRIVATE
    -Wall -Wno-overloaded-virtual
)
target_compile_definitions (libsdfat PUBLIC
    # Disable arduino junk in sdfat
    SDFAT_NOARDUINO=1
    SPI_DRIVER_SELECT=3
    SD_CHIP_SELECT_MODE=2
    ENABLE_DEDICATED_SPI=1
    HAS_SDIO_CLASS
)