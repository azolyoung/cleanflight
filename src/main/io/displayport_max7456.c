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
#include <string.h>

#include "platform.h"

#ifdef USE_MAX7456

#include "common/utils.h"

//#include "config/parameter_group.h"
//#include "config/parameter_group_ids.h"

#include "drivers/display.h"
#include "drivers/max7456.h"
#include "drivers/vcd.h"

#include "io/displayport_max7456.h"
#include "io/osd.h"
#include "io/osd_slave.h"

displayPort_t max7456DisplayPort;

static int grab(displayPort_t *displayPort)
{
    // FIXME this should probably not have a dependency on the OSD or OSD slave code
    UNUSED(displayPort);
#if (defined(USE_OPENTCO) || defined(USE_MAX7456)) && !defined(USE_OSD_SLAVE)
    osdResetAlarms();
    resumeRefreshAt = 0;
#endif
    return 0;
}

static int release(displayPort_t *displayPort)
{
    UNUSED(displayPort);

    return 0;
}

static int drawScreen(displayPort_t *displayPort)
{
    UNUSED(displayPort);
    //max7456DrawScreenPartial();

    return 0;
}

static int writeString(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *s)
{
    UNUSED(displayPort);
    max7456WriteString(x, y, s);

    return 0;
}

static int writeChar(displayPort_t *displayPort, uint8_t x, uint8_t y, uint8_t c)
{
    UNUSED(displayPort);
    max7456WriteChar(x, y, c);

    return 0;
}

static int reloadProfile (displayPort_t *displayPort)
{
    UNUSED(displayPort);
    max7456ReloadProfile();

    return 0;
}


static int clearScreen(displayPort_t *displayPort)
{
    UNUSED(displayPort);
    max7456ClearScreen();

    return 0;
}

static int fillRegion(displayPort_t *displayPort, uint8_t xs, uint8_t ys, uint8_t width, uint8_t height, uint8_t value)
{
    UNUSED(displayPort);
    max7456FillRegion(xs, ys, width, height, value);

    return 0;
}

static bool isTransferInProgress(const displayPort_t *displayPort)
{
    UNUSED(displayPort);
    return max7456DmaInProgress();
}

static void resync(displayPort_t *displayPort)
{
    max7456RefreshAll();
    displayPort->rowCount = max7456GetRowsCount() + displayPortProfile()->rowAdjust;
    displayPort->colCount = 30 + displayPortProfile()->colAdjust;
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

static const displayPortVTable_t max7456VTable = {
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

displayPort_t *max7456DisplayPortInit(const vcdProfile_t *vcdProfile)
{
    // set supported featurs:
    displayPortProfileMutable()->supportedFeatures = MAX7456_FEATURESET;

    // init display port
    displayInit(&max7456DisplayPort, &max7456VTable);

    // init driver
    max7456Init(vcdProfile);

    // resync
    resync(&max7456DisplayPort);
    return &max7456DisplayPort;
}
#endif // USE_MAX7456
