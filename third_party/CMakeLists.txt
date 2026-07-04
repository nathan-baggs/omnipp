set(BUILD_SHARED_LIBS OFF)

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

