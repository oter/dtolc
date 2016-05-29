#include "laser_packet.h"

#include <stdbool.h>
#include <stdio.h>

static uint16_t align_size(uint16_t size)
{
	if (size & (LASER_PACKET_ALIGN - 1))
	{
		return (size & ~((uint16_t)LASER_PACKET_ALIGN - 1)) | (uint16_t)LASER_PACKET_ALIGN;
	}
	return size;
}

static void memory_fault()
{
	while(true);
}

packet_data* data_block_alloc(uint16_t size)
{
	if (size == 0)
	{
		return NULL;
	}

	packet_data* block = bget(align_size(sizeof(packet_data) + size));
	if (block == NULL)
	{
		return NULL;
	}

	block->data = (uint8_t*)block + sizeof(packet_data);
	block->next = NULL;
	block->size = size;
	block->total_size = size;

	return block;
}

packet_data* pd_alloc(uint16_t size, uint16_t block_size)
{
	packet_data* pd = NULL;
	packet_data* next = NULL;

	while(size)
	{
		uint16_t alloc_size = size > block_size ? block_size : size;
		if (next == NULL)
		{
			pd = bget(align_size(sizeof(packet_data) + alloc_size));
			next = pd;
		} else
		{
			next->next = bget(align_size(sizeof(packet_data) + alloc_size));
			next = next->next;
		}
		if (next == NULL)
		{
			pd_free(pd);
			return NULL;
		}
		pd->data = (uint8_t*)pd + sizeof(packet_data);
		pd->next = NULL;
		pd->size = alloc_size;
		pd->total_size = size;
		pd->refs_count = 1;
		size -= alloc_size;
	}

	return pd;
}

void pd_use(packet_data* pd)
{
	while(pd != NULL)
	{
		++pd->refs_count;
		if (pd->refs_count == 255)
		{
			memory_fault();
		}
		pd = pd->next;
	}
}

void pd_cat(packet_data* pd1, packet_data* pd2)
{
	if (pd1 == NULL || pd2 == NULL)
	{
		return;
	}
	pd1->total_size += pd2->total_size;
	while(pd1->next != NULL)
	{
		pd1 = pd1->next;
		pd1->total_size += pd2->total_size;
	}

	pd1->next = pd2;
}

uint16_t pd_clen(packet_data* pd)
{
	uint16_t len = 0;

	while(pd != NULL)
	{
		pd = pd->next;
		++len;
	}

	return len;
}

void pd_free(packet_data* pd)
{
	while(pd != NULL)
	{
		--pd->refs_count;
		packet_data* next = pd->next;
		if (pd->refs_count == 0)
		{
			brel(pd);
		}
		pd = next;
	}
}


laser_packet* lp_alloc(packet_data* data)
{
	if (data == NULL)
	{
		return NULL;
	}

	uint16_t aligned_size = align_size(sizeof(laser_packet));
	laser_packet* lp = bgetz(aligned_size);

	if (lp == NULL)
	{
		return NULL;
	}

	lp->data = data;
	lp->data_size = data->total_size;

	return lp;
}

void lp_use(laser_packet* lp)
{
	if (lp == NULL)
	{
		return;
	}

	pd_use(lp->data);
}

void lp_free(laser_packet* lp)
{
	if (lp == NULL)
	{
		return;
	}

	pd_free(lp->data);
	brel(lp);
}
