cmake_minimum_required(VERSION 3.12)

add_executable(fs)

target_sources(
  fs PRIVATE
  src/fs_table.cpp
  src/main.cpp
  src/server.cpp
)

target_compile_features(fs PRIVATE cxx_std_23)
target_compile_options(fs PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(fs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(fs PRIVATE libc libcxx)

target_link_options(
  fs
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(fs)
