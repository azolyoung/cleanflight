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
#include <stdlib.h>
#include <string.h>

#include "drivers/display.h"

#include "io/beeper.h"
#include "io/serial.h"
#include "io/rcsplit.h"
#include "io/rcsplit_packet_helper.h"

// #ifdef USE_RCCAMERA_DISPLAYPORT



displayPort_t rccameraDisplayPort;

#if USE_FULL_SCREEN_DRAWING
uint8_t rcsplitOSDScreenBuffer[RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT];
#endif

static int grab(displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return 0;
}

static int release(displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return 0;
}

static int clearScreen(displayPort_t *displayPort)
{
    UNUSED(displayPort);

#if USE_FULL_SCREEN_DRAWING
    sbuf_t buf;
    uint16_t expectedPacketSize = 0;
    uint8_t *base = NULL;
    expectedPacketSize = rcCamOSDGenerateClearPacket(NULL);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    rcCamOSDGenerateClearPacket(&buf);
    serialWriteBuf(rcSplitSerialPort, base, expectedPacketSize);
    free(base);

    uint16_t bufferLen = RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT;
    uint16_t x;
    uint32_t *p = (uint32_t*)&rcsplitOSDScreenBuffer[0];
    for (x = 0; x < bufferLen/4; x++)
        p[x] = 0x20202020;
#endif

    return 0;
}

static int drawScreen(displayPort_t *displayPort)
{
    UNUSED(displayPort);

#if USE_FULL_SCREEN_DRAWING
    sbuf_t buf;
    uint16_t expectedPacketSize = 0;
    uint8_t *base = NULL;
    expectedPacketSize = rcCamOSDGenerateDrawScreenPacket(NULL, rcsplitOSDScreenBuffer);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    rcCamOSDGenerateDrawScreenPacket(&buf, rcsplitOSDScreenBuffer);
    serialWriteBuf(rcSplitSerialPort, base, expectedPacketSize);
    free(base);
#endif

    return 0;
}

static int screenSize(const displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return displayPort->rows * displayPort->cols;
}

static int _writeString(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *s, uint16_t len)
{
    UNUSED(displayPort);

#if USE_FULL_SCREEN_DRAWING
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        if (x + i < RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT) // Do not write over screen
            rcsplitOSDScreenBuffer[y * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT + x + i] = *(s + i);
#else
    if (rcSplitSerialPort == NULL)
        return 0;

    sbuf_t buf;
    uint16_t expectedPacketSize = 0;
    uint16_t actualPacketSize = 0;
    uint8_t *base = NULL;

    // expectedPacketSize = rcCamOSDGenerateDrawStringPacket(NULL, x, y, s, len);
    // base = (uint8_t*)malloc(expectedPacketSize);
    // buf.ptr = base;
    // actualPacketSize = rcCamOSDGenerateDrawStringPacket(&buf, x, y, s, len);

    static int count = 0;

    
    expectedPacketSize = rcCamOSDGenerateDrawStringPacket(NULL, x, y, "AELL", 4);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    actualPacketSize = rcCamOSDGenerateDrawStringPacket(&buf, x, y, "AELL", 4);
    if (count % 10 == 0) {
        serialWriteBuf(rcSplitSerialPort, base, actualPacketSize);
    }
    count++;
    free(base);

#endif

    return 0;
}

static int writeString(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *s)
{
    return _writeString(displayPort, x, y, (const char*)s, strlen(s));
}

static int writeChar(displayPort_t *displayPort, uint8_t x, uint8_t y, uint8_t c)
{
    return _writeString(displayPort, x, y, (const char*)&c, 1);
}

static bool isTransferInProgress(const displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return false;
}

static void resync(displayPort_t *displayPort)
{
    UNUSED(displayPort);
}

static int heartbeat(displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return 0;
}

static uint32_t txBytesFree(const displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return UINT32_MAX;
}

static const displayPortVTable_t rccameraDisplayPortVTable = {
    .grab = grab,
    .release = release,
    .clearScreen = clearScreen,
    .drawScreen = drawScreen,
    .screenSize = screenSize,
    .write = writeString,
    .writeChar = writeChar,
    .isTransferInProgress = isTransferInProgress,
    .heartbeat = heartbeat,
    .resync = resync,
    .txBytesFree = txBytesFree,
};

displayPort_t *rccameraDisplayPortInit(serialPort_t *cameraSerialPort)
{
    if (!cameraSerialPort) {
        return NULL;
    }

    // rccameraDisplayPort.device = cameraSerialPort;
    displayInit(&rccameraDisplayPort, &rccameraDisplayPortVTable);
    rccameraDisplayPort.rows = RCCAMERA_SCREEN_CHARACTER_ROW_COUNT;
    rccameraDisplayPort.cols = RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT;    

    return &rccameraDisplayPort;
}

// #endif
