#ifndef RL_TIME_H
#define RL_TIME_H

#include "public.h"
#include "ui/StrTrans.h"
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

// 获取时间的域名服务器
#define GET_TIME_SOCKET_DNS     "www.baidu.com"
// 获取时间的端口
#define GET_TIME_SOCKET_PORT    80

typedef struct tm rl_time_t;

// 星期
static const char *pszTransWeek[] = {STR_SUNDAY, STR_MONDAY, STR_TUESDAY, STR_WEDNESDAY, STR_THURSDAY, STR_FRIDAY, STR_SATURDAY};
// 月份
static const char *pszTransMonth[] = {STR_JANUARY, STR_FEBRUARY, STR_MARCH, STR_APRIL, STR_MAY, STR_JUNE, STR_JULY, STR_AUGUST, STR_SEPTEMBER, STR_OCTOBER, STR_NOVEMBER, STR_DECEMBER};

// 设置时区（环境变量）
int rl_set_timezone(const char *timezone);

// 获取当前时间
int rl_get_time(rl_time_t *time_result);

// 获取当前时间和时间戳
time_t rl_get_time_stamp(rl_time_t *time_result);

// 设置当前系统时间
int rl_update_sys_time(unsigned int year, unsigned int mon, unsigned int day, unsigned int hour, unsigned int min, unsigned int sec);

// 通过服务器获取时间
int rl_get_time_from_server(unsigned int try_sec, rl_time_t *time);

#ifdef __cplusplus
}
#endif

#endif
