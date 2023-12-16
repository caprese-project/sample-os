cmake_minimum_required(VERSION 3.12)

add_executable(ns16550a)

target_sources(
  ns16550a PRIVATE
  src/main.c
)

target_compile_features(ns16550a PRIVATE c_std_17)
target_compile_options(ns16550a PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(ns16550a PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ns16550a PRIVATE libc)

target_link_options(
  ns16550a
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)
