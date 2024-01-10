cmake_minimum_required(VERSION 3.12)

add_executable(printenv)

target_sources(
  printenv PRIVATE
  src/main.c
)

target_compile_features(printenv PRIVATE c_std_17)
target_compile_options(printenv PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(printenv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(printenv PRIVATE libc)

target_link_options(
  printenv
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(printenv)
