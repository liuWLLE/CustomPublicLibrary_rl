#include <time.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include "rllog.h"

// rl_log文件读写互斥锁
static pthread_mutex_t rl_log_mutex = PTHREAD_MUTEX_INITIALIZER;
// rl_log文件句柄
static int log_file_fd = -1;
// 进程pid
static pid_t pid_now;
// 进程名称（一般小于16个字符）
static char proc_name[17] = {0};
// 日志等级
static RL_LOG_LEVEL cur_rl_log_level;

// 写入日志函数
static int write_rl_log(RL_LOG_LEVEL level,const char *message)
{
    // 检查文件状态
    if (log_file_fd == -1)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:write_rl_log:24] Failed to open:%s", RL_LOG_FILE_DIR);
        perror(err_msg);
        return RL_FAILED;
    }

    // 格式化日志内容
    int log_len = 0;
    char log_buf[RL_LOG_BUF_SIZE + 64] = {0};
    if (level != RL_LOG_LEVEL_NORMAL)
    {
        // 更新时间
        time_t time_now;
        time(&time_now);
        struct tm time_info;
        localtime_r(&time_now, &time_info);
        // 获取线程号
        int tid_now = syscall(SYS_gettid);
        log_len = snprintf(log_buf, sizeof(log_buf),
                            "%02d:%02d:%02d-%s-p%d-t%d %s:%s\n",
                            time_info.tm_hour, time_info.tm_min, time_info.tm_sec,
                            proc_name, pid_now, tid_now,
                            (level == RL_LOG_LEVEL_ERROR) ? "error" :
                            (level == RL_LOG_LEVEL_WARN) ? "warn" :
                            (level == RL_LOG_LEVEL_INFO) ? "info": "debug",
                            message);
    }
    else
    {
        log_len = snprintf(log_buf, sizeof(log_buf), "%s", message);
    }
    if (log_len < 0)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:write_rl_log:48] Failed to write rl_log in:%s", RL_LOG_FILE_DIR);
        perror(err_msg);
        return RL_FAILED;
    }

    // 加锁
    pthread_mutex_lock(&rl_log_mutex);
    // 检查日志文件大小
    struct stat st;
    // 超过 RL_LOG_FILE_SIZE KB 则清空文件后再写入
    if (stat(RL_LOG_FILE_DIR, &st) == 0 && st.st_size > RL_LOG_FILE_SIZE)
    {
        // 清空文件内容
        if (ftruncate(log_file_fd, 0) == -1)
        {
            char err_msg[128] = {0};
            snprintf(err_msg, sizeof(err_msg), "[rllog:write_rl_log:63] Failed to clear:%s", RL_LOG_FILE_DIR);
            perror(err_msg);
            // 解锁
            pthread_mutex_unlock(&rl_log_mutex);
            return RL_FAILED;
        }
        // 将文件指针移动到文件开头
        if (lseek(log_file_fd, 0, SEEK_SET) == -1)
        {
            char err_msg[128] = {0};
            snprintf(err_msg, sizeof(err_msg), "[rllog:write_rl_log:73] Failed to reset pointer:%s", RL_LOG_FILE_DIR);
            perror(err_msg);
            // 解锁
            pthread_mutex_unlock(&rl_log_mutex);
            return RL_FAILED;
        }
    }
    // log 写入文件
    ssize_t bytes_written = 0;
    while (bytes_written < log_len)
    {
        ssize_t ret = write(log_file_fd, log_buf + bytes_written, log_len - bytes_written);
        if (ret == -1)
        {
            char err_msg[128] = {0};
            snprintf(err_msg, sizeof(err_msg), "[rllog:write_rl_log:88] Failed to write log:%s", RL_LOG_FILE_DIR);
            perror(err_msg);
            // 解锁
            pthread_mutex_unlock(&rl_log_mutex);
            return RL_FAILED;
        }
        bytes_written += ret;
    }
    // 解锁
    pthread_mutex_unlock(&rl_log_mutex);
    return RL_SUCCESS;
}

// 仅允许在main.cpp中使用
int rl_log_init(RL_LOG_LEVEL level)
{
    // 初始化lod等级
    cur_rl_log_level = level;
    // 只读打开 /proc/self/comm 文件以获取进程名称
    FILE *proc_file = fopen("/proc/self/comm", "r");
    if (!proc_file)
    {
        perror("[rllog:rl_log_init:104] Failed to open /proc/self/comm file");
        return RL_FAILED;
    }
    if (fgets(proc_name, sizeof(proc_name) - 1, proc_file) == NULL)
    {
        perror("[rllog:rl_log_init:106] fgets failed");
        fclose(proc_file);
        return RL_FAILED;
    }
    // 将换行符替换为字符串结束符 '\0'
    proc_name[strcspn(proc_name, "\n")] = '\0';
    fclose(proc_file);
    // 获取进程pid
    pid_now = getpid();
    // O_WRONLY（只写）| O_CREAT（创建文件）| O_APPEND（追加模式）| 拥有者可以读写，其他人只能读
    log_file_fd = open(RL_LOG_FILE_DIR, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_file_fd == -1)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_init:128] Failed to open:%s", RL_LOG_FILE_DIR);
        perror(err_msg);
        return RL_FAILED;
    }
    return RL_SUCCESS;
}

// 仅允许在main.cpp中使用
int rl_log_deint()
{
    if (log_file_fd == -1)
    {
        return RL_FAILED;
    }
    else
    {
        // 强制将文件数据同步到磁盘后关闭
        if (fsync(log_file_fd) == -1 || close(log_file_fd) == -1)
        {
            perror("[rllog:rl_log_deint:135] Failed to close log file");
            return RL_FAILED;
        }
        log_file_fd = -1;
    }
    return RL_SUCCESS;
}

// 通用日志函数
static int rl_log_generic(RL_LOG_LEVEL level, const char *fmt, va_list args)
{
    if (cur_rl_log_level < level)
    {
        return RL_FAILED;
    }
    char buf[RL_LOG_BUF_SIZE];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len < 0 || len >= RL_LOG_BUF_SIZE)
    {
        perror("[rllog:rl_log_generic:166] Log message truncated or encoding error");
        return RL_FAILED;
    }
    if (write_rl_log(level, buf) != RL_SUCCESS)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_generic:172] Failed to write log in:%s", RL_LOG_FILE_DIR);
        perror(err_msg);
        return RL_FAILED;
    }
    return RL_SUCCESS;
}

void rl_log_debug(const char *fmt, ...)
{
    // 可变参函数
    va_list arg;
    // 初始化arg
    va_start(arg, fmt);
    if (rl_log_generic(RL_LOG_LEVEL_DEBUG, fmt, arg) != RL_SUCCESS)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_debug:188] Failed output log");
        perror(err_msg);
    }
    // 结束arg
    va_end(arg);
}

void rl_log_info(const char *fmt, ...)
{
    // 可变参函数
    va_list arg;
    // 初始化arg
    va_start(arg, fmt);
    if (rl_log_generic(RL_LOG_LEVEL_INFO, fmt, arg) != RL_SUCCESS)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_info:204] Failed output log");
        perror(err_msg);
    }
    // 结束arg
    va_end(arg);
}

void rl_log_warn(const char *fmt, ...)
{
    // 可变参函数
    va_list arg;
    // 初始化arg
    va_start(arg, fmt);
    if (rl_log_generic(RL_LOG_LEVEL_WARN, fmt, arg) != RL_SUCCESS)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_warn:220] Failed output log");
        perror(err_msg);
    }
    // 结束arg
    va_end(arg);
}

void rl_log_error(const char *fmt, ...)
{
    // 可变参函数
    va_list arg;
    // 初始化arg
    va_start(arg, fmt);
    if (rl_log_generic(RL_LOG_LEVEL_ERROR, fmt, arg) != RL_SUCCESS)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_error:236] Failed output log");
        perror(err_msg);
    }
    // 结束arg
    va_end(arg);
}

void rl_log_normal(const char *fmt, ...)
{
    // 可变参函数
    va_list arg;
    // 初始化arg
    va_start(arg, fmt);
    if (rl_log_generic(RL_LOG_LEVEL_NORMAL, fmt, arg) != RL_SUCCESS)
    {
        char err_msg[128] = {0};
        snprintf(err_msg, sizeof(err_msg), "[rllog:rl_log_normal:236] Failed output log");
        perror(err_msg);
    }
    // 结束arg
    va_end(arg);
}