#ifndef _UART_IF_H_
#define _UART_IF_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define UART_DMA_RX_BLOCK_SIZE 32
#define UART_DMA_RX_BLOCKS_COUNT 512

extern void InitUartInterface(uint32_t sys_clock);






#endif /* _UART_IF_H_ */
