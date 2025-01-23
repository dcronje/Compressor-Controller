#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

// Enable operating system (FreeRTOS support)
#define NO_SYS 0
#define LWIP_NETCONN 1
#define LWIP_SOCKET 1

// Memory and buffer configurations
#define MEM_LIBC_MALLOC 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE (32 * 1024)
#define MEMP_NUM_TCP_SEG 64
#define MEMP_NUM_ARP_QUEUE 10
#define PBUF_POOL_SIZE 32
#define PBUF_POOL_BUFSIZE 1024

// TCP/IP stack configurations
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_IPV4 1
#define LWIP_DNS 1

// TCP settings
#define LWIP_TCP_KEEPALIVE 1
#define TCP_WND (8 * TCP_MSS)
#define TCP_MSS 1460
#define TCP_SND_BUF (16 * TCP_MSS)
#define TCP_SND_QUEUELEN ((8 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define MEMP_NUM_TCP_SEG TCP_SND_QUEUELEN

// DHCP settings
#define LWIP_DHCP 1
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

// DHCP server support
#define LWIP_DHCP_SERVER 1
#define DHCPD_MAX_LEASES 4
#define DHCPD_MAX_LEASE_TIME 7200 // Lease time in seconds

// HTTP server (non-HTTPS)
#define LWIP_HTTPD 1
#define LWIP_HTTPD_DYNAMIC_HEADERS 1
#define LWIP_HTTPD_CGI 1
#define LWIP_HTTPD_SSI 1
#define HTTPD_MAX_REQ_LENGTH 2048
#define HTTPD_MAX_RETRIES 4
#define HTTPD_USE_CUSTOM_FSDATA 0

// mDNS settings (optional)
#define LWIP_MDNS_RESPONDER 1
#define LWIP_NUM_NETIF_CLIENT_DATA 1

// System settings
#define MEMP_NUM_SYS_TIMEOUT 10
#define LWIP_TCPIP_CORE_LOCKING 0
#define TCPIP_MBOX_SIZE 16
#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_THREAD_PRIO 3
#define LWIP_IGMP 1

#define LWIP_HTTPD_SUPPORT_POST 1
#define LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN 256 // Adjust as needed
#define LWIP_HTTPD_MAX_CGI_PARAMETERS 8          // Adjust for the number of parameters
#define LWIP_HTTPD_SUPPORT_11_KEEPALIVE 1

#define LWIP_HTTPD_SSI_INCLUDE_TAG 0 // Include the SSI tag in the output
#define LWIP_HTTPD_MAX_TAG_INSERT_LEN 2048

#define DEFAULT_ACCEPTMBOX_SIZE 8   // Number of messages in the accept mailbox
#define DEFAULT_RAW_RECVMBOX_SIZE 8 // Number of messages in the raw recv mailbox
#define DEFAULT_UDP_RECVMBOX_SIZE 8 // Number of messages in the UDP recv mailbox
#define DEFAULT_TCP_RECVMBOX_SIZE 8 // Number of messages in the TCP recv mailbox

// Debugging
#ifndef NDEBUG
#define LWIP_DEBUG 1
#define LWIP_STATS 1
#define LWIP_STATS_DISPLAY 1
#endif

// Debug settings for various modules
#define ETHARP_DEBUG LWIP_DBG_OFF
#define NETIF_DEBUG LWIP_DBG_OFF
#define PBUF_DEBUG LWIP_DBG_OFF
#define API_LIB_DEBUG LWIP_DBG_OFF
#define API_MSG_DEBUG LWIP_DBG_OFF
#define SOCKETS_DEBUG LWIP_DBG_OFF
#define ICMP_DEBUG LWIP_DBG_OFF
#define INET_DEBUG LWIP_DBG_OFF
#define IP_DEBUG LWIP_DBG_OFF
#define IP_REASS_DEBUG LWIP_DBG_OFF
#define RAW_DEBUG LWIP_DBG_OFF
#define MEM_DEBUG LWIP_DBG_OFF
#define MEMP_DEBUG LWIP_DBG_OFF
#define SYS_DEBUG LWIP_DBG_OFF
#define TCP_DEBUG LWIP_DBG_OFF
#define TCP_INPUT_DEBUG LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG LWIP_DBG_OFF
#define TCP_RTO_DEBUG LWIP_DBG_OFF
#define TCP_CWND_DEBUG LWIP_DBG_OFF
#define TCP_WND_DEBUG LWIP_DBG_OFF
#define TCP_FR_DEBUG LWIP_DBG_OFF
#define TCP_QLEN_DEBUG LWIP_DBG_OFF
#define TCP_RST_DEBUG LWIP_DBG_OFF
#define UDP_DEBUG LWIP_DBG_OFF
#define TCPIP_DEBUG LWIP_DBG_OFF
#define PPP_DEBUG LWIP_DBG_OFF
#define SLIP_DEBUG LWIP_DBG_OFF
#define DHCP_DEBUG LWIP_DBG_OFF

// Prevent redefinition of timeval
#define LWIP_TIMEVAL_PRIVATE 0

#endif /* __LWIPOPTS_H__ */
