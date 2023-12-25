cmake_minimum_required(VERSION 3.12)

add_executable(cons)

target_sources(
  cons PRIVATE
  src/main.c
)

target_compile_features(cons PRIVATE c_std_17)
target_compile_options(cons PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(cons PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(cons PRIVATE libc)

target_link_options(
  cons
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(cons)
