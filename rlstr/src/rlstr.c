#include "rlstr.h"
#include <ctype.h>

void *rl_memset(void *dst, int c, size_t size)
{
    if (dst == NULL)
    {
        return NULL;
    }

    return memset(dst, c, size);
}

void *rl_memcpy(void *dst, const void *src, size_t size)
{
    if (dst == NULL || src == NULL)
    {
        return NULL;
    }
    return memcpy(dst, src, size);
}

bool rl_str_isempty(const char *buf)
{
    if (buf == NULL)
    {
        return RL_TRUE;
    }

    if (buf[0] == 0)
    {
        return RL_TRUE;
    }

    return RL_FALSE;
}

int rl_strlen(const char *src)
{
    if (rl_str_isempty(src) == RL_TRUE)
    {
        return 0;
    }
    return strlen(src);
}

int rl_strcpy_s(char *dst, const size_t dst_len, const char *src)
{
    if (dst == NULL || rl_str_isempty(src) == RL_TRUE)
    {
        return RL_FAILED;
    }

    size_t copy_size = strlen(src);
    if (copy_size > (dst_len - 1))
    {
        copy_size = dst_len - 1;
    }
    strncpy(dst, src, copy_size);
    *(dst + copy_size) = '\0';

    return RL_SUCCESS;
}

int rl_strcmp(const char *s1, const char *s2)
{
    if (rl_str_isempty(s1) == RL_TRUE || rl_str_isempty(s2) == RL_TRUE)
    {
        return RL_FAILED;
    }

    return strcmp(s1, s2);
}

int rl_str_isdigit(const char *buf)
{
    if (rl_str_isempty(buf) == RL_TRUE)
    {
        return RL_FAILED;
    }

    size_t i = 0;
    size_t len = rl_strlen(buf);
    if (buf[0] == '-')
    {
        if (len == 1)
        {
            return RL_FALSE;
        }
        i = 1;
    }

    for (; i < len; i++)
    {
        if (!isdigit((unsigned char)buf[i]))
        {
            return RL_FALSE;
        }
    }

    return RL_TRUE;
}

int rl_str_atoi(const char *str)
{
    if (rl_str_isempty(str) == RL_TRUE)
    {
        return RL_FAILED;
    }

    return atoi(str);
}

long int rl_str_strtol(const char *str, int base)
{
    if (rl_str_isempty(str) == RL_TRUE)
    {
        return RL_FAILED;
    }

    return strtol(str, NULL, base);
}

int rl_memcmp(const void *ptr1, const void *ptr2, size_t size)
{
    if (rl_str_isempty(ptr1) == RL_TRUE || rl_str_isempty(ptr2) == RL_TRUE)
    {
        return RL_FAILED;
    }
    if (size == 0)
    {
        return RL_FAILED;
    }
    return memcmp(ptr1, ptr2, size);
}

// 删除字符串的换行符
int rl_str_rm_line(char *buf)
{
    if (rl_str_isempty(buf) == RL_TRUE)
    {
        return RL_FAILED;
    }

    char *line = strchr(buf, '\n');
    if (line)
    {
        *line = '\0';
    }

    return RL_SUCCESS;
}

// 在字符串 str 的第 pos 位置插入字符
int rl_str_insert_ch(char *str, size_t pos, char ch, size_t max)
{
    if (str == NULL || max == 0)
    {
        return RL_FAILED;
    }
    size_t len = strlen(str);
    // 如果字符串已经满了（包含 '\0'，所以 max-1），不能再插入
    if (len >= max - 1)
    {
        return RL_FAILED;
    }
    // 如果 pos 超出最大允许范围，返回失败
    if (pos > len || pos >= max - 1)
    {
        return RL_FAILED;
    }

    // 从末尾向后移动字符，为插入字符腾出位置（包含 '\0'）
    memmove(&str[pos + 1], &str[pos], len - pos + 1);

    str[pos] = ch;
    return RL_SUCCESS;
}

// 删除字符串 str 的第 pos 位置的字符
int rl_delete_char(char *str, size_t pos)
{
    size_t len = strlen(str);
    if (pos >= len)
    {
        return RL_FAILED;
    }

    // 包括末尾 '\0'
    memmove(&str[pos], &str[pos + 1], len - pos);
    
    return RL_SUCCESS;
}

// 删除字符串中某段字符串
int rl_remove_substr(char *str, const char *target)
{
    if (rl_str_isempty(str) == RL_TRUE || rl_str_isempty(target) == RL_TRUE)
    {
        return RL_FAILED;
    }

    char *pos = strstr(str, target);
    if (pos == NULL)
    {
        return RL_FAILED;
    }

    // 将后续字符前移
    memmove(pos, pos + strlen(target), strlen(pos + strlen(target)) + 1);

    return RL_SUCCESS;
}

// 替换字符串中某段字符串
int rl_replace_substr(char *str, const char *target, const char *replace, size_t len)
{
    if (rl_str_isempty(str) == RL_TRUE || rl_str_isempty(target) == RL_TRUE || rl_str_isempty(replace) == RL_TRUE)
    {
        return RL_FAILED;
    }

    char *pos = strstr(str, target);
    if (pos == NULL)
    {
        return RL_FAILED;
    }

    size_t prefix_len = pos - str;
    size_t total_len = prefix_len + strlen(replace) + strlen(pos + strlen(target)) + 1;
    if (total_len > len)
    {
        return RL_FAILED;
    }

    char buffer[len];
    strcpy(buffer, str);                  // 备份原字符串
    buffer[prefix_len] = '\0';            // 截断
    strcat(buffer, replace);              // 添加替换内容
    strcat(buffer, pos + strlen(target)); // 添加剩余部分
    strcpy(str, buffer);                  // 拷贝回原始字符串

    return RL_SUCCESS;
}

// 把字符串中的某段替换为某个字符
int rl_replace_char(char *str, const char *target, char replace, size_t len)
{
    if (rl_str_isempty(str) == RL_TRUE || rl_str_isempty(target) == RL_TRUE)
    {
        return RL_FAILED;
    }

    char *pos = strstr(str, target);
    if (pos == NULL)
    {
        return RL_FAILED;
    }

    size_t prefix_len = pos - str;
    size_t remain_len = strlen(pos + strlen(target));
    if (prefix_len + 1 + remain_len + 1 > len)
    {
        return RL_FAILED;
    }

    char buffer[len];
    strncpy(buffer, str, prefix_len);     // 拷贝目标之前的部分
    buffer[prefix_len] = replace;         // 插入替代字符
    buffer[prefix_len + 1] = '\0';        // 临时终止
    strcat(buffer, pos + strlen(target)); // 拼接目标之后的部分
    strcpy(str, buffer);                  // 拷贝回原始字符串
    
    return RL_SUCCESS;
}


// 获取字符串中指定字符第一次出现后面的内容
int rl_get_str_after_char(const char *src, char ch, char *out, size_t out_size)
{
    if (rl_str_isempty(src) == RL_TRUE || out == NULL || out_size == 0)
    {
        return RL_FAILED;
    }

    const char *pos = strchr(src, ch);

    // 没有找到字符，或者字符后面没有内容
    if (pos == NULL || *(pos + 1) == '\0')
    {
        return RL_FAILED;
    }

    snprintf(out, out_size, "%s", pos + 1);

    return RL_SUCCESS;
}