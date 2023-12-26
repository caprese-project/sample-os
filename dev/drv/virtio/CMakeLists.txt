cmake_minimum_required(VERSION 3.12)

add_executable(virtio)

target_sources(
  virtio PRIVATE
  src/main.c
)

target_compile_features(virtio PRIVATE c_std_17)
target_compile_options(virtio PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(virtio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(virtio PRIVATE libc)

target_link_options(
  virtio
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)
