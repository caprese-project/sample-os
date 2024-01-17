#ifndef FS_MOUNT_POINT_H_
#define FS_MOUNT_POINT_H_

#include <fs/ipc.h>
#include <functional>
#include <libcaprese/cap.h>
#include <libcaprese/cxx/id_map.h>
#include <optional>
#include <string_view>

class mount_point {
  id_cap_t       fs_id;
  endpoint_cap_t fs_ep;
  bool           mounted;

  static caprese::id_map<mount_point&> fs_mount_point_table;
  static caprese::id_map<id_cap_t>     fd_fs_table;

public:
  [[nodiscard]] static std::optional<std::reference_wrapper<mount_point>> find_mount_point(id_cap_t fd) noexcept;

public:
  mount_point(id_cap_t id, endpoint_cap_t ep) noexcept;
  ~mount_point() noexcept;

  mount_point(const mount_point&)            = delete;
  mount_point& operator=(const mount_point&) = delete;
  mount_point(mount_point&&)                 = default;
  mount_point& operator=(mount_point&&)      = default;

  [[nodiscard]] bool mount() noexcept;
  [[nodiscard]] bool unmount() noexcept;

  [[nodiscard]] id_cap_t get_fs_id() const noexcept;
  [[nodiscard]] bool     is_mounted() const noexcept;

  [[nodiscard]] std::optional<fs_file_info> get_info(std::string_view path) noexcept;
  [[nodiscard]] bool                        create(std::string_view path, int type) noexcept;
  [[nodiscard]] bool                        remove(std::string_view path) noexcept;
  [[nodiscard]] id_cap_t                    open(std::string_view path) noexcept;
  [[nodiscard]] bool                        close(id_cap_t fd) noexcept;
  [[nodiscard]] std::streamsize             read(id_cap_t fd, char* buffer, std::streamsize size) noexcept;
  [[nodiscard]] std::streamsize             write(id_cap_t fd, std::string_view data) noexcept;
  [[nodiscard]] bool                        seek(id_cap_t fd, std::streamoff offset, int whence) noexcept;
  [[nodiscard]] std::streampos              tell(id_cap_t fd) noexcept;
};

#endif // FS_MOUNT_POINT_H_
