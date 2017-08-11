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

uint16_t rcCamCalcPacketCRC(sbuf_t *buf, uint8_t *base, uint16_t skipDataLocation, uint16_t skipDataLength);
uint16_t rcCamOSDGeneratePacket(sbuf_t *src, uint8_t command, const uint8_t *data, uint16_t len);
// uint16_t rcCamOSDGenerateDrawStringPacket(sbuf_t *buf, uint8_t x, uint8_t y, const char *text, uint8_t textlen);
// uint16_t rcCamOSDGenerateDrawScreenPacket(sbuf_t *buf, uint8_t *screenBuffer);
// uint16_t rcCamOSDGenerateClearPacketAdvance(sbuf_t *buf, uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY);
uint16_t rcCamOSDGenerateGetCameraInfoPacket(sbuf_t *buf);
uint16_t rcCamOSDGenerateControlPacket(sbuf_t *src, uint8_t subcommand);
uint16_t rcCamOSDGenerateClearPacket(sbuf_t *buf);
uint16_t rcCamOSDGenerateDrawParticleScreenPacket(sbuf_t *buf, uint8_t *dataBuf, uint16_t dataLen);
