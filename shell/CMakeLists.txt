cmake_minimum_required(VERSION 3.12)

add_executable(shell)

target_sources(
  shell PRIVATE
  src/main.cpp
)

target_compile_features(shell PRIVATE cxx_std_23)
target_compile_options(shell PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(shell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(shell PRIVATE libc libcxx)

target_link_options(
  shell
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(shell)
