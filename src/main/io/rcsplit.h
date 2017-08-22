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
#include "fc/rc_modes.h"
#include "drivers/opentco_cam.h"

#define RCSPLIT_PACKET_HEADER           0x55
#define RCSPLIT_PACKET_TAIL             0xaa // it's decrepted on Protocol V2 for split

typedef enum {
    RCSPLIT_STATE_UNKNOWN = 0,
    RCSPLIT_STATE_INITIALIZING,
    RCSPLIT_STATE_IS_READY,
    RCSPLIT_STATE_INITIALIZE_FAILED,
} rcsplit_state_e;

typedef enum {
    RCSPLIT_PACKET_CMD_CTRL =                           0x01
} rcsplit_packet_cmd_e;

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
bool isCameraReady();

// only for unit test
extern rcsplit_state_e cameraState;
extern serialPort_t *rcSplitSerialPort;
extern opentco_cam_switch_state_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];
