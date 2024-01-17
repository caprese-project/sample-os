#ifndef FS_DIRECTORY_H_
#define FS_DIRECTORY_H_

#include <fs/ipc.h>
#include <fs/mount_point.h>
#include <functional>
#include <libcaprese/cap.h>
#include <map>
#include <optional>
#include <string>
#include <string_view>

enum struct directory_type {
  regular_directory = FS_FT_DIR,
  mount_point       = FS_FT_MNT,
};

class directory {
  std::string                                   abs_path;
  std::map<std::string, directory, std::less<>> subdirs;
  std::optional<mount_point>                    mnt;
  directory_type                                type;

public:
  directory(std::string_view abs_path);

  directory(const directory&)            = delete;
  directory& operator=(const directory&) = delete;
  directory(directory&&)                 = default;
  directory& operator=(directory&&)      = default;

  [[nodiscard]] const std::string& get_abs_path() const;
  [[nodiscard]] directory_type     get_type() const;
  [[nodiscard]] std::string_view   get_name() const;
  [[nodiscard]] bool               contains(std::string_view name) const;
  [[nodiscard]] directory&         get_dir(std::string_view name);
  [[nodiscard]] const directory&   get_dir(std::string_view name) const;
  [[nodiscard]] bool               mount(endpoint_cap_t ep_cap);
  [[nodiscard]] id_cap_t           get_fs_id() const;
  [[nodiscard]] bool               get_info(fs_file_info* buffer) const;
  [[nodiscard]] std::streamsize    read(std::streampos pos, fs_file_info* buffer);

  [[nodiscard]] std::optional<std::reference_wrapper<directory>>                              find_directory(std::string_view path);
  [[nodiscard]] std::optional<std::pair<std::reference_wrapper<directory>, std::string_view>> find_mount_point(std::string_view path);
  [[nodiscard]] std::optional<std::reference_wrapper<directory>>                              create_directories(std::string_view path);
  [[nodiscard]] std::optional<std::reference_wrapper<directory>>                              create_mount_point(std::string_view path, endpoint_cap_t ep);
  [[nodiscard]] bool                                                                          remove_mount_point(std::string_view path);
  [[nodiscard]] bool                                                                          is_mounted(std::string_view path);

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

#endif // FS_DIRECTORY_H_
