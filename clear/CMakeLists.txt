cmake_minimum_required(VERSION 3.12)

add_executable(clear)

target_sources(
  clear PRIVATE
  src/main.c
)

target_compile_features(clear PRIVATE c_std_17)
target_compile_options(clear PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(clear PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(clear PRIVATE libc)

target_link_options(
  clear
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(clear)
