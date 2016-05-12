#include "dma_mem.h"

#define DMA_MEM_QUEUE_SIZE 8

mem_cpy_descr descrs[DMA_MEM_QUEUE_SIZE];
uint8_t write_index = 0;
uint8_t read_index = 0;

enum state_e
{
	STATE_IDLE,

};

state_e dma_mem_state = STATE_IDLE;
uint16_t dma_mem_index = 0;
uint16_t dma_mem_left = 0;

void on_process_dma_mem(uint32_t mode)
{
	if (dma_mem_state == STATE_IDLE || mode == UDMA_MODE_STOP)
	{

	}
}



void dma_mem_cpy(mem_cpy_descr descr)
{


	++write_index;
	if (write_index == DMA_MEM_QUEUE_SIZE)
	{
		write_index = 0;
	}

	if (write_index == read_index)
	{
		// FIXME: Data overrun
		while(true);
	}
}
