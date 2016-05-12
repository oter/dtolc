#include "dma_routine.h"

#include "dma_mem.h"

static uint32_t dma_errors_count = 0;
static uint32_t dma_bad_ints = 0;

void udma_error_handler(void)
{
    uint32_t status = uDMAErrorStatusGet();
    if(status)
    {
        uDMAErrorStatusClear();
        dma_errors_count++;
    }
}

void uDMAIntHandler(void)
{
    uint32_t mode = uDMAChannelModeGet(UDMA_CHANNEL_SW);
    if(mode == UDMA_MODE_STOP)
    {
    	on_process_dma_mem(mode);
    }
    else
    {
        dma_bad_ints++;
    }
}
