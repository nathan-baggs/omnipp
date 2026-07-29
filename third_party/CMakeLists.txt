set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

add_subdirectory(googletest EXCLUDE_FROM_ALL)
add_subdirectory(cstring_view EXCLUDE_FROM_ALL)
add_subdirectory(tree-sitter EXCLUDE_FROM_ALL)

add_library(tree_sitter_parsers STATIC
    tree-sitter-c/src/parser.c
    tree-sitter-cpp/src/parser.c
    tree-sitter-cpp/src/scanner.c
)

target_include_directories(tree_sitter_parsers PRIVATE
    tree-sitter-c/src
    tree-sitter-cpp/src
)

target_link_libraries(tree-sitter PRIVATE omnipp_global_flags)
target_link_libraries(tree_sitter_parsers PRIVATE omnipp_global_flags)

include(ExternalProject)

set(URING_CFLAGS "-O3 -march=native -ggdb -D_GNU_SOURCE -fPIC")

ExternalProject_Add(liburing_ep
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/liburing"
  CONFIGURE_COMMAND
    ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR> <BINARY_DIR>
    && ${CMAKE_COMMAND} -E env
         CC=${CMAKE_C_COMPILER}
         CFLAGS=${URING_CFLAGS}
         <BINARY_DIR>/configure
  BUILD_COMMAND
    ${CMAKE_COMMAND} -E env
      CC=${CMAKE_C_COMPILER}
      CFLAGS=${URING_CFLAGS}
      make -C <BINARY_DIR>/src -j liburing.a
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS "<BINARY_DIR>/src/liburing.a"
)

ExternalProject_Get_Property(liburing_ep BINARY_DIR)

file(MAKE_DIRECTORY "${BINARY_DIR}/src/include")

add_library(uring STATIC IMPORTED GLOBAL)
add_dependencies(uring liburing_ep)
set_target_properties(uring PROPERTIES
  IMPORTED_LOCATION "${BINARY_DIR}/src/liburing.a"
  INTERFACE_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/third_party/liburing/src/include;$<BUILD_INTERFACE:${BINARY_DIR}/src/include>"
)

set(Boost_ROOT "${CMAKE_SOURCE_DIR}/third_party/vectorscan/include" CACHE PATH "" FORCE)
set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/vectorscan/include" CACHE PATH "" FORCE)
set(BOOST_INCLUDEDIR "${CMAKE_SOURCE_DIR}/third_party/vectorscan/include" CACHE PATH "" FORCE)
set(FAT_RUNTIME OFF CACHE BOOL "Disable Vectorscan Fat Runtime" FORCE)
set(USE_CPU_NATIVE ON CACHE BOOL "Build Vectorscan tuned for this CPU" FORCE)
set(BUILD_AVX2 ON CACHE BOOL "Compile Vectorscan AVX2 engine implementations" FORCE)
add_subdirectory(vectorscan EXCLUDE_FROM_ALL)

add_subdirectory(mimalloc EXCLUDE_FROM_ALL)

add_subdirectory(abseil-cpp EXCLUDE_FROM_ALL)
set(RE2_USE_ABSEIL OFF CACHE BOOL "Build RE2 without Abseil dependency" FORCE)
set(RE2_INSTALL OFF CACHE BOOL "Disable RE2 install rules" FORCE)

add_subdirectory(re2 EXCLUDE_FROM_ALL)

