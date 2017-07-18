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

uint8_t rcCamCalcPacketCRC(sbuf_t *buf, uint8_t *base, uint16_t skipDataLocation, uint16_t skipDataLength)
{
    uint16_t offset = 0;
    uint8_t crc=0x00;
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
        crc = crc16_ccitt(crc, c);

        offset++;
        len = sbufBytesRemaining(buf);
    }

    return crc;
}

static uint16_t rcCamOSDGeneratePacket(sbuf_t *src, uint8_t command, const uint8_t *data, uint16_t len)
{
    uint16_t pakcetLen = sizeof(rcsplit_packet_v2_t) - sizeof(uint8_t*) + len;

    if (src == NULL) {
        return pakcetLen;
    }

    uint8_t *base = src->ptr;
    uint8_t crcFieldOffset = 0;

    sbufWriteU8(src, RCSPLIT_PACKET_HEADER);
    sbufWriteU8(src, command);
    sbufWriteU8(src, len);
    sbufWriteData(src, data, len);
    crcFieldOffset = sbufConstPtr(src) - base;
    sbufWriteU8(src, 0);
    sbufWriteU8(src, RCSPLIT_PACKET_TAIL);
    
    // calc the crc of the packet, and skip the crc field
    uint8_t crc = rcCamCalcPacketCRC(src, base, crcFieldOffset, sizeof(uint16_t));
    src->ptr = base;
    // write crc value to the position that it should be there
    sbufAdvance(src, crcFieldOffset);
    sbufWriteU16(src, crc);

    // skip to end, and reset it to read
    sbufAdvance(src, 1); 
    sbufSwitchToReader(src, base);

    return sbufBytesRemaining(src);
}

uint16_t convertInt16ToBigEndian(uint16_t val)
{
    uint16_t r = 0;

    uint16_t tmp = 0x1234;
    // check current cpu of hardware is using little endian or not, if it is, just convert the int to big endian
    if(*(char*)&tmp == 0x34) {
        r = val >> 8;
        r |= (val & 0xFF) << 8;
    } else{
        r = val;
    }
    return r;
}

uint16_t rcCamOSDGenerateDrawScreenPacket(sbuf_t *buf, uint8_t *screenBuffer)
{
    if (rcSplitSerialPort == NULL)
        return 0;

    uint16_t packetSize = rcCamOSDGeneratePacket(buf, 
                                                RCSPLIT_PACKET_CMD_OSD_DRAW_SCREEN, 
                                                screenBuffer, 
                                                RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT);

    return packetSize;
}

uint16_t rcCamOSDGenerateClearPacket(sbuf_t *buf)
{
    if (rcSplitSerialPort == NULL)
        return 0;

    // fill the data struct of command RCSPLIT_PACKET_CMD_OSD_DRAW_SCREEN
    uint16_t dataLen = sizeof(rcsplit_osd_clear_screen_data_t);
    rcsplit_osd_clear_screen_data_t *data = (rcsplit_osd_clear_screen_data_t*)malloc(dataLen);
    data->align = 0x01;
    data->start_x = 0;
    data->start_y = 0;
    data->end_x = convertInt16ToBigEndian(RCCAMERA_SCREEN_WIDTH - 1);
    data->end_y = convertInt16ToBigEndian(RCCAMERA_SCREEN_HEIGHT - 1);

    uint16_t packetSize = rcCamOSDGeneratePacket(buf, RCSPLIT_PACKET_CMD_OSD_CLEAR, (uint8_t*)data, dataLen);

    free(data);
    data = NULL;

    return packetSize;
}
