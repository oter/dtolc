#ifndef _DMA_MEM_H_
#define _DMA_MEM_H_

#include "err.h"
#include "laser_packet.h"

typedef void (*dma_mem_cb_t)();

// We suppose memory transfers from the ring buffer into the packed_data
typedef struct
{
	uint8_t* ring_start;
	uint16_t ring_size;
	uint8_t* from_start;
	uint16_t from_size;
	packet_data* to;
	dma_mem_cb_t on_done;
} mem_cpy_descr;

extern void on_process_dma_mem(uint32_t mode);
extern void dma_mem_cpy(mem_cpy_descr descr);

#endif // _DMA_MEM_H_
