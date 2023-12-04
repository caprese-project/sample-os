#ifndef MM_IPC_H_
#define MM_IPC_H_

#define MM_MSG_TYPE_ATTACH   1
#define MM_MSG_TYPE_DETACH   2
#define MM_MSG_TYPE_SBRK     3
#define MM_MSG_TYPE_FETCH    4
#define MM_MSG_TYPE_RETRIEVE 5
#define MM_MSG_TYPE_REVOKE   6

#define MM_CODE_S_OK       0
#define MM_CODE_E_ILL_ARGS 1

#define MM_FETCH_FLAG_READ  (1 << 0)
#define MM_FETCH_FLAG_WRITE (1 << 1)
#define MM_FETCH_FLAG_EXEC  (1 << 2)
#define MM_FETCH_FLAG_DEV   (1 << 3)

#endif // MM_IPC_H_
