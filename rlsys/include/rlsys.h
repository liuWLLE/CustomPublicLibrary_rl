#ifndef RL_SYS_H
#define RL_SYS_H

#include "public.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    unsigned long long last_total;
    unsigned long long last_idle;
    bool valid;
} cpu_usage_t;

// CPU 温度
#define SYS_CPU_GET_TEMPERATURE_FILE    "/sys/class/thermal/thermal_zone0/temp"
// CPU 信息
#define SYS_CPU_GET_INFORMATION_FILE    "/proc/stat"
// 程序路径
#define SYS_PROC_GET_PATH_FILE          "/proc/self/exe"
// 进程状态
#define SYS_PROC_SELF_STATUS_FILE        "/proc/self/status"
// 进程状态（PID）
#define SYS_PROC_STATUS_PATH_FORMAT     "/proc/%d/status"
// 进程名称
#define SYS_PROC_GET_NAME_FILE          "/proc/self/comm"
// 系统信息
#define SYS_PROC_MEMORY_INFO_FILE       "/proc/meminfo"
// 进程文件句柄信息
#define SYS_PROC_FD_FILE_FORMAT         "/proc/%d/fd"
// 进程路径
#define SYS_PROC_DIR                    "/proc"
// 进程名称文件
#define SYS_PROC_NAME_FORMAT            "/proc/%s/comm"
// 文件系统根目录
#define SYS_ROOT_PATH                   "/"

// 采样并计算CPU利用率
int rl_get_cpu_usage(cpu_usage_t *tracker);

// 查看当前cpu温度（51440）
int rl_get_cpu_temperature();

// 查看当前程序的路径
int rl_get_proc_path(char *buf, unsigned int len);

// 查看当前程序所在的目录
int rl_get_proc_dir(char *buf, unsigned int len);

// 获取文件相对程序的绝对路径
int rl_get_file_abs_path(char *buf, unsigned int len, const char *file_path);

// 获取进程使用的文件句柄数量
int rl_get_self_proc_fd_count();

// 通过PID获取进程使用的文件句柄数量
int rl_get_proc_fd_count_by_pid(pid_t pid);

// 获取当前进程使用的运行内存情况（单位 KB）
int rl_get_self_proc_memory_usage();

// 通过pid获取进程使用的运行内存情况（单位 KB）
int rl_get_proc_memory_usage_by_pid(pid_t pid);

// 通过进程名称获取pid
int rl_get_proc_pid_by_name(const char *proc_name);

// 获取系统的总运行内存
int rl_get_sys_memory_total();

// 获取系统可用运行内存
int rl_get_sys_memory_available();

// 获取根目录文件系统的总存储大小
int rl_get_sys_storage_total_mb();

// 获取根目录文件系统的可用存储大小
int rl_get_sys_storage_available_mb();

// 获取当前进程的名称
int rl_get_proc_name(char *name, unsigned int len);

// 计算代码运行时间
double rl_calcuate_run_time_ms(const struct timespec *start);

#ifdef __cplusplus
}
#endif

#endif