#ifndef RL_STR_H
#define RL_STR_H

#include "public.h"

#ifdef __cplusplus
extern "C"
{
#endif

void *rl_memset(void *dst, int c, size_t size);
void *rl_memcpy(void *dst, const void *src, size_t size);

bool rl_str_isempty(const char *buf);
int rl_strlen(const char *src);
int rl_strcpy_s(char *dst, const size_t dst_len, const char *src);
int rl_strcmp(const char *s1, const char *s2);
long int rl_str_strtol(const char *str, int base);
int rl_str_isdigit(const char *buf);
int rl_str_atoi(const char *str);

int rl_memcmp(const void *ptr1, const void *ptr2, size_t size);

// 删除字符串的换行符
int rl_str_rm_line(char *buf);

// 在字符串 str 的第 pos 位置插入字符
int rl_str_insert_ch(char *str, size_t pos, char ch, size_t max);

// 删除字符串 str 的第 pos 位置的字符
int rl_delete_char(char *str, size_t pos);

// 删除字符串中某段字符串
int rl_remove_substr(char *str, const char *target);

// 替换字符串中某段字符串
int rl_replace_substr(char *str, const char *target, const char *replace, size_t len);

// 把字符串中的某段替换为某个字符
int rl_replace_char(char *str, const char *target, char replace, size_t len);

// 获取字符串中指定字符第一次出现后面的内容
int rl_get_str_after_char(const char *src, char ch, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
