cmake_minimum_required(VERSION 3.12)

add_executable(touch)

target_sources(
  touch PRIVATE
  src/main.c
)

target_compile_features(touch PRIVATE c_std_17)
target_compile_options(touch PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(touch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(touch PRIVATE libc)

target_link_options(
  touch
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(touch)
