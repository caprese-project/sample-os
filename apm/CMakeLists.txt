cmake_minimum_required(VERSION 3.12)

add_executable(apm)

target_sources(
  apm PRIVATE
  src/main.c
)

target_compile_features(apm PRIVATE c_std_17)
target_compile_options(apm PRIVATE ${CONFIG_C_OPTIONS})

target_include_directories(apm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(apm PRIVATE libc)

target_link_options(
  apm
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_custom_target(
  apm_elf
  COMMAND ${CMAKE_OBJCOPY} --strip-unneeded $<TARGET_FILE:apm> $<TARGET_FILE_DIR:apm>/apm.elf
)
add_dependencies(apm_elf apm)

add_custom_target(
  apm_size
  COMMAND sh "-c" "ls -l $<TARGET_FILE_DIR:apm>/apm.elf | awk '{print \"#define CONFIG_APM_ELF_SIZE \"$5}' > ${GENERATE_DIR}/apm_elf_size.h"
  BYPRODUCTS ${GENERATE_DIR}/apm_elf_size.h
  VERBATIM
)
add_dependencies(apm_size apm_elf)
