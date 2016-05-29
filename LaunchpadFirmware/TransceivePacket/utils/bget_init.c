#include "utils/bget_init.h"

#include <stdbool.h>

#include "utils/bget.h"

#define BGET_POOL_SIZE 32768


void InitBGET(void)
{
	void* mem_buf = malloc(BGET_POOL_SIZE);
	if (!mem_buf)
	{
		// Insufficient memory available to allocate, so we can't move forward
		while(true) {}
	}

	bpool(mem_buf, BGET_POOL_SIZE);
}
