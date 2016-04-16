#ifndef __ETH_H__
#define __ETH_H__

#include <stdint.h>
#include <stdbool.h>

extern void EthInit(uint32_t ui32SysClkHz, const uint8_t *pui8MAC, uint32_t ui32IPAddr,
        uint32_t ui32NetMask, uint32_t ui32GWAddr, uint32_t ui32IPMode);
extern void EthIntHandler(void);
extern void SysTickIntHandler(void);

#endif // __ETH_H__
