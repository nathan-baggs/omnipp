# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.30...4.3)

project(
    beman.cstring_view
    DESCRIPTION "A null-terminated string view."
    LANGUAGES CXX
    VERSION 0.1.0
)

# [CMAKE.SKIP_TESTS]
option(
    BEMAN_CSTRING_VIEW_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# [CMAKE.CPP17_MODE]
option(
    BEMAN_CSTRING_VIEW_CPP17_MODE
    "Use the C++17 compatible implementation. Default: OFF. Values: { ON, OFF }."
    OFF
)

# [CMAKE.SKIP_EXAMPLES]
option(
    BEMAN_CSTRING_VIEW_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# for find of beman_install_library and configure_build_telemetry
include(infra/cmake/beman-install-library.cmake)
include(infra/cmake/BuildTelemetryConfig.cmake)

add_library(beman.cstring_view INTERFACE)
add_library(beman::cstring_view ALIAS beman.cstring_view)

target_sources(
    beman.cstring_view
    PUBLIC FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set_target_properties(
    beman.cstring_view
    PROPERTIES VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL}
)

add_subdirectory(include/beman/cstring_view)

beman_install_library(beman.cstring_view TARGETS beman.cstring_view)
configure_build_telemetry()

if(BEMAN_CSTRING_VIEW_CPP17_MODE)
    target_compile_options(beman.cstring_view INTERFACE -DUSE_CPP17_VARIANT)
endif()

if(BEMAN_CSTRING_VIEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/beman/cstring_view)
endif()

if(BEMAN_CSTRING_VIEW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
