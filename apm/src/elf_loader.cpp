#include <apm/elf_loader.h>
#include <apm/task_manager.h>
#include <cstring>
#include <iterator>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>

namespace {
  constexpr uintptr_t round_up(uintptr_t value, uintptr_t align) {
    return (value + align - 1) / align * align;
  }

  constexpr uintptr_t round_down(uintptr_t value, uintptr_t align) {
    return value / align * align;
  }
} // namespace

bool elf_loader::is_valid_format() {
  std::istream& stream = stream_ref.get();

  stream.seekg(0);

  file_header header;
  stream.read((char*)&header, sizeof(header));

  if (stream.fail()) [[unlikely]] {
    return false;
  }

  if (!std::equal(magic, magic + std::size(magic), header.e_ident._magic)) [[unlikely]] {
    return false;
  }

  if (header.e_ident._xlen != xlen::xlen64) [[unlikely]] {
    return false;
  }

  if (header.e_ident._endian != endian::little) [[unlikely]] {
    return false;
  }

  if (header.e_ident._version != version::current) [[unlikely]] {
    return false;
  }

  if (header.e_machine != machine::riscv) [[unlikely]] {
    return false;
  }

  if (header.e_type != type::executable) [[unlikely]] {
    return false;
  }

  stream.seekg(header.e_phoff);
  if (stream.fail()) [[unlikely]] {
    return false;
  }

  for (size_t i = 0; i < header.e_phnum; ++i) {
    program_header prog_header;
    stream.read((char*)&prog_header, sizeof(prog_header));

    if (stream.fail()) [[unlikely]] {
      return false;
    }

    if (prog_header.p_type != segment_type::load) {
      continue;
    }

    if (prog_header.p_filesz > prog_header.p_memsz) {
      return false;
    }
  }

  return true;
}

bool elf_loader::load() {
  if (!is_valid_format()) {
    return false;
  }

  std::istream& stream = stream_ref.get();
  stream.seekg(0);

  file_header header;
  stream.read((char*)&header, sizeof(header));

  stream.seekg(header.e_phoff);

  char* buf = new char[KILO_PAGE_SIZE];

  for (size_t i = 0; i < header.e_phnum; ++i) {
    program_header prog_header;
    stream.read((char*)&prog_header, sizeof(program_header));

    if (prog_header.p_type != segment_type::load) {
      continue;
    }

    bool readable   = static_cast<uint32_t>(prog_header.p_flags) & static_cast<uint32_t>(segment_flag::readable);
    bool writable   = static_cast<uint32_t>(prog_header.p_flags) & static_cast<uint32_t>(segment_flag::writable);
    bool executable = static_cast<uint32_t>(prog_header.p_flags) & static_cast<uint32_t>(segment_flag::executable);

    int flags = 0;
    if (readable) {
      flags |= MM_VMAP_FLAG_READ;
    }
    if (writable) {
      flags |= MM_VMAP_FLAG_WRITE;
    }
    if (executable) {
      flags |= MM_VMAP_FLAG_EXEC;
    }

    uintptr_t va_start  = round_down(prog_header.p_vaddr, KILO_PAGE_SIZE);
    uintptr_t va_end    = round_up(prog_header.p_vaddr + prog_header.p_memsz, KILO_PAGE_SIZE);
    size_t    va_offset = prog_header.p_vaddr % KILO_PAGE_SIZE;

    std::streampos pos = stream.tellg();
    stream.seekg(prog_header.p_offset);

    for (uintptr_t va = va_start; va < va_end; va += KILO_PAGE_SIZE) {
      char* data = nullptr;

      size_t offset = va - va_start;

      if (offset < prog_header.p_filesz + va_offset) {
        size_t size = KILO_PAGE_SIZE;
        if (offset + size > prog_header.p_filesz + va_offset) {
          size = prog_header.p_filesz + va_offset - offset;
        }

        memset(buf, 0, KILO_PAGE_SIZE);
        stream.read(buf, size);

        data = buf;
      }

      if (!map_page(va, KILO_PAGE, flags, data)) {
        delete[] buf;
        return false;
      }
    }

    stream.seekg(pos);
  }

  delete[] buf;

  if (sysret_failed(sys_task_cap_set_reg(target_ref.get().get_task_cap().get(), REG_PROGRAM_COUNTER, header.e_entry))) {
    return false;
  }

  return true;
}
