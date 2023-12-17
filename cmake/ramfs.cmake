cmake_minimum_required(VERSION 3.12)

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
    COMMAND sh "-c" "find -mindepth 1 | cpio -o -H newc > ${GENERATE_DIR}/ramfs"
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
