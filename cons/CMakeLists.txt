cmake_minimum_required(VERSION 3.12)

add_executable(cons)

target_sources(
  cons PRIVATE
  src/console.cpp
  src/file.cpp
  src/main.cpp
  src/server.cpp
  src/uart_file.cpp
)

target_compile_features(cons PRIVATE cxx_std_23)
target_compile_options(cons PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(cons PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${DEV_INTERFACE_DIR})

target_link_libraries(cons PRIVATE libc libcxx)

target_link_options(
  cons
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(cons)
