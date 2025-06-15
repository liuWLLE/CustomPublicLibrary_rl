#include "rlsys.h"
#include "rl/rlstr.h"
#include <libgen.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>

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

// 获取使用的文件句柄数量
int rl_get_fd_count()
{
    char path[64] = {0};
    // 获取当前进程的文件描述符目录路径
    snprintf(path, sizeof(path), "/proc/%d/fd", getpid()); 

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

// 获取使用的内存情况（单位是 KB）
int rl_get_memory_usage()
{
    FILE *fp = fopen(SYS_PROC_GET_STATUS_FILE, "r");
    if (fp == NULL)
    {
        rl_log_error("[%s:%s:%d] open file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_GET_STATUS_FILE);
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
        rl_log_error("[%s:%s:%d] VmRSS not found in file:%s", __FILENAME__, __FUNCTION__, __LINE__, SYS_PROC_GET_STATUS_FILE);
        return RL_FAILED;
    }

    return memory_usage;
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