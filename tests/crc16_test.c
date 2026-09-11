/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stddef.h>
#include "ka_crc16_table.h"

static unsigned short checksum(unsigned short crc, const unsigned char *data, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        crc = (crc >> 8) ^ KA_CRC16_TABLE[(crc ^ data[i]) & 0xff];
    return crc;
}

int main(void)
{
    const unsigned char check[] = "123456789";
    assert(sizeof(KA_CRC16_TABLE) / sizeof(KA_CRC16_TABLE[0]) == 256);
    assert(checksum(0, check, 9) == 0xbb3d); /* CRC-16/ARC check value */
    assert(checksum(0xffff, check, 9) == 0x4b37); /* CRC-16/MODBUS */
    assert(checksum(0x1234, check, 0) == 0x1234);
    return 0;
}
