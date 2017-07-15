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

#ifdef USE_RCCAMERA_DISPLAYPORT

#define RCCAMERA_SCREEN_WIDTH 320
#define RCCAMERA_SCREEN_HEIGHT 240

#define RCCAMERA_FONT_WIDTH 5
#define RCCAMERA_FONT_HEIGHT 7
#define RCCAMERA_HORIZONTAL_PADDING 1
#define RCCAMERA_VERTICAL_PADDING 1

#define RCCAMERA_CHARACTER_WIDTH_TOTAL (RCCAMERA_FONT_WIDTH + RCCAMERA_HORIZONTAL_PADDING)
#define RCCAMERA_CHARACTER_HEIGHT_TOTAL (RCCAMERA_FONT_HEIGHT + RCCAMERA_VERTICAL_PADDING)

#define RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT (RCCAMERA_SCREEN_WIDTH / RCCAMERA_CHARACTER_WIDTH_TOTAL)
#define RCCAMERA_SCREEN_CHARACTER_ROW_COUNT (RCCAMERA_SCREEN_HEIGHT / RCCAMERA_CHARACTER_HEIGHT_TOTAL)

displayPort_t rccameraDisplayPort;

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
    return 0;
}

static int drawScreen(displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return 0;
}

static int screenSize(const displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return maxScreenSize;
}

static int writeString(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *s)
{
    UNUSED(displayPort);
    return 0;
}

static int writeChar(displayPort_t *displayPort, uint8_t x, uint8_t y, uint8_t c)
{
    UNUSED(displayPort);
    return 0;
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
    .writeString = writeString,
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

    rccameraDisplayPort.device = cameraSerialPort;
    displayInit(&rccameraDisplayPort, &rccameraDisplayPortVTable);
    rccameraDisplayPort.rows = RCCAMERA_SCREEN_CHARACTER_ROW_COUNT;
    rccameraDisplayPort.cols = RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT;    

    return &rccameraDisplayPort;
}

#endif
