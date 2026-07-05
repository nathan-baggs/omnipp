add_library(omnipp_lib INTERFACE
  commands/cat.h
  concepts/concepts.h
  config/config.h
  pipeline/pipeline.h
  sink/stdout.h
  source/mapped_file.h
  transform/tree_sitter.h
  utils/auto_release.h
)

target_compile_options(omnipp_lib INTERFACE
  -Wall
  -Wextra
  -Werror
  -Wconversion-null
  -Woverlength-strings
  -Wpointer-arith
  -Wunused-local-typedefs
  -Wunused-result
  -Wvarargs
  -Wvla
  -Wwrite-strings
)

target_compile_features(omnipp_lib INTERFACE cxx_std_26)
target_include_directories(omnipp_lib INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(omnipp_lib INTERFACE beman::cstring_view tree-sitter tree_sitter_parsers)

add_executable(omnipp main.cpp)

target_compile_options(omnipp PRIVATE -ggdb)

target_link_libraries(omnipp PRIVATE omnipp_lib)
target_link_libraries(omnipp PRIVATE stdc++exp beman::cstring_view tree-sitter tree_sitter_parsers)
target_compile_features(omnipp PRIVATE cxx_std_26)
target_include_directories(omnipp PRIVATE ${PROJECT_SOURCE_DIR}/src)


install(TARGETS omnipp DESTINATION bin)

set(EXTRA_NAMES ocat cat)

foreach(EXTRA_NAME IN LISTS EXTRA_NAMES)
    add_custom_command(TARGET omnipp POST_BUILD
        # ${CMAKE_COMMAND} -E copy ensures cross-platform compatibility
        COMMAND ${CMAKE_COMMAND} -E copy 
            $<TARGET_FILE:omnipp> 
            $<TARGET_FILE_DIR:omnipp>/${EXTRA_NAME}${CMAKE_EXECUTABLE_SUFFIX}
        COMMENT "Copying omnipp to ${EXTRA_NAME}..."
    )
endforeach()
