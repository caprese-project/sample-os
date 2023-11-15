cmake_minimum_required(VERSION 3.20)

FetchContent_Declare(
  caprese_libc
  GIT_REPOSITORY https://github.com/caprese-project/libc
)
FetchContent_MakeAvailable(caprese_libc)

add_library(libc STATIC)

target_sources(
  libc PRIVATE
  src/stdio.c
)

target_compile_features(libc PRIVATE c_std_17)
target_compile_options(libc PRIVATE ${CONFIG_C_OPTIONS})

target_include_directories(libc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(libc PUBLIC caprese_libc)
