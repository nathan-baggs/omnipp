include(GoogleTest)

mark_as_advanced(BUILD_GMOCK BUILD_GTEST gtest_hide_internal_symbols)

add_executable(unit_tests
  mapped_file_tests.cpp
)

target_compile_features(unit_tests PRIVATE cxx_std_26)
target_link_libraries(unit_tests gmock_main omnipp_lib)
gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)

