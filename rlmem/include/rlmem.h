#ifndef RL_MEM_H
#define RL_MEM_H

#include "public.h"

// 日志路径
#define RL_MEM_TRACE_LOG_FILE_DIR   "../log/memory_trace"

#ifdef __cplusplus
extern "C"
{
#endif

// 仅程序初始化时调用
void rl_memory_trace_init(bool state);

// 仅程序结束时调用（输出内存泄露情况到文件）
int rl_memory_trace_deinit();

void *rl_malloc(unsigned int size, const char *file, const char *func, int line);

int rl_free(void *ptr, const char *file, const char *func, int line);

void *rl_calloc(unsigned int block, unsigned int count, const char *file, const char *func, int line);

void *rl_realloc(void *ptr, unsigned int size, const char *file, const char *func, int line);

#ifdef __cplusplus
}
#endif

#endif
