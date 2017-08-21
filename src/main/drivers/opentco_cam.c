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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drivers/opentco.h"

static opentcoDevice_t openTCOCamDevice;
static opentcoDevice_t *device = &openTCOCamDevice;
static uint8_t video_system;

opentco_cam_switch_state_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];

PG_REGISTER_WITH_RESET_FN(opentcoCameraProfile_t, opentcoCameraProfile, PG_FPV_CAMERA_CONFIG, 0);

static void opentcoCamQuerySupportedFeatures()
{
    uint16_t opentcoCamFeatures = 0;

    // fetch available and acitvated features
    opentcoReadRegister(device, OPENTCO_CAM_REGISTER_SUPPORTED_FEATURES,  &opentcoCamFeatures);

    // store
    opentcoCameraProfileMutable()->supportedFeatures = opentcoCamFeatures;
}

static bool opentcoCamControl(opentcoDevice_t *device, uint8_t controlbehavior, uint16_t val)
{
    opentcoInitializeFrame(device, OPENTCO_CAM_COMMAND_CAMERA_CONTROL);
    sbufWriteU8(device->sbuf, controlbehavior);
    opentcoSendFrame(device);
    return true;
}

static bool isFeatureSupported(uint8_t feature)
{
    if (opentcoCameraProfile->supportedFeatures & feature)
        return true;

    return false;
}

static void opentcoCamProcessMode()
{
    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
        
        if (IS_RC_MODE_ACTIVE(i)) {
            // check last state of this mode, if it's true, then ignore it.
            // Here is a logic to make a toggle control for this mode
            if (switchStates[switchIndex].isActivated) {
                continue;
            }

            uint8_t behavior = 0;
            switch (i) {
            case BOXCAMERA1:
                if (isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_WIFI_BTN)
                    behavior = OPENTCO_CAM_CONTROL_SIMULATE_WIFI_BTN;
                break;
            case BOXCAMERA2:
                if (isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_POWER_BTN))
                    behavior = OPENTCO_CAM_CONTROL_SIMULATE_POWER_BTN;
                break;
            case BOXCAMERA3:
                if (isFeatureSupported(OPENTCO_CAM_FEATURE_CHANGE_MODE))
                    behavior = OPENTCO_CAM_CONTROL_SIMULATE_CHANGE_MODE;
                break;
            default:
                behavior = 0;
                break;
            }
            if (behavior != 0) {
                opentcoCamControl(device, behavior);
                switchStates[switchIndex].isActivated = true;
            }
        } else {
            switchStates[switchIndex].isActivated = false;
        }
    }
}

void opentcoCamProcess(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    // process camera custom mode if has any changed
    rcSplitProcessMode();
}

bool opentcoCamInit(void)
{
    // open serial port
    if (!opentcoInit(device)) {
        return false;
    }

    opentcoCamQuerySupportedFeatures();

#ifdef USE_RCSPLIT
    setTaskEnabled(TASK_RCSPLIT, true);
#endif

    return true;
}