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
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND
    env
      CC=${CMAKE_C_COMPILER}
      CFLAGS=${URING_CFLAGS}
      <SOURCE_DIR>/configure
  BUILD_COMMAND
    env
      CC=${CMAKE_C_COMPILER}
      CFLAGS=${URING_CFLAGS}
      make -C <SOURCE_DIR>/src -j liburing.a
  INSTALL_COMMAND ""
)

add_library(uring STATIC IMPORTED GLOBAL)
add_dependencies(uring liburing_ep)

set_target_properties(uring PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/third_party/liburing/src/liburing.a"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/third_party/liburing/src/include"
)

set(RAGEL_PREFIX "${CMAKE_BINARY_DIR}/third_party/ragel")
set(RAGEL_BIN "${RAGEL_PREFIX}/bin/ragel")
set(RAGEL_CFLAGS "-static -march=native -ggdb")
set(RAGEL_CXXFLAGS "-static -march=native -ggdb")
set(RAGEL_LDFLAGS "-static")

ExternalProject_Add(ragel_ep
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/ragel"
  BUILD_IN_SOURCE 1

  CONFIGURE_COMMAND
    find <SOURCE_DIR> -name config.h.in -o -name aclocal.m4 -o -name configure -o -name Makefile.in -o -name stamp-h1 | xargs touch &&

    env
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      CFLAGS=${RAGEL_CFLAGS}
      CXXFLAGS=${RAGEL_CXXFLAGS}
      LDFLAGS=${RAGEL_LDFLAGS}
      <SOURCE_DIR>/configure --prefix=${RAGEL_PREFIX}

  BUILD_COMMAND
    env
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      CFLAGS=${RAGEL_CFLAGS}
      CXXFLAGS=${RAGEL_CXXFLAGS}
      LDFLAGS=${RAGEL_LDFLAGS}
      make -j

  INSTALL_COMMAND
    env
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      make install
)

set(RAGEL_EXECUTABLE "${RAGEL_BIN}" CACHE FILEPATH "Vendored ragel executable" FORCE)
set(RAGEL "${RAGEL_BIN}" CACHE FILEPATH "Vendored ragel executable" FORCE)

set(Boost_ROOT "${CMAKE_SOURCE_DIR}/third_party/vectorscan/include" CACHE PATH "" FORCE)
set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/vectorscan/include" CACHE PATH "" FORCE)
set(BOOST_INCLUDEDIR "${CMAKE_SOURCE_DIR}/third_party/vectorscan/include" CACHE PATH "" FORCE)
add_subdirectory(vectorscan EXCLUDE_FROM_ALL)
target_compile_options(hs_exec_simde PRIVATE -march=x86-64)
