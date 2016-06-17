#ifndef SCREEN_PROTO_H
#define SCREEN_PROTO_H

#include <cstdint>
#include <stdbool.h>

#include "crc16.h"

#define SCREEN_PACKET_ID 0x3b98aaf0

#pragma pack(push, 1)
struct ScreenPacket
{
    uint32_t start;
    uint32_t id;
    uint32_t data[32];
    uint16_t crc;

    void Init()
    {
        this->start = SCREEN_PACKET_ID;
        this->id = 0;

        uint8_t i = 0;

        for (i = 0; i < 32; ++i)
        {
            this->data[i] = 0;
        }

        this->crc = 0;
    }

    void FillIntegrity(bool increment = true)
    {
        static uint32_t id = 0;

        this->start = SCREEN_PACKET_ID;
        this->id = id;
        this->crc = calc_crc16((char*)this, sizeof(ScreenPacket) - sizeof(uint16_t /*crc*/));

        if (increment)
        {
            ++id;
        }
    }

    bool CheckIntegrity(bool increment = true)
    {
        static uint32_t id = 0;

        uint16_t crc = calc_crc16((char*)this, sizeof(ScreenPacket) - sizeof(uint16_t /*crc*/));

        bool valid = (this->start == SCREEN_PACKET_ID) && (this->id == id) && (this->crc == crc);

        if (increment)
        {
            ++id;
        }

        return valid;
    }
};
#pragma pack(pop)

#endif // SCREEN_PROTO_H
