#ifndef INIT_ELF_H_
#define INIT_ELF_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint16_t  elf_half_t;
typedef uint32_t  elf_word_t;
typedef int32_t   elf_sword_t;
typedef uintptr_t elf_off_t;
typedef uintptr_t elf_addr_t;

#define ELF_MAGIC_0 '\x7f'
#define ELF_MAGIC_1 'E'
#define ELF_MAGIC_2 'L'
#define ELF_MAGIC_3 'F'

#define ELF_XLEN_32          1
#define ELF_XLEN_64          2
#define ELF_LITTLE_ENDIAN    1
#define ELF_BIG_ENDIAN       2
#define ELF_CURRENT_VERSION  1
#define ELF_TYPE_NONE        0
#define ELF_TYPE_RELOCATABLE 1
#define ELF_TYPE_EXECUTABLE  2
#define ELF_TYPE_SHARED      3
#define ELF_TYPE_CORE        4
#define ELF_MACHINE_RISCV    0xf3

#define ELF_PH_SEGMENT_NULL      0
#define ELF_PH_SEGMENT_LOAD      1
#define ELF_PH_SEGMENT_DYNAMIC   2
#define ELF_PH_SEGMENT_INTERPRET 3
#define ELF_PH_SEGMENT_NOTE      4

#define ELF_PH_FLAG_EXECUTABLE (1 << 0)
#define ELF_PH_FLAG_WRITABLE   (1 << 1)
#define ELF_PH_FLAG_READABLE   (1 << 2)

typedef struct {
  uint8_t    magic[4];
  uint8_t    xlen;
  uint8_t    endian;
  uint8_t    elf_header_version;
  uint8_t    os_abi;
  uint8_t    os_abi_version;
  uint8_t    pad[7];
  elf_half_t type;
  elf_half_t machine;
  elf_word_t elf_version;
  elf_addr_t entry_position;
  elf_off_t  program_header_position;
  elf_off_t  section_header_position;
  elf_word_t flags;
  elf_half_t header_size;
  elf_half_t program_header_entry_size;
  elf_half_t num_program_header_entries;
  elf_half_t section_header_entry_size;
  elf_half_t num_section_header_entries;
  elf_half_t idx_section_header;
} elf_header_t;

typedef struct {
  elf_word_t segment_type;
  elf_word_t flags;
  elf_off_t  offset;
  elf_addr_t virtual_address;
  elf_addr_t physical_address;
  elf_off_t  file_size;
  elf_off_t  memory_size;
  elf_off_t  alignment;
} elf_program_header_t;

typedef struct {
  elf_word_t name;
  elf_word_t type;
  elf_word_t flags;
  elf_addr_t address;
  elf_off_t  offset;
  elf_word_t size;
  elf_word_t link;
  elf_word_t info;
  elf_word_t address_alignment;
  elf_word_t entry_size;
} elf_section_header_t;

#endif // INIT_ELF_H_
