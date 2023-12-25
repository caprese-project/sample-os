cmake_minimum_required(VERSION 3.12)

add_executable(mm)
target_sources(
  mm PRIVATE
  src/main.cpp
  src/memory_manager.cpp
  src/server.cpp
  src/task_table.cpp
)

target_compile_features(mm PRIVATE cxx_std_23)
target_compile_options(mm PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(mm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(mm PRIVATE libc libcxx)

target_link_options(
  mm
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_ramfs(mm)
