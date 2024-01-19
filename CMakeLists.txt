cmake_minimum_required(VERSION 3.20)

project(
  sample-os
  VERSION 0.0.1
  HOMEPAGE_URL https://github.com/caprese-project/sample-os
  LANGUAGES ASM C CXX
)

set(
  CONFIG_COMPILE_OPTIONS
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
  $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
)

set(GENERATE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATE_DIR})

set(TMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/tmp)
file(MAKE_DIRECTORY ${TMP_DIR})

include(FetchContent)

set(LIBCAPRESE_COMPILE_OPTIONS ${CONFIG_COMPILE_OPTIONS})

FetchContent_Declare(
  libcaprese
  GIT_REPOSITORY https://github.com/caprese-project/libcaprese.git
)
FetchContent_MakeAvailable(libcaprese)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DEV_INTERFACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dev/if)

include(cmake/ramfs.cmake)

init_ramfs()

add_subdirectory(mm)
add_subdirectory(apm)
add_subdirectory(dm)
add_subdirectory(dev)
add_subdirectory(fs)
add_subdirectory(ramfs)
add_subdirectory(cons)
add_subdirectory(shell)
add_subdirectory(echo)
add_subdirectory(clear)
add_subdirectory(printenv)
add_subdirectory(pwd)
add_subdirectory(ls)
add_subdirectory(touch)

make_ramfs()

add_subdirectory(init)

add_subdirectory(libc)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(CONFIG_LOG_LEVEL all)
else()
  set(CONFIG_LOG_LEVEL fatal)
endif()

if (NOT DEFINED PLATFORM)
  set(PLATFORM qemu-riscv-virt)
endif()

if (NOT DEFINED CONFIG_OUTPUT_DIR)
  set(CONFIG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

FetchContent_Declare(
  caprese
  GIT_REPOSITORY https://github.com/caprese-project/caprese.git
)
FetchContent_MakeAvailable(caprese)
