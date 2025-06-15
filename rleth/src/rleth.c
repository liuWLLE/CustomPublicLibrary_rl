#include "rleth.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/route.h>
#include <netdb.h>
#include "rl/rlstr.h"

#define __FILENAME__ "rleth"

bool rl_is_ipv4(const char *str)
{
    if (rl_str_isempty(str) == RL_TRUE)
    {
        return RL_FALSE;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, str, &addr) == RL_TRUE)
    {
        return RL_TRUE;
    }
    return RL_FALSE;
}

// 获取以太网网卡状态
int rl_get_ethernet_card_state(unsigned int timeout_ms, int retry_count)
{
    if (retry_count < 0)
    {
        rl_log_error("[%s:%s:%d] retry_count invalid", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }
    // 多次尝试
    while (--retry_count >= 0)
    {
        // 初始化socket
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            rl_log_error("[%s:%s:%d] init socket failed, attempt=%d", __FILENAME__, __FUNCTION__, __LINE__, retry_count);
            usleep(timeout_ms * 1000);
            continue;
        }
        // 初始化ifreq
        struct ifreq ifr;
        rl_memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, SYS_USER_ETHERNET_CARD, IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
        // 调用 ioctl 查询接口的状态标志
        if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1)
        {
            rl_log_error("[%s:%s:%d] ioctl get flags failed, attempt=%d", __FILENAME__, __FUNCTION__, __LINE__, retry_count);
        }
        else
        {
            // IFF_UP：网卡启用；IFF_RUNNING：连接上了网线（物理连接存在）
            if ((ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING))
            {
                close(sockfd);
                return RL_SUCCESS;
            }
            else
            {
                rl_log_debug("[%s:%s:%d] get ethernet card state failed, attempt=%d", __FILENAME__, __FUNCTION__, __LINE__, retry_count);
            }
        }
        close(sockfd);
        // 间隔时长
        usleep(timeout_ms * 1000);
    }
    rl_log_error("[%s:%s:%d] multiple get ethernet card state failed", __FILENAME__, __FUNCTION__, __LINE__);

    return RL_FAILED;
}

// 测试是否链接外网（TCP）
int rl_get_ethernet_connect_tcp(unsigned int try_sec)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        rl_log_error("[%s:%s:%d] init socket failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 设置 socket 发送超时时间（用于 connect 阻塞超时控制）
    struct timeval timeout;
    timeout.tv_sec = try_sec;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] setsockopt send timeout failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 使用 gethostbyname() 解析域名
    struct hostent *host = gethostbyname(SYS_ETH_CONNETC_TCP_NAME);
    if (host == NULL)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] gethostbyname:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_TCP_NAME);
        return RL_FAILED;
    }

    // 设置连接目标端口和地址
    struct sockaddr_in server;
    rl_memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SYS_ETH_CONNETC_TCP_PORT);
    // 使用解析到的 IP 地址
    memcpy(&server.sin_addr, host->h_addr_list, host->h_length);

    // 阻塞 connect（会被 SO_SNDTIMEO 超时控制）
    int ret = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
    if (ret == 0)
    {
        close(sockfd);
        return RL_SUCCESS;
    }

    close(sockfd);
    rl_log_error("[%s:%s:%d] dns:%s connect failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_TCP_NAME);
    return RL_FAILED;
}

// 测试是否链接外网（TCP）
int rl_get_ethernet_connect_tcp_sel(unsigned int try_sec)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        rl_log_error("[%s:%s:%d] init socket failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 设置socket非阻塞
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] set nonblock failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 使用 gethostbyname() 解析域名
    struct hostent *host = gethostbyname(SYS_ETH_CONNETC_TCP_NAME);
    if (host == NULL)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] gethostbyname:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_TCP_NAME);
        return RL_FAILED;
    }

    // 设置连接目标端口和地址
    struct sockaddr_in server;
    rl_memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SYS_ETH_CONNETC_TCP_PORT);
    // 使用解析到的 IP 地址
    memcpy(&server.sin_addr, host->h_addr_list, host->h_length);

    // 非阻塞连接
    // 如果 connect() 立即返回 0，说明连接成功（极少见，除非目标在本地或 LAN 中）
    // 如果返回 -1 且 errno == EINPROGRESS，说明连接正在进行中（正常的非阻塞行为）
    // 如果 errno 是其他值，说明连接失败，直接返回 false。
    int ret = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
    if (ret < 0 && errno != EINPROGRESS)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] try connect:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_TCP_NAME);
        return RL_FAILED;
    }

    // 使用 select() 等待连接完成或超时
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    struct timeval timeout;
    timeout.tv_sec = try_sec;
    timeout.tv_usec = 0;

    // select() 用来等待 socket 写入状态（表示连接成功或失败）
    ret = select(sockfd + 1, NULL, &fdset, NULL, &timeout);
    if (ret == 1)
    {
        int error;
        socklen_t len = sizeof(error);
        // 使用 getsockopt() 查看 socket 最终状态（如果 error == 0，说明连接成功，返回 true）
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0)
        {
            close(sockfd);
            return RL_SUCCESS;
        }
    }

    close(sockfd);
    rl_log_error("[%s:%s:%d] try connect:%s failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_TCP_NAME);
    return RL_FAILED;
}

// 测试是否链接外网（UDP）
int rl_get_ethernet_connect_udp(unsigned int try_sec)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        rl_log_error("[%s:%s:%d] create udp socket failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 设置超时（用于 sendto 阻塞超时控制）
    struct timeval tv;
    tv.tv_sec = try_sec;
    tv.tv_usec = 0;
    // 发送超时
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    // 接收超时
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server;
    rl_memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SYS_ETH_CONNETC_UDP_PORT);
    if (inet_pton(AF_INET, SYS_ETH_CONNETC_UDP_IP, &server.sin_addr) != 1)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] invalid ip: %s", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_UDP_IP);
        return RL_FAILED;
    }

    unsigned char buf[512] = {0};
    // 这个是合法的 A 类型查询 www.google.com 的 DNS 报文（共33字节）
    unsigned char dns_query[] = {
        0x12, 0x34, 0x01, 0x00, // Transaction ID + 标志
        0x00, 0x01, 0x00, 0x00, // 问题数目 + 应答数
        0x00, 0x00, 0x00, 0x00, // 授权数 + 附加数
        0x03, 'w', 'w', 'w',
        0x06, 'g', 'o', 'o', 'g', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,       // 结尾
        0x00, 0x01, // 类型 A
        0x00, 0x01  // 类 IN
    };
    memcpy(buf, dns_query, sizeof(dns_query));

    // 发送
    int ret = sendto(sockfd, buf, sizeof(dns_query), 0, (struct sockaddr *)&server, sizeof(server));
    if (ret < 0)
    {
        close(sockfd);
        rl_log_error("[%s:%s:%d] udp sendto failed: ip=%s port=%d", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_UDP_IP, SYS_ETH_CONNETC_UDP_PORT);
        return RL_FAILED;
    }

    // 等待对方回包
    char recv_buf[64];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ret = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from, &fromlen);
    if (ret >= 0)
    {
        close(sockfd);
        return RL_SUCCESS; // 成功接收到回应，连通正常
    }

    // 若无回应（超时或失败）
    close(sockfd);
    rl_log_error("[%s:%s:%d] udp recvfrom ip:%s port=%d failed", __FILENAME__, __FUNCTION__, __LINE__, SYS_ETH_CONNETC_UDP_IP, SYS_ETH_CONNETC_UDP_PORT);
    return RL_FAILED;
}

// 获取dhcp状态
int rl_get_dhcp()
{
    FILE *fp = fopen(SYS_ETH_GET_NETWORK_FILE, "r");
    if (!fp)
    {
        rl_log_error("[%s:%s:%d] open file to get dhcp failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    char line[256];
    // 遍历查找
    while (fgets(line, sizeof(line), fp))
    {
        // 去掉开头的空格
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t')
        {
            trimmed++;
        }
        // 检查是否是接口段落开头
        if (strncmp(trimmed, "iface", 5) == 0)
        {
            // 解析接口名和模式
            char iface[32], inet[32], mode[32];
            if (sscanf(trimmed, "iface %31s %31s %31s", iface, inet, mode) == 3)
            {
                if (rl_strcmp(iface, SYS_USER_ETHERNET_CARD) == 0)
                {
                    if (rl_strcmp(mode, "dhcp") == 0)
                    {
                        fclose(fp);
                        return RL_TRUE;
                    }
                    else if (rl_strcmp(mode, "static") == 0)
                    {
                        fclose(fp);
                        return RL_FALSE;
                    }
                }
            }
        }
    }

    fclose(fp);
    rl_log_error("[%s:%s:%d] file can't find dhcp", __FILENAME__, __func__, __LINE__);
    return RL_FAILED;
}

// 获取 IP 地址
int rl_get_ip_address(char *buf, unsigned int len)
{
    if (buf == NULL || len == 0)
    {
        rl_log_error("[%s:%s:%d] buf is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 创建一个 UDP socket（不用于传输数据，只是为了能调用 ioctl）
    // AF_INET 表示 IPv4，SOCK_DGRAM 表示 UDP 类型
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        rl_log_error("[%s:%s:%d] init socket failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 创建一个用于网络配置查询的结构体 ifreq，用于 ioctl 系统调用
    struct ifreq ifr;
    rl_memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, SYS_USER_ETHERNET_CARD, IFNAMSIZ - 1);
    // 保证 null 结尾
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // SIOCGIFADDR 命令表示“获取 IP 地址”
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == -1)
    {
        buf[0] = '\0';
        close(sockfd);
        rl_log_error("[%s:%s:%d] get ip by ioctl failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    // 转换为 sockaddr_in（IPv4）来读取 IP 地址
    struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    if (inet_ntop(AF_INET, &ipaddr->sin_addr, buf, len) == NULL)
    {
        buf[0] = '\0';
        close(sockfd);
        rl_log_error("[%s:%s:%d] inet_ntop trans ip failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    close(sockfd);
    return RL_SUCCESS;
}

// 获取子网掩码
int rl_get_subnet_mask(char *buf, unsigned int len)
{
    if (buf == NULL || len == 0)
    {
        rl_log_error("[%s:%s:%d] buf is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        rl_log_error("[%s:%s:%d] init socket failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    struct ifreq ifr;
    rl_memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, SYS_USER_ETHERNET_CARD, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // SIOCGIFNETMASK 命令表示“获取 子网掩码”
    if (ioctl(sockfd, SIOCGIFNETMASK, &ifr) == -1)
    {
        buf[0] = '\0';
        close(sockfd);
        rl_log_error("[%s:%s:%d] get subnet mask by ioctl failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    struct sockaddr_in *mask = (struct sockaddr_in *)&ifr.ifr_netmask;
    if (inet_ntop(AF_INET, &mask->sin_addr, buf, len) == NULL)
    {
        buf[0] = '\0';
        close(sockfd);
        rl_log_error("[%s:%s:%d] inet_ntop trans subnet mask failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    close(sockfd);
    return RL_SUCCESS;
}

// 获取网关
int rl_get_gateway(char *buf, unsigned int len)
{
    if (buf == NULL || len == 0)
    {
        rl_log_error("[%s:%s:%d] buf is null", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    FILE *fp = fopen(SYS_ETH_GET_GATEWAY_FILE, "r");
    if (!fp)
    {
        rl_log_error("[%s:%s:%d] open file to get gateway failed", __FILENAME__, __FUNCTION__, __LINE__);
        return RL_FAILED;
    }

    char line[256];
    // 跳过第一行表头
    fgets(line, sizeof(line), fp);
    // 遍历每一行，提取信息
    while (fgets(line, sizeof(line), fp))
    {
        char iface[IFNAMSIZ];
        unsigned long dest, gw, flags;
        if (sscanf(line, "%s\t%lx\t%lx\t%lx", iface, &dest, &gw, &flags) >= 4)
        {
            // 判断是否为默认网关（默认网关的 Destination 是 0、 0x2 = RTF_GATEWAY）
            if (dest == 0 && (flags & 0x2))
            {
                // 如果不是需要的网卡则跳过
                if (rl_strcmp(iface, SYS_USER_ETHERNET_CARD) != 0)
                {
                    continue;
                }
                // 转换网关地址为点分十进制字符串
                struct in_addr gw_addr;
                gw_addr.s_addr = gw;
                // 转换格式
                if (inet_ntop(AF_INET, &gw_addr, buf, len) == NULL)
                {
                    rl_log_error("[%s:%s:%d] inet_ntop convert failed", __FILENAME__, __func__, __LINE__);
                    fclose(fp);
                    return RL_FAILED;
                }
                fclose(fp);
                return RL_SUCCESS;
            }
        }
    }
    fclose(fp);
    rl_log_error("[%s:%s:%d] file can't find gateway", __FILENAME__, __func__, __LINE__);
    return RL_FAILED;
}

