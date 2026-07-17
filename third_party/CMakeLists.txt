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

include(ExternalProject)

set(RAGEL_PREFIX "${CMAKE_BINARY_DIR}/third_party/ragel")
set(RAGEL_BIN "${RAGEL_PREFIX}/bin/ragel")

ExternalProject_Add(ragel_ep
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/ragel"
  BUILD_IN_SOURCE 1

  CONFIGURE_COMMAND
    find <SOURCE_DIR> -name config.h.in -o -name aclocal.m4 -o -name configure -o -name Makefile.in -o -name stamp-h1 | xargs touch &&

    env
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      CXXFLAGS=-static
      LDFLAGS=-static
      <SOURCE_DIR>/configure --prefix=${RAGEL_PREFIX}

  BUILD_COMMAND
    env
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      CXXFLAGS=-static
      LDFLAGS=-static
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
