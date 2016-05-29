#ifndef _LASER_PACKET_H_
#define _LASER_PACKET_H_


#include <stdint.h>

#include "utils/bget.h"


// Align have to be power of two
#define LASER_PACKET_ALIGN 32
#define PACKET_DATA_BLOCK_SIZE 512

struct _packet_data
{
	uint8_t* data;
	uint16_t size;
	uint16_t total_size;
	struct _packet_data* next;
	uint8_t refs_count;
};

typedef struct _packet_data packet_data;

typedef struct
{
	uint32_t type;
	uint16_t data_size;
	packet_data* data;
	uint32_t crc_checksum;
} laser_packet;

// Allocate packet data buffer for the data storage
extern packet_data* pd_alloc(uint16_t size, uint16_t block_size);

// Concatenate pd2 into the end of the pd1
extern void pd_cat(packet_data* pd1, packet_data* pd2);

extern uint16_t pd_clen(packet_data* pd);

// Increase referenses count
extern void pd_use(packet_data* pd);

// Return the space allocated for the buffer back to pool
extern void pd_free(packet_data* pd);

// Allocate laser packet
extern laser_packet* lp_alloc(packet_data* data);

// Increase referenses count
extern void lp_use(laser_packet* lp);

// Free allocated laser packet
extern void lp_free(laser_packet* lp);


#endif // _LASER_PACKET_H_
