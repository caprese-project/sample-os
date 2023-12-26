cmake_minimum_required(VERSION 3.12)

add_executable(plic)

target_sources(
  plic PRIVATE
  src/main.c
)

target_compile_features(plic PRIVATE c_std_17)
target_compile_options(plic PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(plic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(plic PRIVATE libc)

target_link_options(
  plic
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)
