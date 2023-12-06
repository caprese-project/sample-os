cmake_minimum_required(VERSION 3.20)

set(LIBC_COMPILE_OPTIONS ${CONFIG_COMPILE_OPTIONS})

FetchContent_Declare(
  caprese_libc
  GIT_REPOSITORY https://github.com/caprese-project/libc
)
FetchContent_GetProperties(caprese_libc)

if(NOT caprese_libc_POPULATED)
  FetchContent_MakeAvailable(caprese_libc)
endif()

add_library(libc OBJECT)

target_sources(
  libc PRIVATE
  src/crt/crt_entry.S
  src/crt/crt_startup.c
  src/crt/global.c
  src/crt/heap.c
  src/service/mm.c
  src/signal.c
  src/stdio.c
  src/stdlib.c
)

target_compile_features(libc PRIVATE c_std_17)
target_compile_options(libc PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(libc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${ROOT_DIR}/mm/include)

target_link_libraries(libc PUBLIC caprese_libc caprese_libcxx libcaprese)
