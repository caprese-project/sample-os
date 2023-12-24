#ifndef APM_IPC_H_
#define APM_IPC_H_

#define APM_MSG_TYPE_CREATE 1
#define APM_MSG_TYPE_LOOKUP 2

#define APM_CODE_S_OK           0
#define APM_CODE_E_FAILURE      1
#define APM_CODE_E_ILL_ARGS     2
#define APM_CODE_E_NO_SUCH_FILE 3

#define APM_CREATE_FLAG_DEFAULT   0
#define APM_CREATE_FLAG_SUSPENDED (1 << 0)
#define APM_CREATE_FLAG_DETACHED  (1 << 1)

#endif // APM_IPC_H_
