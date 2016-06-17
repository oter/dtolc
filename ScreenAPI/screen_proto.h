#ifndef SCREEN_PROTO_H
#define SCREEN_PROTO_H

#include <cstdint>
#include <stdbool.h>

#include "crc16.h"

#pragma pack(push, 1)
struct ScreenPacket
{
    uint32_t type;
    uint32_t id;
    uint32_t data[32];
    uint16_t crc;

    void FillIntegrity(bool increment = true)
    {
        static uint32_t id = 0;

        this->id = id;
        this->crc = calc_crc16((char*)this, sizeof(ScreenPacket) - sizeof(uint16_t /*crc*/));

        if (increment)
        {
            ++id;
        }
    }

    bool CheckIntegrity(bool increment = true)
    {
        bool integrally = false;
        static uint32_t id = 0;

        uint16_t crc = calc_crc16((char*)this, sizeof(ScreenPacket) - sizeof(uint16_t /*crc*/));

        integrally = (this->crc == crc) && this->id == id;

        if (increment)
        {
            ++id;
        }

        return integrally;
    }
};
#pragma pack(pop)

#endif // SCREEN_PROTO_H
