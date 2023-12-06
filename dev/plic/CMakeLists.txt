cmake_minimum_required(VERSION 3.12)

add_executable(plic)

target_sources(
  plic PRIVATE
  src/main.c
)

target_compile_features(plic PRIVATE c_std_17)
target_compile_options(plic PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(plic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(plic PRIVATE libc)

target_link_options(
  plic
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_custom_target(
  plic_elf
  COMMAND ${CMAKE_OBJCOPY} --strip-unneeded $<TARGET_FILE:plic> $<TARGET_FILE_DIR:plic>/plic.elf
)
add_dependencies(plic_elf plic)

add_custom_target(
  plic_size
  COMMAND sh "-c" "ls -l $<TARGET_FILE_DIR:plic>/plic.elf | awk '{print \"#define CONFIG_PLIC_ELF_SIZE \"$5}' > ${GENERATE_DIR}/plic_elf_size.h"
  BYPRODUCTS ${GENERATE_DIR}/plic_elf_size.h
  VERBATIM
)
add_dependencies(plic_size plic_elf)
