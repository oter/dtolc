#ifndef SCREEN_PROTO_H
#define SCREEN_PROTO_H

#include <cstdint>
#include <stdbool.h>

#include "crc16.h"

#define SCREEN_PACKET_ID 0x3b98aaf0

#define SCREEN_PACKET_DATA_SIZE 32

#pragma pack(push, 1)
struct ScreenPacket
{
    uint32_t start;
    int32_t order;
    uint32_t data[SCREEN_PACKET_DATA_SIZE];
    uint16_t crc;

    void Init(const uint8_t* data, uint32_t size)
    {
        this->start = SCREEN_PACKET_ID;
        this->order = 0;

        uint8_t i = 0;

        for (i = 0; i < 4 * 32 && i < size; ++i)
        {
            ((uint8_t*)(this->data))[i] = data[i];
        }

        for (uint32_t i = size; i < 4 * 32 && size < 4 * 32; ++i)
        {
            ((uint8_t*)(this->data))[i] = 0;
        }

        this->crc = 0;
    }

    void FillIntegrity(int32_t order = -1)
    {
        this->start = SCREEN_PACKET_ID;
        this->order = order;
        this->crc = calc_crc16((char*)this, sizeof(ScreenPacket) - sizeof(uint16_t /*crc*/)); 
    }

    bool CheckIntegrity(int32_t order = -1)
    {
        static uint32_t id = 0;

        uint16_t crc = calc_crc16((char*)this, sizeof(ScreenPacket) - sizeof(uint16_t /*crc*/));
        bool order_match = (order == -1) || (this->order == order);
        bool valid = (this->start == SCREEN_PACKET_ID) && order_match && (this->crc == crc);

        return valid;
    }
};
#pragma pack(pop)

#endif // SCREEN_PROTO_H
