#ifndef RL_LOG_H
#define RL_LOG_H

#include "public.h"

#ifdef __cplusplus
extern "C"
{
#endif

// 日志路径
#define RL_LOG_FILE_DIR     "/tmp/Messages"
// 日志文件大小 10 MB
#define RL_LOG_FILE_SIZE    1024 * 1024 * 50
// 单条日志最大长度
#define RL_LOG_BUF_SIZE     1024

typedef enum
{
    RL_LOG_LEVEL_NORMAL,
    RL_LOG_LEVEL_ERROR,
    RL_LOG_LEVEL_WARN,
    RL_LOG_LEVEL_INFO,
    RL_LOG_LEVEL_DEBUG
} RL_LOG_LEVEL;

int rl_log_init(RL_LOG_LEVEL level);
int rl_log_deint();

// __attribute__((format(printf, format_index, args_index))) 的意思是：
// format_index：格式字符串参数在函数参数列表中的索引（从 1 开始）
// args_index：变参开始的参数索引（从 1 开始）
// 要求编译器检查格式字符串和传入参数类型/个数是否匹配

void rl_log_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void rl_log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void rl_log_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void rl_log_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void rl_log_normal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif
