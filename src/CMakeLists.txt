add_library(omnipp_lib INTERFACE
  hello.h
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

add_executable(omnipp main.cpp)

target_compile_options(omnipp PRIVATE
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

target_link_libraries(omnipp INTERFACE omnipp_lib)
target_compile_features(omnipp PRIVATE cxx_std_26)

if(ENABLE_ASAN)
  target_compile_options(omnipp PRIVATE -fsanitize=address)
  string(REPLACE "-static " "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  target_link_options(omnipp PRIVATE -fsanitize=address -static-libasan -Wl,--whole-archive -lasan -Wl,--no-whole-archive -lpthread)
endif()

install(TARGETS omnipp DESTINATION bin)
