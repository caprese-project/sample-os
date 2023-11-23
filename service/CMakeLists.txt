cmake_minimum_required(VERSION 3.12)

add_library(service)

target_sources(
  service PRIVATE
  src/mm.c
)

target_compile_features(service PRIVATE c_std_17)
target_compile_options(service PRIVATE ${CONFIG_C_OPTIONS})

target_include_directories(service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${ROOT_DIR}/mm/include)

target_link_libraries(service PUBLIC libc)
