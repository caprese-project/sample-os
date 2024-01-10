#ifndef FS_IPC_H_
#define FS_IPC_H_

#define FS_MSG_TYPE_MOUNT   1
#define FS_MSG_TYPE_UNMOUNT 2
#define FS_MSG_TYPE_MOUNTED 3
#define FS_MSG_TYPE_EXISTS  4
#define FS_MSG_TYPE_OPEN    5
#define FS_MSG_TYPE_CLOSE   6
#define FS_MSG_TYPE_READ    7
#define FS_MSG_TYPE_WRITE   8
#define FS_MSG_TYPE_SEEK    9
#define FS_MSG_TYPE_TELL    10
#define FS_MSG_TYPE_MKDIR   11
#define FS_MSG_TYPE_RMDIR   12

#define FS_CODE_S_OK              0
#define FS_CODE_E_FAILURE         1
#define FS_CODE_E_ILL_ARGS        2
#define FS_CODE_E_NO_SUCH_FILE    3
#define FS_CODE_E_ALREADY_MOUNTED 4
#define FS_CODE_E_NOT_MOUNTED     5
#define FS_CODE_E_EOF             6
#define FS_CODE_E_UNSUPPORTED     7

#endif // FS_IPC_H_
