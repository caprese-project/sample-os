cmake_minimum_required(VERSION 3.12)

add_executable(apm)

target_sources(
  apm PRIVATE
  src/elf_loader.cpp
  src/main.cpp
  src/program_loader.cpp
  src/section.s
  src/server.cpp
  src/task_manager.cpp
)

target_compile_features(apm PRIVATE cxx_std_23)
target_compile_options(apm PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(apm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(apm PRIVATE libc libcxx)

target_link_options(
  apm
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(apm)
