cmake_minimum_required(VERSION 3.20)
project(iot-network-broker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(ENABLE_SANITIZERS "Enable ASan and UBSan for debug builds" ON)

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Sanitizers enabled: AddressSanitizer, UndefinedBehaviorSanitizer")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wconversion>
)

enable_testing()

add_library(iot-core INTERFACE)
target_include_directories(iot-core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(iot-core INTERFACE project_warnings)

file(GLOB_RECURSE SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(iot-network-broker
    ${SRC_FILES}
)
target_link_libraries(iot-network-broker PRIVATE iot-core)

set_target_properties(iot-network-broker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

file(GLOB_RECURSE TEST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

add_executable(tests
    ${TEST_FILES}
)

find_package(GTest REQUIRED)

target_link_libraries(tests PRIVATE
    iot-core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)