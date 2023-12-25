#ifndef INIT_LAUNCH_H_
#define INIT_LAUNCH_H_

#include <init/task.h>

void launch_mm(task_context_t* ctx);
void launch_apm(task_context_t* ctx);
void launch_fs(task_context_t* ctx);
void launch_ramfs(task_context_t* ctx);
void launch_dm(task_context_t* ctx);
void launch_shell(task_context_t* ctx);

#endif // INIT_LAUNCH_H_
