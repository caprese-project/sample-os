cmake_minimum_required(VERSION 3.12)

add_executable(ramfs)

target_sources(
  ramfs PRIVATE
  src/cpio.cpp
  src/dir_stream.cpp
  src/directory.cpp
  src/file_stream.cpp
  src/file.cpp
  src/fs.cpp
  src/main.cpp
  src/server.cpp
)

target_compile_features(ramfs PRIVATE cxx_std_23)
target_compile_options(ramfs PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(ramfs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ramfs PRIVATE libc libcxx)

target_link_options(
  ramfs
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(ramfs)
