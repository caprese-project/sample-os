cmake_minimum_required(VERSION 3.12)

add_executable(echo)

target_sources(
  echo PRIVATE
  src/main.c
)

target_compile_features(echo PRIVATE c_std_17)
target_compile_options(echo PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(echo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(echo PRIVATE libc)

target_link_options(
  echo
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(echo)
