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
  -ffreestanding
  -fno-common
  -mno-relax
  -fno-stack-protector
  -fno-exceptions
  -fno-pie
  -no-pie
  $<IF:$<CONFIG:Debug>,-O0,-O3>
  $<IF:$<CONFIG:Debug>,-g3,-g0>
)

include(FetchContent)
include(ExternalProject)

add_subdirectory(init)
add_subdirectory(apm)
add_subdirectory(mm)
add_subdirectory(fs)

add_subdirectory(libc)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(CONFIG_LOG_LEVEL all)
else()
  set(CONFIG_LOG_LEVEL fatal)
endif()

set(PLATFORM qemu-riscv-virt)
set(CONFIG_ROOT_TASK_PAYLOAD $<TARGET_FILE_DIR:init>/payload)
set(CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS 0xa0000000)

ExternalProject_Add(
  caprese
  GIT_REPOSITORY https://github.com/caprese-project/caprese.git
  CMAKE_ARGS -DPLATFORM=${PLATFORM} -DCONFIG_ROOT_TASK_PAYLOAD=${CONFIG_ROOT_TASK_PAYLOAD} -DCONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS=${CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS} -DCONFIG_LOG_LEVEL=${CONFIG_LOG_LEVEL} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  INSTALL_COMMAND ""
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/caprese-prefix/src/caprese-build/image.elf
)

ExternalProject_Add_StepDependencies(caprese build root_task_payload)

add_custom_command(
  TARGET caprese POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/caprese-prefix/src/caprese-build/image.elf ${CMAKE_CURRENT_BINARY_DIR}/image.elf
)
