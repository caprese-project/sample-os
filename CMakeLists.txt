cmake_minimum_required(VERSION 3.20)

project(
  sample-os
  VERSION 0.0.1
  HOMEPAGE_URL https://github.com/caprese-project/sample-os
  LANGUAGES ASM C CXX
)

set(
  CONFIG_C_OPTIONS
  -Wall
  -Wextra
  -Werror
  -Wno-attributes
  -fno-omit-frame-pointer
  -gdwarf-2
  -MD
  -mcmodel=medany
  -fno-common
  -mno-relax
  -fno-stack-protector
  -fno-exceptions
  -fno-pie
  -no-pie
  $<IF:$<CONFIG:Debug>,-O0,-O3>
  $<IF:$<CONFIG:Debug>,-g3,-g0>
)

set(GENERATE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATE_DIR})

include(FetchContent)

set(LIBCAPRESE_COMPILE_OPTIONS ${CONFIG_C_OPTIONS})

FetchContent_Declare(
  libcaprese
  GIT_REPOSITORY https://github.com/caprese-project/libcaprese.git
)
FetchContent_MakeAvailable(libcaprese)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(init)
add_subdirectory(mm)
add_subdirectory(apm)
add_subdirectory(dev)
add_subdirectory(fs)

add_subdirectory(libc)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(CONFIG_LOG_LEVEL all)
else()
  set(CONFIG_LOG_LEVEL fatal)
endif()

set(PLATFORM qemu-riscv-virt)
set(CONFIG_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/image.elf)

FetchContent_Declare(
  caprese
  GIT_REPOSITORY https://github.com/caprese-project/caprese.git
)
FetchContent_MakeAvailable(caprese)
