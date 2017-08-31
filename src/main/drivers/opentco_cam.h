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

#include "fc/rc_modes.h"
#include "drivers/camera_control.h"

typedef struct {
    uint8_t boxId;
    bool isActivated;
} opentco_cam_switch_state_t;

typedef struct opentcoCameraProfile_s {
    uint16_t supportedFeatures;
} opentcoCameraProfile_t;

PG_DECLARE(opentcoCameraProfile_t, opentcoCameraProfile);

extern opentcoDevice_t *camDevice;

void opentcoCamProcess(timeUs_t currentTimeUs);
bool opentcoCamInit(void);

// 5 key osd cable simulation
bool openCamIsSupport5KeyCalbleSimulation();
void opentcoCamSimulate5KeyCablePress(cameraControlKey_e key);

bool opentcoCamGetCameraStatus(opentcoDevice_t *camDevice, uint8_t statusID);

// used for unit test
opentco_cam_switch_state_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];
