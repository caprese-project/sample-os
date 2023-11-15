cmake_minimum_required(VERSION 3.12)

add_executable(init)

target_sources(
  init PRIVATE
  src/entry.S
  src/main.c
)

target_compile_features(init PRIVATE c_std_17)
target_compile_options(init PRIVATE ${CONFIG_C_OPTIONS})

target_link_libraries(init PRIVATE libc)

target_link_options(
  init
  PRIVATE
  -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
  -nostdlib
  -z max-page-size=4096
)
set_target_properties(init PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

add_custom_target(
  root_task_payload
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:init> $<TARGET_FILE_DIR:init>/payload
)
add_dependencies(root_task_payload init)
