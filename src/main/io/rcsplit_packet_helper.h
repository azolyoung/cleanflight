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


#pragma once

void rcCamOSDGenerateWritePacket(sbuf_t *dst, uint16_t x, uint16_t y, const char *characters, uint6_t charactersLen);



// for unit test
void rcCamOSDPasrePacket(sbuf_t *src, rcsplit_packet_v2_t *outPacket);
void rcCamOSDParseWriteCommandData(uint8_t *data, uint16_t dataLen, rcsplit_osd_write_chars_data_t *outData);
void rcCamOSDParseClearCommandData(uint8_t *data, uint16_t dataLen, rcsplit_osd_clear_screen_data_t *outData);