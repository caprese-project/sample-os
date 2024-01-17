#ifndef FS_IPC_H_
#define FS_IPC_H_

#ifndef __cplusplus
#include <stddef.h>
#include <stdint.h>
#else // !__cplusplus
#include <cstddef>
#include <cstdint>
#endif // __cplusplus

#define FS_MSG_TYPE_MOUNT   1
#define FS_MSG_TYPE_UNMOUNT 2
#define FS_MSG_TYPE_MOUNTED 3
#define FS_MSG_TYPE_INFO    4
#define FS_MSG_TYPE_CREATE  5
#define FS_MSG_TYPE_REMOVE  6
#define FS_MSG_TYPE_OPEN    7
#define FS_MSG_TYPE_CLOSE   8
#define FS_MSG_TYPE_READ    9
#define FS_MSG_TYPE_WRITE   10
#define FS_MSG_TYPE_SEEK    11
#define FS_MSG_TYPE_TELL    12

#define FS_READ_MAX_SIZE  0x1000
#define FS_WRITE_MAX_SIZE 0x1000
#define FS_MSG_CAPACITY   (sizeof(uintptr_t) * 4 + FS_READ_MAX_SIZE)

#define FS_CODE_S_OK           0
#define FS_CODE_E_FAILURE      1
#define FS_CODE_E_ILL_ARGS     2
#define FS_CODE_E_NO_SUCH_FILE 3
#define FS_CODE_E_NOT_MOUNTED  4
#define FS_CODE_E_EOF          5
#define FS_CODE_E_REDIRECT     6
#define FS_CODE_E_UNSUPPORTED  7
#define FS_CODE_E_TYPE         8

#define FS_FT_REG 1
#define FS_FT_DIR 2
#define FS_FT_MNT 3

#define FS_FILE_NAME_SIZE_MAX 0xff

struct fs_file_info {
  uint8_t file_type;
  uint8_t file_name_size;
  size_t  file_size;
  char    file_name[FS_FILE_NAME_SIZE_MAX];
};

#endif // FS_IPC_H_
