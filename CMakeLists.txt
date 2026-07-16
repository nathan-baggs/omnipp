if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are forbidden. Use -B build")
endif()

cmake_minimum_required(VERSION 3.30)

project(
  omnipp
  VERSION "2026.7.2.0"
  DESCRIPTION "modern c++ utils"
  LANGUAGES C CXX
)

list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "gcc_s_asneeded")
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES "gcc_s_asneeded")

set(CMAKE_DISABLE_SOURCE_CHANGES ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
  -ggdb
  $<$<CONFIG:Debug>:-fsanitize=undefined>
  $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-fcontract-evaluation-semantic=enforce>
  $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RelWithDebInfo>>:-fcontract-evaluation-semantic=ignore>
  $<$<CONFIG:RelWithDebInfo>:-march=native>
)
add_link_options(
  $<$<CONFIG:Debug>:-fsanitize=undefined>
  $<$<CONFIG:Debug>:-lpthread>
  $<$<NOT:$<CONFIG:Debug>>:-static>
  $<$<NOT:$<CONFIG:Debug>>:-static-libgcc>
  $<$<NOT:$<CONFIG:Debug>>:-static-libstdc++>
)

add_subdirectory(third_party)
add_subdirectory(src)

enable_testing()
include(CTest)
#add_subdirectory(tests)

set(CPACK_PACKAGE_NAME "omnipp")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_SOURCE_IGNORE_FILES "build")
include(CPack)
