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


#include "io/beeper.h"
#include "io/serial.h"
#include "io/rcsplit.h"
#include "io/rcsplit_packet_helper.h"
#include "io/displayport_rccamera.h"

#ifdef USE_RCCAMERA_DISPLAYPORT

#define RCSPLIT_MAX_CHARS2UPDATE 50

static displayPort_t rccameraDisplayPort;

// #if USE_FULL_SCREEN_DRAWING
uint8_t rcsplitOSDScreenBuffer[RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC];
static uint8_t shadowBuffer[RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC];
static uint16_t rcsplitMaxScreenSize = RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC;
static uint8_t particleChangeBuffer[RCSPLIT_MAX_CHARS2UPDATE*6];
bool  rcsplitLock        = false;
// #endif

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


    // sbuf_t buf;
    // uint16_t expectedPacketSize = 0;
    // uint8_t *base = NULL;
    // expectedPacketSize = rcCamOSDGenerateClearPacket(NULL);
    // base = (uint8_t*)malloc(expectedPacketSize);
    // buf.ptr = base;
    // rcCamOSDGenerateClearPacket(&buf);
    // serialWriteBuf(rcSplitSerialPort, base, expectedPacketSize);
    // free(base);

// #if USE_FULL_SCREEN_DRAWING
    // uint16_t bufferLen = RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC;
    // uint16_t x;
    // uint32_t *p = (uint32_t*)&rcsplitOSDScreenBuffer[0];
    // for (x = 0; x < bufferLen/4; x++)
    //     p[x] = 0x20202020;
// #endif

    memset(rcsplitOSDScreenBuffer, 0x20, RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC);
    return 0;
}

static int drawScreen(displayPort_t *displayPort)
{
    UNUSED(displayPort);

    // if camera not ready, just return
    // if (!isCameraReady()) {
    //     return 0;
    // }

    if (!rcsplitLock) {
        
        rcsplitLock = true;
        static uint16_t pos = 0;
        uint8_t buff_len = 0;

        for (int i = 0; i < RCSPLIT_MAX_CHARS2UPDATE; i++) {
            if (rcsplitOSDScreenBuffer[pos] != shadowBuffer[pos]) {
                uint8_t x = pos % RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT;
                uint8_t y = pos / RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT;
                uint8_t c = rcsplitOSDScreenBuffer[pos];

                particleChangeBuffer[buff_len++] = x;
                particleChangeBuffer[buff_len++] = y;
                particleChangeBuffer[buff_len++] = c;
                shadowBuffer[pos] = rcsplitOSDScreenBuffer[pos];
            }

            if (++pos >= rcsplitMaxScreenSize) {
                pos = 0;
                break;
            }
        }

        if (buff_len) {
            sbuf_t buf;
            uint16_t expectedPacketSize = 0;
            uint8_t *base = NULL;
            expectedPacketSize = rcCamOSDGenerateDrawParticleScreenPacket(NULL, particleChangeBuffer, buff_len);
            base = (uint8_t*)malloc(expectedPacketSize);
            buf.ptr = base;
            rcCamOSDGenerateDrawParticleScreenPacket(&buf, particleChangeBuffer, buff_len);
            serialWriteBuf(rcSplitSerialPort, base, expectedPacketSize);
            free(base);    
        }

        rcsplitLock = false;
    }
    
    return 0;
}

static int screenSize(const displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return displayPort->rowCount * displayPort->colCount;
}

static int _writeString(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *s, uint16_t len)
{
    UNUSED(displayPort);

    uint8_t i = 0;
    for (i = 0; i < len; i++)
        if (x + i < RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT) // Do not write over screen
            rcsplitOSDScreenBuffer[y * RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT + x + i] = *(s + i);

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

static int fillRegion(displayPort_t *displayPort, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t value)
{
    UNUSED(displayPort);
    UNUSED(x);
    UNUSED(y);
    UNUSED(width);
    UNUSED(height);
    UNUSED(value);
}

static int reloadProfile(displayPort_t *displayPort)
{
    UNUSED(displayPort);
}

static const displayPortVTable_t rccameraDisplayPortVTable = {
    .grab = grab,
    .release = release,
    .clearScreen = clearScreen,
    .drawScreen = drawScreen,
    .fillRegion = fillRegion,
    .writeString = writeString,
    .writeChar = writeChar,
    .reloadProfile = reloadProfile,
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

    displayInit(&rccameraDisplayPort, &rccameraDisplayPortVTable);
    rccameraDisplayPort.rowCount = RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC;
    rccameraDisplayPort.colCount = RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT;

    return &rccameraDisplayPort;
}

#endif
