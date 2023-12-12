#ifndef APM_ELF_LOADER_H_
#define APM_ELF_LOADER_H_

#include <apm/program_loader.h>
#include <cstddef>
#include <cstdint>

class elf_loader: public program_loader {
  static constexpr const uint8_t magic[4] = { '\x7f', 'E', 'L', 'F' };

  enum struct xlen : uint8_t {
    xlen32 = 1,
    xlen64 = 2,
  };

  enum struct endian : uint8_t {
    little = 1,
    big    = 2,
  };

  enum struct version : uint8_t {
    current = 1,
  };

  enum struct type : uint16_t {
    none        = 0,
    relocatable = 1,
    executable  = 2,
    shared      = 3,
    core        = 4,
  };

  enum struct machine : uint16_t {
    riscv = 0xf3,
  };

  enum struct segment_type : uint32_t {
    null      = 0,
    load      = 1,
    dynamic   = 2,
    interpret = 3,
    note      = 4,
  };

  enum struct segment_flag : uint32_t {
    executable = 1 << 0,
    writable   = 1 << 1,
    readable   = 1 << 2,
  };

  struct file_header {
    struct {
      uint8_t _magic[4];
      xlen    _xlen;
      endian  _endian;
      version _version;
      uint8_t _os_abi;
      uint8_t _os_abi_version;
      uint8_t _pad[7];
    } e_ident;

    type     e_type;
    machine  e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
  };

  struct program_header {
    segment_type p_type;
    segment_flag p_flags;
    uint64_t     p_offset;
    uint64_t     p_vaddr;
    uint64_t     p_paddr;
    uint64_t     p_filesz;
    uint64_t     p_memsz;
    uint64_t     p_align;
  };

private:
  bool is_valid_format();

public:
  using program_loader::program_loader;

  virtual bool load() override;
};

#endif // APM_ELF_LOADER_H_
