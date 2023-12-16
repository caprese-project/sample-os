cmake_minimum_required(VERSION 3.12)

add_subdirectory(dev/plic)
add_subdirectory(dev/ns16550a)
add_subdirectory(dev/virtio)

set(RAMFS_TARGETS)

macro(init_ramfs)
  file(REMOVE_RECURSE ${TMP_DIR}/ramfs)
  file(MAKE_DIRECTORY ${TMP_DIR}/ramfs)
endmacro(init_ramfs)

macro(add_ramfs target)
  list(APPEND RAMFS_TARGETS ${target})
endmacro(add_ramfs)

macro(make_ramfs)
  add_custom_target(
    ramfs
    COMMAND sh "-c" "find -mindepth 1 | cpio -o > ${GENERATE_DIR}/ramfs"
    WORKING_DIRECTORY ${TMP_DIR}/ramfs
    BYPRODUCTS ${GENERATE_DIR}/ramfs
    VERBATIM
  )

  foreach(target IN LISTS RAMFS_TARGETS)
    add_custom_target(
      ramfs_${target}
      COMMAND ${CMAKE_OBJCOPY} --strip-unneeded $<TARGET_FILE:${target}> ${TMP_DIR}/ramfs/${target}
    )
    add_dependencies(ramfs_${target} ${target})
    add_dependencies(ramfs ramfs_${target})
  endforeach(target)

  add_custom_target(
    ramfs_size
    COMMAND sh "-c" "ls -l ${GENERATE_DIR}/ramfs | awk '{print \"#define CONFIG_RAMFS_SIZE \"$5}' > ${GENERATE_DIR}/ramfs_size.h"
    DEPENDS ramfs
    BYPRODUCTS ${GENERATE_DIR}/ramfs_size.h
    VERBATIM
  )
endmacro(make_ramfs)

init_ramfs()
add_ramfs(plic)
add_ramfs(ns16550a)
add_ramfs(virtio)
make_ramfs()

add_executable(dm)
add_dependencies(dm ramfs)

target_sources(
  dm PRIVATE
  src/main.cpp
  src/section.s
)

target_compile_features(dm PRIVATE c_std_17)
target_compile_features(dm PRIVATE cxx_std_23)
target_compile_options(dm PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(dm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(dm PRIVATE libc libcxx)

add_custom_target(
  dm_linker
  COMMAND ${CMAKE_C_COMPILER} -I ${GENERATE_DIR} -E -P -x c ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS >${CMAKE_CURRENT_BINARY_DIR}/linker.ld
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/linker.ld
)
add_dependencies(dm_linker ramfs_size)
add_dependencies(dm dm_linker)

target_link_options(
  dm
  PRIVATE
  -T ${CMAKE_CURRENT_BINARY_DIR}/linker.ld
  -nostdlib
  -z max-page-size=4096
)

add_custom_target(
  dm_elf
  COMMAND ${CMAKE_OBJCOPY} --strip-unneeded --update-section .ramfs=${GENERATE_DIR}/ramfs $<TARGET_FILE:dm> $<TARGET_FILE_DIR:dm>/dm.elf
)
add_dependencies(dm_elf dm)

add_custom_target(
  dm_size
  COMMAND sh "-c" "ls -l $<TARGET_FILE_DIR:dm>/dm.elf | awk '{print \"#define CONFIG_DM_ELF_SIZE \"$5}' > ${GENERATE_DIR}/dm_elf_size.h"
  BYPRODUCTS ${GENERATE_DIR}/dm_elf_size.h
  VERBATIM
)
add_dependencies(dm_size dm_elf)
