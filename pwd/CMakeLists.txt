cmake_minimum_required(VERSION 3.12)

add_executable(pwd)

target_sources(
  pwd PRIVATE
  src/main.c
)

target_compile_features(pwd PRIVATE c_std_17)
target_compile_options(pwd PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(pwd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(pwd PRIVATE libc)

target_link_options(
  pwd
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(pwd)
