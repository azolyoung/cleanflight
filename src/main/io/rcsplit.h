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

#include <stdbool.h>
#include "common/time.h"
#include "fc/fc_msp.h"

typedef struct {
    bool isActivated;
} rcsplitSwitchState_t;

typedef enum {
    RCSPLIT_STATE_UNKNOWN = 0,
    RCSPLIT_STATE_INITIALIZING,
    RCSPLIT_STATE_IS_READY,
} rcsplitState_e;

// packet header and tail
#define RCSPLIT_PACKET_HEADER                   0x55
#define RCSPLIT_PACKET_CMD_CTRL                 0x01
#define RCSPLIT_PACKET_CMD_OSD_WRITE_CHARS      0x20 // write characters to OSD in rcsplit
#define RCSPLIT_PACKET_CMD_OSD_CLEAR            0x21
#define RCSPLIT_PACKET_TAIL                     0xaa


// the commands of RunCam Split serial protocol
typedef enum {
    RCSPLIT_CTRL_ARGU_INVALID = 0x0,
    RCSPLIT_CTRL_ARGU_WIFI_BTN = 0x1,
    RCSPLIT_CTRL_ARGU_POWER_BTN = 0x2,
    RCSPLIT_CTRL_ARGU_CHANGE_MODE = 0x3,
    RCSPLIT_CTRL_ARGU_WHO_ARE_YOU = 0xFF,
} rcsplit_ctrl_argument_e;

bool rcSplitInit(void);
void rcSplitProcess(timeUs_t currentTimeUs);

// only for unit test
extern rcsplitState_e cameraState;
extern serialPort_t *rcSplitSerialPort;
extern rcsplitSwitchState_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];

void rcCamOSDWrite(uint8_t x, uint8_t y, const char *buff);
void rcCamOSDWriteChar(uint8_t x, uint8_t y, uint8_t c);
void rcCamOSDClearScreen(void);
