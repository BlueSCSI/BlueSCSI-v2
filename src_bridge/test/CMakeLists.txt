# Fetch Unity testing framework
include(FetchContent)
FetchContent_Declare(
    Unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
)
FetchContent_MakeAvailable(Unity)

# Enable testing
enable_testing()

# Create test executables
set(TEST_FILES
    usb_endpoint_descriptor_test.cpp
    usb_interface_descriptor_test.cpp
    usb_configuration_descriptor_test.cpp
    usb_device_descriptor_test.cpp
)

foreach(test_file ${TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} 
        ${test_file}
        ${PROJECT_SOURCE_DIR}/src/usb/configuration_descriptor.cpp
        ${PROJECT_SOURCE_DIR}/src/usb/device_descriptor.cpp
        ${PROJECT_SOURCE_DIR}/src/usb/endpoint_descriptor.cpp
        ${PROJECT_SOURCE_DIR}/src/usb/interface_descriptor.cpp
    )
    
    target_include_directories(${test_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${unity_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}  # For unity_wrapper.h
    )
    
    target_link_libraries(${test_name} PRIVATE
        unity
    )
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Optional: Add a custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_TARGETS}
)



# # Fetch Unity testing framework
# include(FetchContent)
# FetchContent_Declare(
#     Unity
#     GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
#     GIT_TAG v2.5.2
# )
# FetchContent_MakeAvailable(Unity)

# # Enable testing
# enable_testing()

# # Create test executables
# set(TEST_FILES
#     usb_endpoint_descriptor_test.cpp
#     usb_interface_descriptor_test.cpp
#     usb_configuration_descriptor_test.cpp
#     usb_device_descriptor_test.cpp
# )

# # Initialize list of test targets
# set(TEST_TARGETS "")

# foreach(test_file ${TEST_FILES})
#     get_filename_component(test_name ${test_file} NAME_WE)
#     add_executable(${test_name} 
#         ${test_file}
#         ${PROJECT_SOURCE_DIR}/src/usb/configuration_descriptor.cpp
#         ${PROJECT_SOURCE_DIR}/src/usb/device_descriptor.cpp
#         ${PROJECT_SOURCE_DIR}/src/usb/endpoint_descriptor.cpp
#         ${PROJECT_SOURCE_DIR}/src/usb/interface_descriptor.cpp
#     )
    
#     target_include_directories(${test_name} PRIVATE
#         ${PROJECT_SOURCE_DIR}/include
#         ${unity_SOURCE_DIR}/src
#         ${CMAKE_CURRENT_SOURCE_DIR}  # For unity_wrapper.h
#     )
    
#     target_link_libraries(${test_name} PRIVATE
#         unity
#     )
    
#     add_test(NAME ${test_name} COMMAND ${test_name})
    
#     # Add target to list of test targets
#     list(APPEND TEST_TARGETS ${test_name})
# endforeach()

# # Add custom target that depends on all test targets
# add_custom_target(run_tests
#     COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
#     DEPENDS ${TEST_TARGETS}
# )