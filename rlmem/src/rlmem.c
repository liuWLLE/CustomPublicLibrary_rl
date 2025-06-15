#include "rlmem.h"
#include "rl/rlsys.h"
#include "rl/rlstr.h"
#include "rl/rltime.h"

#define __FILENAME__ "rlmem"

// 是否开启内存分配跟踪
static bool rl_memory_trace_state = RL_FALSE;

// 内存分配跟踪
static pthread_mutex_t mem_lock;
typedef struct MEM_NODE
{
    void *ptr;
    unsigned int size;
    const char *file;
    const char *func;
    int line;
    struct MEM_NODE *next;
} MEM_NODE_T;
static MEM_NODE_T *g_mem_header;
static unsigned int g_alloc_block_count = 0;
static unsigned int g_alloc_total_bytes = 0;

// 仅程序初始化时调用
void rl_memory_trace_init(bool state)
{
    if (rl_memory_trace_state == RL_TRUE)
    {
        rl_log_debug("[%s:%s:%d] alread enable memory trace", __FILENAME__, __FUNCTION__, __LINE__);
        return;
    }

    if (state == RL_TRUE)
    {
        pthread_mutex_init(&mem_lock, NULL);
        g_mem_header = NULL;
        rl_memory_trace_state = RL_TRUE;
        rl_log_debug("[%s:%s:%d] enable memory trace", __FILENAME__, __FUNCTION__, __LINE__);
    }
    else
    {
        pthread_mutex_destroy(&mem_lock);
        rl_memory_trace_state = RL_FALSE;
        rl_log_debug("[%s:%s:%d] disable memory trace", __FILENAME__, __FUNCTION__, __LINE__);
    }
}

// 仅程序结束时调用（输出内存泄露情况到文件）
int rl_memory_trace_deinit()
{
    if (rl_memory_trace_state == RL_FALSE)
    {
        rl_log_debug("[%s:%s:%d] not enabled memory trace", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 获取日志路径
    char path[1024] = {0};
    int ret = rl_get_file_abs_path(path, 1024, RL_MEM_TRACE_LOG_FILE_DIR);
    if (ret == RL_FAILED || rl_str_isempty(path) == RL_TRUE)
    {
        rl_log_error("[%s:%s:%d] get log path failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    char name[32] = {0};
    if (rl_get_proc_name(name, sizeof(name)) == RL_FAILED)
    {
        rl_log_error("[%s:%s:%d] get proc name failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 追加进程名和 .log，确保不会越界
    unsigned int used = strlen(path);
    int remain = sizeof(path) - used;
    if (remain > 0)
    {
        snprintf(path + used, remain, "_%s.log", name);
    }
    else
    {
        rl_log_error("[%s:%s:%d] log file path too long", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 获取时间
    rl_time_t time;
    if (rl_get_time(&time) == RL_FAILED)
    {
        rl_log_error("[%s:%s:%d] get time failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    pthread_mutex_lock(&mem_lock);
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
    {
        pthread_mutex_unlock(&mem_lock);
        rl_log_debug("[%s:%s:%d] open memory trace file:%s failed", __FILENAME__, __FUNCTION__, __LINE__, path);
        return RL_FAILED;
    }

    fprintf(fp, "==== Memory Leak Report ====\n");
    fprintf(fp, "%04d-%02d-%02d %02d:%02d:%02d\n", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
    rl_log_debug("[%s:%s:%d] ==== Memory Leak Report ====", __FILENAME__, __FUNCTION__, __LINE__);
    unsigned int leak_count = 0;
    unsigned int leak_bytes = 0;

    // 如果没有内存泄露
    if (g_mem_header == NULL)
    {
        fprintf(fp, "No leaks detected\n");
        rl_log_debug("[%s:%s:%d] No leaks detected", __FILENAME__, __FUNCTION__, __LINE__);
    }
    // 如果发生内存泄露
    else
    {
        MEM_NODE_T *node = g_mem_header;
        while (node != NULL)
        {
            fprintf(fp, "[LEAK] %p (%d bytes) from %s:%s:%d\n", node->ptr, node->size, node->file, node->func, node->line);
            rl_log_debug("[%s:%s:%d] [LEAK] %p (%d bytes) from %s:%s:%d", __FILENAME__, __FUNCTION__, __LINE__, node->ptr, node->size, node->file, node->func, node->line);
            leak_count++;
            leak_bytes += node->size;
            // 释放记录的节点
            MEM_NODE_T *temp = node;
            node = node->next;
            free(temp);
        }
        g_mem_header = NULL;
        fprintf(fp, "Total leaks: %d blocks, %d bytes\n", leak_count, leak_bytes);
        rl_log_debug("[%s:%s:%d] Total leaks: %d blocks, %d bytes", __FILENAME__, __FUNCTION__, __LINE__, leak_count, leak_bytes);
    }

    fprintf(fp, "============================\n");
    rl_log_debug("[%s:%s:%d] ============================", __FILENAME__, __FUNCTION__, __LINE__);
    fclose(fp);
    pthread_mutex_unlock(&mem_lock);
    pthread_mutex_destroy(&mem_lock);

    rl_log_debug("[%s:%s:%d] memory trace complete, Leaks: %zu blocks, %zu bytes",__FILENAME__, __FUNCTION__, __LINE__, leak_count, leak_bytes);

    rl_memory_trace_state = RL_FALSE;
    return RL_SUCCESS;
}

void *rl_malloc(unsigned int size, const char *file, const char *func, int line)
{
    if (size == 0)
    {
        rl_log_error("[%s:%s:%d] size=%d invalid", __FILENAME__, __FUNCTION__, __LINE__, size);
        return NULL;
    }
    // 尝试分配内存
    void *ptr = malloc(size);
    if (ptr == NULL)
    {
        rl_log_error("[%s:%s:%d] malloc=%d bytes failed", __FILENAME__, __FUNCTION__, __LINE__, size);
        return NULL;
    }

    // 如果启用内存分配跟踪
    if (rl_memory_trace_state == RL_TRUE)
    {
        // 链表记录本次的内存分配
        MEM_NODE_T *node = (MEM_NODE_T *)malloc(sizeof(MEM_NODE_T));
        if (node == NULL)
        {
            free(ptr);
            rl_log_error("[%s:%s:%d] memory trace malloc=%d bytes failed", __FILENAME__, __FUNCTION__, __LINE__, sizeof(MEM_NODE_T));
            return NULL;
        }
        node->ptr = ptr;
        node->size = size;
        node->file = file;
        node->func = func;
        node->line = line;
        pthread_mutex_lock(&mem_lock);
        node->next = g_mem_header;
        g_mem_header = node;
        // 记录总分配内存
        g_alloc_block_count++;
        g_alloc_total_bytes += size;
        pthread_mutex_unlock(&mem_lock);
        rl_log_debug("[%s:%s:%d][malloc] addr=%p(%d bytes), total blocks=%d, total bytes=%d", file, func, line, ptr, size, g_alloc_block_count, g_alloc_total_bytes);
    }
    return ptr;
}

int rl_free(void *ptr, const char *file, const char *func, int line)
{
    if (ptr == NULL)
    {
        rl_log_error("[%s:%s:%d] free failed, ptr is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 如果启用内存分配跟踪
    if (rl_memory_trace_state == RL_TRUE)
    {
        // 链表记录释放内存
        pthread_mutex_lock(&mem_lock);
        bool found_node = RL_FALSE;
        MEM_NODE_T **cur = &g_mem_header;
        while (*cur != NULL)
        {
            if ((*cur)->ptr == ptr)
            {
                MEM_NODE_T *temp = *cur;
                *cur = temp->next;
                unsigned int temp_size = temp->size;
                // 释放链表节点
                free(temp);
                found_node = RL_TRUE;
                // 统计释放的内存
                g_alloc_block_count--;
                g_alloc_total_bytes -= temp_size;
                break;
            }
            cur = &((*cur)->next);
        }
        // 未找到
        if (found_node == RL_FALSE)
        {
            pthread_mutex_unlock(&mem_lock);
            rl_log_warn("[%s:%s:%d] ptr=%p not found in trace list, maybe double free or external malloc", file, func, line, ptr);
            return RL_FAILED;
        }
        pthread_mutex_unlock(&mem_lock);
        rl_log_debug("[%s:%s:%d][free] addr=%p, remain blocks=%d, remain bytes=%d", file, func, line, ptr, g_alloc_block_count, g_alloc_total_bytes);
    }
    // 释放实际的内存
    free(ptr);
    return RL_SUCCESS;
}

void *rl_calloc(unsigned int block, unsigned int count, const char *file, const char *func, int line)
{
    if (block == 0 || count == 0)
    {
        rl_log_error("[%s:%s:%d] block=%d or count=%d invalid", __FILENAME__, __FUNCTION__, __LINE__, block, count);
        return NULL;
    }
    // 分配内存
    unsigned int size = block * count;
    void *ptr = calloc(block, count);
    if (ptr == NULL)
    {
        rl_log_error("[%s:%s:%d] calloc=%d bytes failed", __FILENAME__, __FUNCTION__, __LINE__, size);
        return NULL;
    }

    // 如果启用内存分配跟踪
    if (rl_memory_trace_state == RL_TRUE)
    {
        MEM_NODE_T *node = (MEM_NODE_T *)malloc(sizeof(MEM_NODE_T));
        if (node == NULL)
        {
            free(ptr);
            rl_log_error("[%s:%s:%d] memory trace malloc=%d bytes failed", __FILENAME__, __FUNCTION__, __LINE__, sizeof(MEM_NODE_T));
            return NULL;
        }
        node->ptr = ptr;
        node->size = size;
        node->file = file;
        node->func = func;
        node->line = line;

        pthread_mutex_lock(&mem_lock);
        node->next = g_mem_header;
        g_mem_header = node;
        // 记录总分配内存
        g_alloc_block_count++;
        g_alloc_total_bytes += size;
        pthread_mutex_unlock(&mem_lock);
        rl_log_debug("[%s:%s:%d][calloc] addr=%p(%d bytes), total blocks=%d, total bytes=%d", file, func, line, ptr, size, g_alloc_block_count, g_alloc_total_bytes);
    }
    return ptr;
}

void *rl_realloc(void *ptr, unsigned int size, const char *file, const char *func, int line)
{
    if (size == 0)
    {
        rl_log_error("[%s:%s:%d] size=0, invalid realloc", __FILENAME__, __FUNCTION__, __LINE__);
        return NULL;
    }
    if (ptr == NULL)
    {
        return rl_malloc(size, file, func, line);
    }

    // 尝试分配内存
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL)
    {
        rl_log_error("[%s:%s:%d] realloc=%p bytes=%d failed", __FILENAME__, __FUNCTION__, __LINE__, ptr, size);
        return NULL;
    }
    // 如果启用内存分配跟踪
    if (rl_memory_trace_state == RL_TRUE)
    {
        pthread_mutex_lock(&mem_lock);
        bool found_node = RL_FALSE;
        MEM_NODE_T *cur = g_mem_header;
        while (cur != NULL)
        {
            // 需要先找到原来的 ptr 对应节点，并更新其地址和大小
            if (cur->ptr == ptr)
            {
                // 更新总字节数：先减去旧的，再加上新的
                unsigned int old_size = cur->size;
                g_alloc_total_bytes -= old_size;
                g_alloc_total_bytes += size;

                cur->ptr = new_ptr;
                cur->size = size;
                cur->file = file;
                cur->func = func;
                cur->line = line;
                found_node = RL_TRUE;
                break;
            }
            cur = cur->next;
        }
        pthread_mutex_unlock(&mem_lock);

        // 如果原来找不到 ptr（比如是从系统 malloc），就添加一条新记录
        if (found_node == RL_FALSE)
        {
            rl_log_warn("[%s:%s:%d] realloc original ptr=%p not found in trace list, new record created", file, func, line, ptr);
            MEM_NODE_T *node = (MEM_NODE_T *)malloc(sizeof(MEM_NODE_T));
            if (node == NULL)
            {
                free(new_ptr);
                rl_log_error("[%s:%s:%d] memory trace malloc %d bytes failed", __FILENAME__, __FUNCTION__, __LINE__, sizeof(MEM_NODE_T));
                return NULL;
            }
            node->ptr = new_ptr;
            node->size = size;
            node->file = file;
            node->func = func;
            node->line = line;
            pthread_mutex_lock(&mem_lock);
            node->next = g_mem_header;
            g_mem_header = node;
            // 新节点，增加统计
            g_alloc_block_count++;
            g_alloc_total_bytes += size;
            pthread_mutex_unlock(&mem_lock);
        }
        rl_log_debug("[%s:%s:%d][realloc] old=%p, new=%p, size=%d, total blocks=%d, total bytes=%d", file, func, line, ptr, new_ptr, size, g_alloc_block_count, g_alloc_total_bytes);
    }
    return new_ptr;
}
