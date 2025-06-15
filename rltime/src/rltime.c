#include "rltime.h"
#include "rl/rlstr.h"
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define __FILENAME__ "rltime"

// 设置时区（环境变量）
int rl_set_timezone(const char *timezone)
{
    if (rl_str_isempty(timezone) == RL_TRUE)
    {
        rl_log_error("[%s:%s:%d] timezone is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 设置时区环境变量
    if (setenv("TZ", timezone, 1) != 0)
    {
        rl_log_error("[%s:%s:%d] ste timezone:%s failed", __FILENAME__, __FUNCTION__, __LINE__, timezone);
        return RL_FAILED;
    }

    // 刷新时区信息
    tzset();
    return RL_SUCCESS;
}

// 获取当前时间
int rl_get_time(rl_time_t *time_result)
{
    time_t time_stamp;

    // 获取当前时间戳
    if (time(&time_stamp) == (time_t)-1)
    {
        rl_log_error("[%s:%s:%d] get time stamp failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 使用localtime_r转换本地时间（线程安全）
    if (localtime_r(&time_stamp, time_result) == NULL)
    {
        rl_log_error("[%s:%s:%d] localtime_r get time failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    return RL_SUCCESS;
}

// 获取当前时间和时间戳
time_t rl_get_time_stamp(rl_time_t *time_result)
{
    time_t time_stamp;

    // 获取当前时间戳
    if (time(&time_stamp) == (time_t)-1)
    {
        rl_log_error("[%s:%s:%d] get time stamp failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 使用localtime_r转换本地时间（线程安全）
    if (localtime_r(&time_stamp, time_result) == NULL)
    {
        rl_log_error("[%s:%s:%d] localtime_r get time failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    return time_stamp;
}

// 设置当前系统时间
int rl_update_sys_time(unsigned int year, unsigned int mon, unsigned int day, unsigned int hour, unsigned int min, unsigned int sec)
{
    // 限制年份范围
    if (year < 1970 || year > 2200)
    {
        rl_log_error("[%s:%s:%d] year=%d invalid", __FILENAME__, __FUNCTION__, __LINE__, year);
        return RL_FAILED;
    }

    // 检查月份范围
    if (mon < 1 || mon > 12)
    {
        rl_log_error("[%s:%s:%d] month=%d invalid", __FILENAME__, __FUNCTION__, __LINE__, mon);
        return RL_FAILED;
    }

    // 根据月份和闰年来检查日期
    unsigned int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
    {
        // 闰年2月29天
        days_in_month[1] = 29;
    }
    if (day < 1 || day > days_in_month[mon - 1])
    {
        rl_log_error("[%s:%s:%d] day=%d invalid", __FILENAME__, __FUNCTION__, __LINE__, mon);
        return RL_FAILED;
    }

    // 检查小时、分钟、秒的范围
    if (hour > 23 || min > 59 || sec > 59)
    {
        rl_log_error("[%s:%s:%d] hour=%d or min=%d or sec=%d invalid", __FILENAME__, __FUNCTION__, __LINE__, hour, min, sec);
        return RL_FAILED;
    }

    // 修改系统时间
    char buf[64];
    snprintf(buf, sizeof(buf), "sudo date -u -s \"%04d-%02d-%02d %02d:%02d:%02d\"", year, mon, day, hour, min, sec);
    if (system(buf) == -1) 
    {
        rl_log_error("[%s:%s:%d] sys set time:%s failed", __FILENAME__, __FUNCTION__, __LINE__, buf);
        return RL_FAILED;
    } 
    if (system("sudo hwclock -w") == -1) 
    {
        rl_log_error("[%s:%s:%d] sys set sync rtc failed", __FILENAME__, __FUNCTION__, __LINE__, buf);
        return RL_FAILED;
    } 
    return RL_SUCCESS;
}

// 通过服务器获取时间
int rl_get_time_from_server(unsigned int try_sec, rl_time_t *time)
{
    // 创建 socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        rl_log_error("[%s:%s:%d] init socket failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 设置 socket 超时时间
    struct timeval timeout;
    timeout.tv_sec = try_sec;
    timeout.tv_usec = 0;
    // 设置接收超时
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] setsockopt set recv timeout failed", __FILE__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 设置发送超时
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] setsockopt set send timeout failed", __FILE__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 解析域名
    struct addrinfo hints, *res = NULL;
    rl_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(GET_TIME_SOCKET_DNS, NULL, &hints, &res) != 0 || res == NULL)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] can't get host name:%s", __FILENAME__, __func__, __LINE__, GET_TIME_SOCKET_DNS);
        return RL_FAILED;
    }

    // 设置目标地址端口
    struct sockaddr_in server_addr = *(struct sockaddr_in *)res->ai_addr;
    server_addr.sin_port = htons(GET_TIME_SOCKET_PORT);
    freeaddrinfo(res);
    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] try connect:%sfailed", __FILENAME__, __FUNCTION__, __LINE__, GET_TIME_SOCKET_DNS);
        return RL_FALSE;
    }

    // 构建 HTTP GET 请求
    char request[512];
    snprintf(request, sizeof(request),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n",
             GET_TIME_SOCKET_DNS);

    // 发送请求
    if (send(sockfd, request, rl_strlen(request), 0) == -1)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] send request connect failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FALSE;
    }

    // 接收响应（循环读取直到完整头）
    char response[2048] = {0};
    int total_received = 0;
    int ret;
    while ((ret = recv(sockfd, response + total_received, sizeof(response) - total_received - 1, 0)) > 0)
    {
        total_received += ret;
        // recv 数据时防止越界；
        if (total_received >= (2048 - 1))
        {
            break;
        }
        response[total_received] = '\0';
        // 读取完整头部
        if (strstr(response, "\r\n\r\n"))
        {
            break;
        }
    }

    if (ret <= 0)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] receive failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FALSE;
    }

    // 校验 HTTP 状态码
    if (strncmp(response, "HTTP/", 5) != 0 || strstr(response, "200 OK") == NULL)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] HTTP not OK", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FALSE;
    }
    // 输出 HTTP 响应头
    rl_log_debug("[%s:%s:%d] HTTP Response:\n%s", __FILENAME__, __FUNCTION__, __LINE__, response);

    // 解析 Date 字段（从响应头中提取时间）
    char *date_header = strstr(response, "Date: ");
    if (date_header)
    {
        char *end = strstr(date_header, "\r\n");
        if (end)
        {
            *end = '\0';
            const char *date_str = date_header + 6;
            rl_log_debug("[%s:%s:%d] rece date: %s", __FILENAME__, __FUNCTION__, __LINE__, date_str);
            // 转换时间格式
            rl_memset(time, 0, sizeof(rl_time_t));
            if (strptime(date_str, "%a, %d %b %Y %H:%M:%S GMT", time) == NULL)
            {
                rl_log_error("[%s:%s:%d] strptime parse failed", __FILENAME__, __FUNCTION__, __LINE__);
                close(sockfd);
                return RL_FAILED;
            }
        }
        else
        {
            rl_log_debug("[%s:%s:%d] date header malformed", __FILENAME__, __FUNCTION__, __LINE__);
            close(sockfd);
            return RL_FAILED;
        }
    }
    else
    {
        rl_log_debug("[%s:%s:%d] date header not found", __FILENAME__, __FUNCTION__, __LINE__);
        close(sockfd);
        return RL_FAILED;
    }

    close(sockfd);

    return RL_SUCCESS;
}
