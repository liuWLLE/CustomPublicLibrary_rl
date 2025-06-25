#include "rlsys.h"
#include "rl/rlstr.h"
#include <libgen.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include <sys/statvfs.h>

#define __FILENAME__ "rlsys"

// 获取 CPU 利用率（966）
int rl_get_cpu_usage(cpu_usage_t *tracker)
{
    FILE *cpu_fp = fopen(SYS_CPU_GET_INFORMATION_FILE, "r");
    if (cpu_fp == NULL)
    {
        rl_log_error("[%s:%s:%d] open cur cpu information file failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // user：用户态耗时
    // nice：低优先级用户态耗时
    // system：内核态耗时
    // idle：空闲时间
    // iowait：IO 等待时间
    // irq：硬中断处理时间
    // softirq：软中断处理时间 
    char buf[256];
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
    if (fgets(buf, sizeof(buf), cpu_fp) == NULL)
    {
        fclose(cpu_fp);
        rl_log_error("[%s:%s:%d] fgets cpu file failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    if(fclose(cpu_fp) < 0)
    {
        rl_log_error("[%s:%s:%d] fclose cpu file failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);

    // 认为空闲的时间
    unsigned long long curr_idle = idle + iowait;
    // CPU 所有花费时间总和
    unsigned long long curr_total = user + nice + system + curr_idle + irq + softirq;

    // // 首次采样还未准备好结果
    if (tracker->valid == RL_FALSE)
    {
        tracker->last_total = curr_total;
        tracker->last_idle = curr_idle;
        tracker->valid = RL_TRUE;
        return 0;
    }

    // 时间跨度
    unsigned long long total_diff = curr_total - tracker->last_total;
    // 这段时间中“空闲”了多久
    unsigned long long idle_diff = curr_idle - tracker->last_idle;
    tracker->last_total = curr_total;
    tracker->last_idle = curr_idle;

    // 首次采样还未准备好结果
    if (total_diff == 0)
    {
        return 0;
    }
    // CPU使用率 = (总时间 - 空闲时间) / 总时间
    return (int)((total_diff - idle_diff) * 1000 / total_diff);
}

// 查看当前cpu温度（51440）
int rl_get_cpu_temperature()
{
    // 打开当前cpu温度保存文件
    int fd = open(SYS_CPU_GET_TEMPERATURE_FILE, O_RDONLY);
    if (fd < 0)
    {
        rl_log_error("[%s:%s:%d] open cur cpu temperature file failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 读取当前cpu温度
    char buf[16];
    if (read(fd, buf, sizeof(buf) - 1) <= 0)
    {
        rl_log_error("[%s:%s:%d] read cur cpu temperature failed", __FILENAME__, __FUNCTION__, __LINE__);
        close(fd);
        return RL_FAILED;
    }

    // 关闭文件
    close(fd);

    // 转换cpu温度
    int cur_temperatrue;
    // 去除换行符
    rl_str_rm_line(buf);
    if (rl_str_isdigit(buf) == RL_TRUE)
    {
        cur_temperatrue = rl_str_atoi(buf);
    }
    else
    {
        rl_log_error("[%s:%s:%d] cur cpu temperature:%s convert error", __FILENAME__, __FUNCTION__, __LINE__, buf);
        return RL_FAILED;
    }
    return cur_temperatrue;
}

// 查看当前程序的路径
int rl_get_proc_path(char *buf, unsigned int len)
{
    if (buf == NULL || len == 0)
    {
        rl_log_error("[%s:%s:%d] buf is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    int read_len = readlink(SYS_PROC_GET_PATH_FILE, buf, len - 1);
    if (read_len != -1)
    {
        buf[read_len] = '\0';
        return RL_SUCCESS;
    }
    rl_log_error("[%s:%s:%d] get proc path failed", __FILENAME__, __FUNCTION__, __LINE__);
    buf[0] = '\0';
    return RL_FAILED;
}

// 查看当前程序所在的目录
int rl_get_proc_dir(char *buf, unsigned int len)
{
    if (buf == NULL || len == 0)
    {
        rl_log_error("[%s:%s:%d] buf is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 临时缓冲区存储程序路径
    char temp_buf[len];
    int read_len = readlink(SYS_PROC_GET_PATH_FILE, temp_buf, len - 1);
    if (read_len == -1)
    {
        rl_log_error("[%s:%s:%d] get proc path failed", __FILENAME__, __FUNCTION__, __LINE__);
        buf[0] = '\0';
        return RL_FAILED;
    }
    temp_buf[read_len] = '\0';

    // 获取目录部分
    char *dir = dirname(temp_buf);
    snprintf(buf, len, "%s", dir);

    return RL_SUCCESS;
}

// 获取文件相对程序的绝对路径
int rl_get_file_abs_path(char *buf, unsigned int len,const char *file_path)
{
    if (file_path == NULL)
    {
        rl_log_error("[%s:%s:%d] file_path is null", __FILENAME__, __FUNCTION__, __LINE__);
        buf[0] = '\0';
        return RL_FAILED;
    }
    char temp_buf[512] = {0};
    if (rl_get_proc_dir(temp_buf, 512) == RL_FAILED)
    {
        rl_log_error("[%s:%s:%d] get proc dir failed", __FILENAME__, __FUNCTION__, __LINE__);
        buf[0] = '\0';
        return RL_FAILED;
    }
    snprintf(buf, len, "%s/%s", temp_buf, file_path);
    return RL_SUCCESS;
}

// 获取进程使用的文件句柄数量
int rl_get_self_proc_fd_count()
{
    char path[64] = {0};
    // 获取当前进程的文件描述符目录路径
    snprintf(path, sizeof(path), SYS_PROC_FD_FILE_FORMAT, getpid());

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        rl_log_error("[%s:%s:%d] open file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, path);
        return RL_FAILED;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // 只计算文件项，不包括 "." 和 ".."
        if (entry->d_name[0] != '.')
        {
            count++;
        }
    }

    // 关闭目录
    if (closedir(dir) != 0)
    {
        rl_log_error("[%s:%s:%d] close file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, path);
        return RL_FAILED;
    }

    return count;
}

// 通过PID获取进程使用的文件句柄数量
int rl_get_proc_fd_count_by_pid(pid_t pid)
{
    if (pid <= 0)
    {
        rl_log_error("[%s:%s:%d] invalid pid: %d", __FILENAME__, __FUNCTION__, __LINE__, pid);
        return RL_FAILED;
    }

    char path[64] = {0};
    snprintf(path, sizeof(path), SYS_PROC_FD_FILE_FORMAT, pid);

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        rl_log_error("[%s:%s:%d] open dir:%s failed, errno=%d", __FILENAME__, __FUNCTION__, __LINE__, path, errno);
        return RL_FAILED;
    }

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        // 排除 . 和 ..
        if (entry->d_name[0] != '.')
        {
            count++;
        }
    }

    if (closedir(dir) != 0)
    {
        rl_log_error("[%s:%s:%d] close dir:%s failed, errno=%d", __FILENAME__, __FUNCTION__, __LINE__, path, errno);
        return RL_FAILED;
    }

    return count;
}

// 获取当前进程使用的运行内存情况（单位 KB）
int rl_get_self_proc_memory_usage()
{
    FILE *fp = fopen(SYS_PROC_SELF_STATUS_FILE, "r");
    if (fp == NULL)
    {
        rl_log_error("[%s:%s:%d] open file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_SELF_STATUS_FILE);
        return RL_FAILED;
    }

    char line[256];
    int memory_usage = -1;

    // 读取每一行并查找 VmRSS 字段
    while (fgets(line, sizeof(line), fp))
    {
        // 查找 VmRSS 字段，表示实际内存使用情况
        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            // 提取 VmRSS 后面的数字（单位是 KB）
            if (sscanf(line, "VmRSS: %d kB", &memory_usage) == 1)
            {
                break;
            }
        }
    }

    fclose(fp);

    if (memory_usage == -1)
    {
        rl_log_error("[%s:%s:%d] VmRSS not found in file:%s", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_SELF_STATUS_FILE);
        return RL_FAILED;
    }

    return memory_usage;
}

// 通过pid获取进程使用的运行内存情况（单位 KB）
int rl_get_proc_memory_usage_by_pid(pid_t pid)
{
    if (pid <= 0)
    {
        rl_log_error("[%s:%s:%d] invalid pid: %d", __FILENAME__, __FUNCTION__, __LINE__, pid);
        return RL_FAILED;
    }

    char path[128];
    snprintf(path, sizeof(path), SYS_PROC_STATUS_PATH_FORMAT, pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        rl_log_error("[%s:%s:%d] open file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, path);
        return RL_FAILED;
    }

    char line[256];
    int memory_usage = -1;

    // 读取每一行并查找 VmRSS 字段
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            if (sscanf(line, "VmRSS: %d kB", &memory_usage) == 1)
            {
                break;
            }
        }
    }

    fclose(fp);

    if (memory_usage == -1)
    {
        rl_log_error("[%s:%s:%d] VmRSS not found in file:%s", __FILENAME__, __FUNCTION__, __LINE__, path);
        return RL_FAILED;
    }

    return memory_usage;
}

// 通过进程名称获取pid
int rl_get_proc_pid_by_name(const char *proc_name)
{
    if (rl_str_isempty(proc_name) == RL_TRUE)
    {
        rl_log_error("[%s:%s:%d] proc_name is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    DIR *dir = opendir(SYS_PROC_DIR);
    if (dir == NULL)
    {
        rl_log_error("[%s:%s:%d] opendir failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // 只处理数字开头的目录
        if (!isdigit(entry->d_name[0]))
        {
            continue;
        }

        char path[256];
        snprintf(path, sizeof(path), SYS_PROC_NAME_FORMAT, entry->d_name);
        FILE *fp = fopen(path, "r");
        if (fp == NULL)
        {
            continue;
        }

        char name[256];
        if (fgets(name, sizeof(name), fp))
        {
            name[strcspn(name, "\n")] = '\0';
            if (strcmp(name, proc_name) == 0)
            {
                fclose(fp);
                closedir(dir);
                return atoi(entry->d_name);
            }
        }
        fclose(fp);
    }
    closedir(dir);

    return RL_FAILED;
}

// 获取系统的总运行内存
int rl_get_sys_memory_total()
{
    FILE *fp = fopen(SYS_PROC_MEMORY_INFO_FILE, "r");
    if (fp == NULL)
    {
        rl_log_error("[%s:%s:%d] open file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_MEMORY_INFO_FILE);
        return RL_FAILED;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "MemTotal:", 9) == 0)
        {
            unsigned int total = RL_INVALID;
            sscanf(line, "MemTotal: %u kB", &total);
            fclose(fp);
            return total;
        }
    }
    fclose(fp);

    rl_log_error("[%s:%s:%d] parse file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_MEMORY_INFO_FILE);
    return RL_FAILED;
}

// 获取系统可用运行内存
int rl_get_sys_memory_available()
{
    FILE *fp = fopen(SYS_PROC_MEMORY_INFO_FILE, "r");
    if (fp == NULL)
    {
        rl_log_error("[%s:%s:%d] open file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_MEMORY_INFO_FILE);
        return RL_FAILED;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "MemAvailable:", 13) == 0)
        {
            unsigned int available = RL_INVALID;
            sscanf(line, "MemAvailable: %u kB", &available);
            fclose(fp);
            return available;
        }
    }
    fclose(fp);

    rl_log_error("[%s:%s:%d] parse file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_MEMORY_INFO_FILE);
    return RL_FAILED;
}

// 获取根目录文件系统的总存储大小
int rl_get_sys_storage_total_mb()
{
    struct statvfs fs;

    if (statvfs(SYS_ROOT_PATH, &fs) != 0)
    {
        rl_log_error("[%s:%s:%d] statvfs failed for path:%s", __FILENAME__, __FUNCTION__, __LINE__, SYS_ROOT_PATH);
        return RL_FAILED;
    }

    u_int64_t total_bytes = (u_int64_t)fs.f_blocks * fs.f_frsize;
    unsigned long total_mb = total_bytes / (1024 * 1024);

    return (int)total_mb;
}

// 获取根目录文件系统的可用存储大小
int rl_get_sys_storage_available_mb()
{
    struct statvfs fs;

    if (statvfs(SYS_ROOT_PATH, &fs) != 0)
    {
        rl_log_error("[%s:%s:%d] statvfs failed for path:%s", __FILENAME__, __FUNCTION__, __LINE__, SYS_ROOT_PATH);
        return RL_FAILED;
    }

    u_int64_t avail_bytes = (u_int64_t)fs.f_bavail * fs.f_frsize;
    unsigned long avail_mb = avail_bytes / (1024 * 1024);

    return (int)avail_mb;
}

// 获取当前进程的名称
int rl_get_proc_name(char *name, unsigned int len)
{
    if (name == NULL || len == 0)
    {
        rl_log_error("[%s:%s:%d] invalid name buffer", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 只读打开文件以获取进程名称
    FILE *file = fopen(SYS_PROC_GET_NAME_FILE, "r");
    if (file == NULL)
    {
        rl_log_error("[%s:%s:%d] failed to open file:%s", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_GET_NAME_FILE);
        return RL_FAILED;
    }
    if (fgets(name, len, file) == NULL)
    {
        rl_log_error("[%s:%s:%d] fgets failed", __FILENAME__, __FUNCTION__, __LINE__);
        fclose(file);
        return RL_FAILED;
    }
    // 将换行符替换为字符串结束符 '\0'
    name[strcspn(name, "\n")] = '\0';
    fclose(file);
    return RL_SUCCESS;
}

// 计算代码运行时间
// struct timespec start;
// clock_gettime(CLOCK_MONOTONIC, &start);
double rl_calcuate_run_time_ms(const struct timespec *start)
{
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    time_t sec_diff = end.tv_sec - start->tv_sec;
    long nsec_diff = end.tv_nsec - start->tv_nsec;

    if (nsec_diff < 0)
    {
        sec_diff -= 1;
        nsec_diff += 1000000000;
    }

    return sec_diff * 1000.0 + nsec_diff / 1e6;
}