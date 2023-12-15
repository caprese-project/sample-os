cmake_minimum_required(VERSION 3.12)

add_executable(mm)
target_sources(
  mm PRIVATE
  src/main.cpp
  src/memory_manager.cpp
  src/server.cpp
  src/task_table.cpp
)

target_compile_features(mm PRIVATE cxx_std_23)
target_compile_options(mm PRIVATE ${CONFIG_COMPILE_OPTIONS})

target_include_directories(mm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(mm PRIVATE libc libcxx)

target_link_options(
  mm
  PRIVATE
  -nostdlib
  -z max-page-size=4096
)

add_custom_target(
  mm_elf
  COMMAND ${CMAKE_OBJCOPY} --strip-unneeded $<TARGET_FILE:mm> $<TARGET_FILE_DIR:mm>/mm.elf
)
add_dependencies(mm_elf mm)

add_custom_target(
  mm_size
  COMMAND sh "-c" "ls -l $<TARGET_FILE_DIR:mm>/mm.elf | awk '{print \"#define CONFIG_MM_ELF_SIZE \"$5}' > ${GENERATE_DIR}/mm_elf_size.h"
  BYPRODUCTS ${GENERATE_DIR}/mm_elf_size.h
  VERBATIM
)
add_dependencies(mm_size mm_elf)
