#include <crt/global.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fs/ipc.h>
#include <iosfwd>
#include <iterator>
#include <libcaprese/cxx/id_map.h>
#include <libcaprese/syscall.h>
#include <ramfs/server.h>
#include <service/mm.h>
#include <utility>

id_cap_t ramfs_id_cap;

namespace {
  struct cpio_newc_header {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
  };

  constexpr const char CPIO_MAGIC[]   = "070701";
  constexpr const char CPIO_EOF_TAG[] = "TRAILER!!!";

  const char* cpio_data;

  size_t round_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  std::pair<const char*, size_t> find_file(const char* file_name) {
    const char* ptr = cpio_data;
    while (true) {
      const cpio_newc_header* header = (const cpio_newc_header*)ptr;

      if (strncmp(header->magic, CPIO_MAGIC, sizeof(header->magic)) != 0) {
        break;
      }

      char name_size_str[sizeof(header->namesize) + 1];
      memcpy(name_size_str, header->namesize, sizeof(header->namesize));
      name_size_str[sizeof(header->namesize)] = '\0';

      size_t name_size = strtoull(name_size_str, nullptr, 16);

      char data_size_str[sizeof(header->filesize) + 1];
      memcpy(data_size_str, header->filesize, sizeof(header->filesize));
      data_size_str[sizeof(header->filesize)] = '\0';

      size_t data_size = strtoull(data_size_str, nullptr, 16);

      const char* name = ptr + sizeof(cpio_newc_header);
      const char* data = ptr + round_up(sizeof(cpio_newc_header) + name_size, 4);

      if (strcmp(name, CPIO_EOF_TAG) == 0) {
        break;
      } else if (strcmp(name, file_name) == 0) {
        return std::pair<const char*, size_t> { data, data_size };
      } else {
        ptr = data + round_up(data_size, 4);
      }
    }

    return std::pair<const char*, size_t> { nullptr, 0 };
  }

  struct file_info {
    const char*    start;
    const char*    end;
    std::streampos pos;
  };

  caprese::id_map<file_info> file_table;

  void open(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_OPEN);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    const char* path  = reinterpret_cast<const char*>(&msg_buf->data[msg_buf->cap_part_length + 1]);
    auto [data, size] = find_file(path);

    if (data == nullptr) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_NO_SUCH_FILE;
      return;
    }

    id_cap_t fd = unwrap_sysret(sys_id_cap_create());
    file_table.emplace(fd, file_info { data, data + size, 0 });

    msg_buf->cap_part_length                = 1;
    msg_buf->data[0]                        = msg_buf_transfer(unwrap_sysret(sys_id_cap_copy(fd)));
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = FS_CODE_S_OK;
  }

  void close(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_CLOSE);

    if (msg_buf->cap_part_length != 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd = msg_buf->data[1];
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (!file_table.contains(fd)) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    file_table.erase(fd);

    sys_cap_destroy(fd);

    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = FS_CODE_S_OK;
  }

  void read(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_READ);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd = msg_buf->data[1];

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (!file_table.contains(fd)) [[unlikely]] {
      sys_cap_destroy(fd);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    std::streamsize size = std::min(msg_buf->data[msg_buf->cap_part_length + 1], (std::size(msg_buf->data) - 2) * sizeof(uintptr_t));

    msg_buf->cap_part_length = 0;

    file_info&      file_info = file_table.at(fd);
    std::streamsize file_size = file_info.end - file_info.start;
    std::streamsize rem_size  = file_size - static_cast<std::streamsize>(file_info.pos);
    msg_buf->data[1]          = std::min(size, rem_size);

    memcpy(&msg_buf->data[2], file_info.start + file_info.pos, msg_buf->data[1]);
    file_info.pos += msg_buf->data[1];

    if (size > rem_size) {
      msg_buf->data[0] = FS_CODE_E_EOF;
    } else {
      msg_buf->data[0] = FS_CODE_S_OK;
    }

    msg_buf->data_part_length = 2 + (msg_buf->data[1] + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    sys_cap_destroy(fd);
  }

  void seek(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_SEEK);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd = msg_buf->data[1];

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (!file_table.contains(fd)) [[unlikely]] {
      sys_cap_destroy(fd);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    std::streampos pos = static_cast<std::streampos>(msg_buf->data[msg_buf->cap_part_length + 1]);

    if (pos < 0) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
    }

    file_info&      file_info = file_table.at(fd);
    std::streamsize file_size = file_info.end - file_info.start;

    if (pos > file_size) {
      pos = file_size;
    }

    file_info.pos = pos;

    msg_buf->cap_part_length                = 0;
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = FS_CODE_S_OK;

    sys_cap_destroy(fd);
  }

  void tell(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_TELL);

    if (msg_buf->cap_part_length != 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
    }

    id_cap_t fd = msg_buf->data[1];

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
    }

    if (!file_table.contains(fd)) [[unlikely]] {
      sys_cap_destroy(fd);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
    }

    file_info& file_info = file_table.at(fd);

    msg_buf->cap_part_length                = 0;
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = static_cast<uintptr_t>(file_info.pos);

    sys_cap_destroy(fd);
  }

  // clang-format off

  void (*const table[])(message_buffer_t*) = {
    [0]                   = nullptr,
    [FS_MSG_TYPE_MOUNT]   = nullptr,
    [FS_MSG_TYPE_UNMOUNT] = nullptr,
    [FS_MSG_TYPE_MOUNTED] = nullptr,
    [FS_MSG_TYPE_OPEN]    = open,
    [FS_MSG_TYPE_CLOSE]   = close,
    [FS_MSG_TYPE_READ]    = read,
    [FS_MSG_TYPE_WRITE]   = nullptr,
    [FS_MSG_TYPE_SEEK]    = seek,
    [FS_MSG_TYPE_TELL]    = tell,
    [FS_MSG_TYPE_MKDIR]   = nullptr,
    [FS_MSG_TYPE_RMDIR]   = nullptr,
  };

  // clang-format on

  void proc_msg(message_buffer_t* msg_buf) {
    if (msg_buf->cap_part_length == 0 || msg_buf->data_part_length == 0) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t id = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(id)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (unwrap_sysret(sys_id_cap_compare(id, ramfs_id_cap)) != 0) [[unlikely]] {
      sys_cap_destroy(id);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    if (msg_type < FS_MSG_TYPE_OPEN || msg_type > FS_MSG_TYPE_RMDIR) [[unlikely]] {
      sys_cap_destroy(id);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_UNSUPPORTED;
      return;
    }

    if (table[msg_type] == nullptr) [[unlikely]] {
      sys_cap_destroy(id);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_UNSUPPORTED;
      return;
    }

    table[msg_type](msg_buf);

    sys_cap_destroy(id);
  }
} // namespace

[[noreturn]] void run(uintptr_t ramfs_va_base) {
  cpio_data = reinterpret_cast<const char*>(ramfs_va_base);

  message_buffer_t msg_buf;
  sysret_t         sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) [[unlikely]] {
      cap_space_cap_t cap_space_cap = mm_fetch_and_create_cap_space_object();
      unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
    }

    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(__this_ep_cap, &msg_buf);
    } else {
      sysret = sys_endpoint_cap_receive(__this_ep_cap, &msg_buf);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(&msg_buf);
    }
  }
}
