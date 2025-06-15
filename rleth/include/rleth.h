#ifndef RL_ETH_H
#define RL_ETH_H

#include "public.h"

#ifdef __cplusplus
extern "C"
{
#endif

// 网络配置文件
#define SYS_ETH_GET_NETWORK_FILE    "/etc/network/interfaces"
// 网关文件
#define SYS_ETH_GET_GATEWAY_FILE    "/proc/net/route"
// 指定网卡
#define SYS_USER_ETHERNET_CARD      "eth0"
// 测试链接的公网域名（TCP）（百度）
#define SYS_ETH_CONNETC_TCP_NAME    "www.baidu.com"
// 测试链接的公网ip（TCP）（百度）
#define SYS_ETH_CONNETC_TCP_IP      "220.181.111.232"
// 测试链接的端口（TCP）
#define SYS_ETH_CONNETC_TCP_PORT    80
// 测试链接的公网ip（UDP）（谷歌）
#define SYS_ETH_CONNETC_UDP_IP      "8.8.8.8"
// 测试链接的端口（UDP）
#define SYS_ETH_CONNETC_UDP_PORT    53

// 判断ip格式
bool rl_is_ipv4(const char *str);
// 获取以太网网络状态
int rl_get_ethernet_card_state(unsigned int timeout_ms, int retry_count);
// 测试是否链接外网（TCP）
int rl_get_ethernet_connect_tcp(unsigned int try_sec);
// 测试是否链接外网（UDP）
int rl_get_ethernet_connect_udp(unsigned int try_sec);
// 获取dhcp状态
int rl_get_dhcp();
// 获取 IP 地址
int rl_get_ip_address(char *buf, unsigned int len);
// 获取子网掩码
int rl_get_subnet_mask(char *buf, unsigned int len);
// 获取网关
int rl_get_gateway(char *buf, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif