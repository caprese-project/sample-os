#ifndef MM_IPC_H_
#define MM_IPC_H_

#define MM_MSG_TYPE_ATTACH  1
#define MM_MSG_TYPE_DETACH  2
#define MM_MSG_TYPE_VMAP    3
#define MM_MSG_TYPE_VREMAP  4
#define MM_MSG_TYPE_VPMAP   5
#define MM_MSG_TYPE_VPREMAP 6
#define MM_MSG_TYPE_FETCH   7
#define MM_MSG_TYPE_REVOKE  8
#define MM_MSG_TYPE_INFO    9

#define MM_CODE_S_OK               0
#define MM_CODE_E_FAILURE          1
#define MM_CODE_E_ILL_ARGS         2
#define MM_CODE_E_ALREADY_ATTACHED 3
#define MM_CODE_E_NOT_ATTACHED     4
#define MM_CODE_E_OVERFLOW         5
#define MM_CODE_E_ALREADY_MAPPED   6
#define MM_CODE_E_NOT_MAPPED       7

#define MM_VMAP_FLAG_READ  (1 << 0)
#define MM_VMAP_FLAG_WRITE (1 << 1)
#define MM_VMAP_FLAG_EXEC  (1 << 2)
#define MM_VMAP_FLAG_VPCAP (1 << 3)

#define MM_STACK_DEFAULT 0
#define MM_TOTAL_DEFAULT 0
#define MM_VA_RAMDOM     0

#endif // MM_IPC_H_
