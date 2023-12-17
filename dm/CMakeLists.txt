cmake_minimum_required(VERSION 3.12)

add_executable(dm)

target_sources(
  dm PRIVATE
  src/main.cpp
)

target_compile_features(dm PRIVATE c_std_17)
target_compile_features(dm PRIVATE cxx_std_23)
target_compile_options(dm PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(dm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(dm PRIVATE libc libcxx)

target_link_options(
  dm
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)
