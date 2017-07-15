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
#include <ctype.h>

static uint8_t rcCamCalcPacketCRC(sbuf_t *buf, uint8_t *base, uint16_t skipDataLocation, uint16_t skipDataLength)
{
    uint16_t offset = 0;
    uint8_t crc=0x00;
    sbufSwitchToReader(buf, base);

    while (sbufBytesRemaining(buf)) {
        if (offset == skipDataLocation) { // ingore the crc field
            sbufAdvance(buf, skipDataLength);
            continue;
        }

        uint8_t c = sbufReadU8(buf);
        crc ^= c;
        for (uint8_t i = 8; i > 0; --i) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }

        offset++;
    }

    return crc;
}

static void rcCamOSDGeneratePacket(sbuf_t *src, uint8_t command, const uint8_t *data, uint8_t len)
{
    uint16_t pakcetLen = sizeof(rcsplit_packet_v2_t) - sizeof(uint8_t*) + len;
    uint8_t *packetBuffer = (uint8_t*)malloc(pakcetLen);
    uint8_t crcFieldOffset = 0;
    src->ptr = packetBuffer;

    sbufWriteU8(&src, RCSPLIT_PACKET_HEADER);
    sbufWriteU8(&src, command);
    sbufWriteU8(&src, len);
    sbufWriteData(&src, data, len);
    crcFieldOffset = (sbufConstPtr(&src) - packetBuffer;
    sbufWriteU8(&src, 0);
    sbufWriteU8(&src, RCSPLIT_PACKET_TAIL);

    // calc the crc of the packet, and skip the crc field
    uint8_t crc = rcCamCalcPacketCRC(&src, packetBuffer, crcFieldOffset, 1);
    src->ptr = packetBuffer;
    // write crc value to the position that it should be there
    sbufAdvance(src, crcFieldOffset);
    sbufWriteU8(&src, crc);
}

void rcCamOSDGenerateWritePacket(sbuf_t *buf, uint16_t x, uint16_t y, const char *characters, uint6_t charactersLen)
{
    if (rcSplitSerialPort == NULL)
        return ;

    // fill the data struct
    uint16_t dataLen = sizeof(rcsplit_osd_write_chars_data_t) - sizeof(uint8_t*) + charactersLen;
    rcsplit_osd_write_chars_data_t *data = (rcsplit_osd_write_chars_data_t*)malloc(dataLen);
    data->align = RCSPLIT_OSD_TEXT_ALIGN_LEFT;
    data->x = x;
    data->y = y;
    memcpy(data->characters, characters, charactersLen);

    rcCamOSDGeneratePacket(buf, RCSPLIT_PACKET_CMD_OSD_WRITE_CHARS, (uint8_t*)data, dataLen);

    free(data);
    data = NULL;
}

void rcCamOSDPasrePacket(sbuf_t *src, rcsplit_packet_v2_t *outPacket)
{
    
}