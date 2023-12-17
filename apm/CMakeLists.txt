cmake_minimum_required(VERSION 3.12)

init_ramfs()
add_ramfs(dm)
add_ramfs(plic)
add_ramfs(ns16550a)
add_ramfs(virtio)
make_ramfs()

add_executable(apm)

target_sources(
  apm PRIVATE
  src/elf_loader.cpp
  src/main.cpp
  src/program_loader.cpp
  src/ramfs.cpp
  src/section.s
  src/server.cpp
  src/task_manager.cpp
)

target_compile_features(apm PRIVATE c_std_17)
target_compile_features(apm PRIVATE cxx_std_23)
target_compile_options(apm PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(apm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(apm PRIVATE libc libcxx)

add_custom_target(
  apm_linker
  COMMAND ${CMAKE_C_COMPILER} -I ${GENERATE_DIR} -E -P -x c ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS >${CMAKE_CURRENT_BINARY_DIR}/linker.ld
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/linker.ld
)
add_dependencies(apm_linker ramfs_size)
add_dependencies(apm apm_linker)

target_link_options(
  apm
  PRIVATE
  -T ${CMAKE_CURRENT_BINARY_DIR}/linker.ld
  -nostdlib
  -z max-page-size=4096
)

add_custom_target(
  apm_elf
  COMMAND ${CMAKE_OBJCOPY} --strip-unneeded --update-section .ramfs=${GENERATE_DIR}/ramfs $<TARGET_FILE:apm> $<TARGET_FILE_DIR:apm>/apm.elf
)
add_dependencies(apm_elf apm)

add_custom_target(
  apm_size
  COMMAND sh "-c" "ls -l $<TARGET_FILE_DIR:apm>/apm.elf | awk '{print \"#define CONFIG_APM_ELF_SIZE \"$5}' > ${GENERATE_DIR}/apm_elf_size.h"
  BYPRODUCTS ${GENERATE_DIR}/apm_elf_size.h
  VERBATIM
)
add_dependencies(apm_size apm_elf)
