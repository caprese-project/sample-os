#include <apm/elf_loader.h>
#include <apm/task_manager.h>
#include <cstring>
#include <iterator>
#include <libcaprese/syscall.h>
#include <memory>
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

  std::unique_ptr<char[]> buf = std::make_unique<char[]>(KILO_PAGE_SIZE);

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

    uintptr_t va_base = round_down(prog_header.p_vaddr, KILO_PAGE_SIZE);
    uintptr_t offset  = prog_header.p_vaddr % KILO_PAGE_SIZE;
    size_t    written = 0;

    std::streampos pos = stream.tellg();
    stream.seekg(prog_header.p_offset);

    if (offset != 0) {
      size_t len = std::min<size_t>(KILO_PAGE_SIZE - offset, prog_header.p_filesz);

      memset(buf.get(), 0, offset);
      stream.read(buf.get() + offset, len);
      memset(buf.get() + offset + len, 0, KILO_PAGE_SIZE - (offset + len));

      if (!map_page(va_base, KILO_PAGE, flags, buf.get())) {
        return false;
      }

      written += KILO_PAGE_SIZE - offset;
    }

    while (written < prog_header.p_filesz) {
      size_t len = std::min<size_t>(KILO_PAGE_SIZE, prog_header.p_filesz - written);

      stream.read(buf.get(), len);
      memset(buf.get() + len, 0, KILO_PAGE_SIZE - len);

      if (!map_page(va_base + offset + written, KILO_PAGE, flags, buf.get())) {
        return false;
      }

      written += KILO_PAGE_SIZE;
    }

    while (written < prog_header.p_memsz) {
      if (!map_page(va_base + offset + written, KILO_PAGE, flags, nullptr)) {
        return false;
      }

      written += KILO_PAGE_SIZE;
    }

    stream.seekg(pos);
  }

  target_ref.get().set_register(REG_PROGRAM_COUNTER, header.e_entry);

  return true;
}
