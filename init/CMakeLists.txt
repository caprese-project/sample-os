cmake_minimum_required(VERSION 3.12)

add_executable(init)

set(CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS 0xa0000000)

target_sources(
  init PRIVATE
  src/main.c
  src/section.s
  src/task.c
)

target_compile_features(init PRIVATE c_std_17)
target_compile_options(init PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(init PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(init PRIVATE libc)

add_custom_target(
  init_linker
  COMMAND ${CMAKE_C_COMPILER} -DCONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS=${CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS} -I ${GENERATE_DIR} -E -P -x c ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS >${CMAKE_CURRENT_BINARY_DIR}/linker.ld
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/linker.ld
)
add_dependencies(init_linker mm_size apm_size)
add_dependencies(init init_linker)

target_link_options(
  init
  PRIVATE
  -T ${CMAKE_CURRENT_BINARY_DIR}/linker.ld
  -nostdlib
  -z max-page-size=4096
)
set_target_properties(init PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ldS)

add_custom_target(
  root_task_payload
  COMMAND ${CMAKE_OBJCOPY} -O binary --update-section .apm_elf=$<TARGET_FILE_DIR:apm>/apm.elf --update-section .mm_elf=$<TARGET_FILE_DIR:mm>/mm.elf $<TARGET_FILE:init> $<TARGET_FILE_DIR:init>/payload
)
add_dependencies(root_task_payload init apm_elf mm_elf)

set(CONFIG_ROOT_TASK_PAYLOAD $<TARGET_FILE_DIR:init>/payload PARENT_SCOPE)
set(CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS ${CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS} PARENT_SCOPE)
