/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "io/beeper.h"
#include "io/serial.h"

#include "scheduler/scheduler.h"

#include "drivers/serial.h"

#include "common/streambuf.h"
#include "common/maths.h"

#include "io/rcsplit.h"

uint8_t crc8_ccitt(uint8_t crc, unsigned char a)
{
    crc ^= a;
    for (uint8_t i = 8; i > 0; --i) { 
        if (crc & 0x80)
            crc = (crc << 1) ^ 0x31;
        else
            crc = (crc << 1);
    }
    return crc;
}

uint8_t rcCamCalcPacketCRC(sbuf_t *buf, uint8_t *base, uint8_t skipDataLocation, uint8_t skipDataLength)
{
    uint8_t offset = 0;
    uint8_t crc = 0x00;
    sbufSwitchToReader(buf, base);
    int len = sbufBytesRemaining(buf);
    while (len) {
        if (offset == skipDataLocation) { // ingore the crc field
            sbufAdvance(buf, skipDataLength);
            offset++;
            len = sbufBytesRemaining(buf);
            continue;
        }

        uint8_t c = sbufReadU8(buf);
        crc = crc8_ccitt(crc, c);

        offset++;
        len = sbufBytesRemaining(buf);
    }

    return crc;
}

uint16_t rcCamOSDGeneratePacket(sbuf_t *src, uint8_t command, const uint8_t *data, uint16_t len)
{
    uint16_t pakcetLen = sizeof(rcsplit_packet_v2_t) - sizeof(uint8_t*) + len;
    printf("ffff:%d\n", pakcetLen);
    printf("dddd:%d\n", len);
    if (src == NULL) {
        return pakcetLen;
    }

    uint8_t *base = src->ptr;
    uint8_t crcFieldOffset = 0;
    uint8_t combinedCommand = RCSPLIT_OPENCTO_CAMERA_DEVICE << 4 & 0xF0;
    combinedCommand |= command & 0x0F;

    sbufWriteU8(src, RCSPLIT_PACKET_HEADER);
    sbufWriteU8(src, combinedCommand);
    sbufWriteU8(src, len);
    if (len > 0) {
        sbufWriteData(src, data, len);
    }
    crcFieldOffset = sbufConstPtr(src) - base;
    printf("ggg:%d\n", crcFieldOffset);
    sbufWriteU8(src, 0);
    
    // calc the crc of the packet, and skip the crc field
    uint8_t crc = rcCamCalcPacketCRC(src, base, crcFieldOffset, sizeof(uint8_t));
    src->ptr = base;
    // write crc value to the position that it should be there
    sbufAdvance(src, crcFieldOffset);
    // sbufWriteU16(src, crc);
    sbufWriteU8(src, crc);

    sbufSwitchToReader(src, base);

    return sbufBytesRemaining(src);
}

uint16_t rcCamOSDGenerateControlPacket(sbuf_t *buf, uint8_t subcommand)
{
    uint16_t packetSize = rcCamOSDGeneratePacket(buf, RCSPLIT_PACKET_CMD_CTRL, &subcommand, sizeof(subcommand));
    return packetSize;
}

uint16_t rcCamOSDGenerateClearPacket(sbuf_t *buf)
{
    uint16_t packetSize = rcCamOSDGeneratePacket(buf, RCSPLIT_PACKET_CMD_OSD_CLEAR, NULL, 0);
    return packetSize;
}

uint16_t rcCamOSDGenerateDrawParticleScreenPacket(sbuf_t *buf, uint8_t *dataBuf, uint16_t dataLen)
{
    uint16_t packetSize = rcCamOSDGeneratePacket(buf, 
                                                RCSPLIT_PACKET_CMD_OSD_DRAW_PARTICLE_SCREEN_DATA, 
                                                dataBuf, 
                                                dataLen);
    return packetSize;
}

uint16_t rcCamOSDGenerateGetCameraInfoPacket(sbuf_t *buf)
{
    uint16_t packetSize = rcCamOSDGeneratePacket(buf, 
                                                RCSPLIT_PACKET_CMD_GET_CAMERA_INFO, 
                                                NULL, 
                                                0);
    return packetSize;
}