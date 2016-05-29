#ifndef _UART_IF_H_
#define _UART_IF_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "lwip/opt.h"
#include "lwip/ip.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/ip_frag.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/icmp.h"
#include "lwip/igmp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp_impl.h"
#include "lwip/snmp.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/stats.h"

extern void UARTIfInit(uint32_t sys_clock);
extern uint16_t GetUartTxFree();
extern err_t SendPacket(struct pbuf* packet);
extern err_t OnReceivePacket(struct pbuf* packet);

extern void SysTickIntHandler(void);
extern void UARTIntHandler3(void);



#endif /* _UART_IF_H_ */
