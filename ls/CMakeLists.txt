cmake_minimum_required(VERSION 3.12)

add_executable(ls)

target_sources(
  ls PRIVATE
  src/main.c
)

target_compile_features(ls PRIVATE c_std_17)
target_compile_options(ls PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(ls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ls PRIVATE libc)

target_link_options(
  ls
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(ls)
