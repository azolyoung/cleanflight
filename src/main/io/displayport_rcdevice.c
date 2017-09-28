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

#include "platform.h"

#include "common/utils.h"

#include "config/feature.h"
#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "drivers/display.h"
#include "drivers/rcdevice.h"
#include "drivers/rcdevice_osd.h"
#include "drivers/vcd.h"

#include "io/displayport_rcdevice.h"
#include "io/osd.h"
#include "io/osd_slave.h"

#include "fc/config.h"

#if defined(USE_RCDEVICE)

displayPort_t rcdeviceOSDDisplayPort;

static const displayPortVTable_t rcdeviceOSDVTable = {
    .grab = rcdeviceOSDGrab,
    .release = rcdeviceOSDRelease,
    .clearScreen = rcdeviceOSDClearScreen,
    .drawScreen = rcdeviceOSDDrawScreen,
    .writeString = rcdeviceOSDWriteString,
    .writeChar = rcdeviceOSDWriteChar,
    .isTransferInProgress = rcdeviceOSDIsTransferInProgress,
    .heartbeat = rcdeviceOSDHeartbeat,
    .resync = rcdeviceOSDResync,
    .txBytesFree = rcdeviceOSDTxBytesFree,
    .screenSize = rcdeviceScreenSize,
};

displayPort_t *rcdeviceDisplayPortInit(const vcdProfile_t *vcdProfile)
{
    if (rcdeviceOSDInit(vcdProfile)) {
        displayInit(&rcdeviceOSDDisplayPort, &rcdeviceOSDVTable);
        rcdeviceOSDResync(&rcdeviceOSDDisplayPort);
        return &rcdeviceOSDDisplayPort;
    } else {
        return NULL;
    }
}

#endif // defined(USE_RCDEVICE)
