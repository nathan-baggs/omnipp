# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(GTest REQUIRED)

add_executable(beman.cstring_view.tests.cstring_view)
target_sources(
    beman.cstring_view.tests.cstring_view
    PRIVATE cstring_view.test.cpp
)
target_link_libraries(
    beman.cstring_view.tests.cstring_view
    PRIVATE beman::cstring_view GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(beman.cstring_view.tests.cstring_view DISCOVERY_TIMEOUT 60)
