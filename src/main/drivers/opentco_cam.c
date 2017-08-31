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

#include "common/time.h"

#include "config/parameter_group_ids.h"
#include "config/parameter_group.h"

#include "drivers/opentco.h"
#include "drivers/opentco_cam.h"

#include "fc/rc_controls.h"
#include "fc/runtime_config.h"

#include "rx/rx.h"

#define IS_HI(X)  (rcData[X] > 1750)
#define IS_LO(X)  (rcData[X] < 1250)
#define IS_MID(X) (rcData[X] > 1250 && rcData[X] < 1750)

static opentcoDevice_t openTCOCamDevice;
opentcoDevice_t *camDevice = &openTCOCamDevice;
opentco_cam_switch_state_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];
bool fpvCameraOSDMenuVisible = false;

PG_REGISTER(opentcoCameraProfile_t, opentcoCameraProfile, PG_FPV_CAMERA_CONFIG, 0);

static void opentcoCamQuerySupportedFeatures()
{
    uint16_t opentcoCamFeatures = 0;

    // fetch available and acitvated features
    opentcoReadRegister(camDevice, OPENTCO_CAM_REGISTER_SUPPORTED_FEATURES,  &opentcoCamFeatures);

    // store
    opentcoCameraProfileMutable()->supportedFeatures = opentcoCamFeatures;
}

static bool opentcoCamControl(opentcoDevice_t *camDevice, uint8_t controlbehavior)
{
    opentcoInitializeFrame(camDevice, OPENTCO_CAM_COMMAND_CAMERA_CONTROL);
    sbufWriteU8(camDevice->sbuf, controlbehavior);
    opentcoSendFrame(camDevice);
    return true;
}

bool opentcoCamGetCameraStatus(opentcoDevice_t *camDevice, uint8_t statusID)
{
    opentcoInitializeFrame(camDevice, OPENTCO_CAM_COMMAND_GET_CAMERA_STATUS);
    sbufWriteU8(camDevice->sbuf, statusID);
    opentcoSendFrame(camDevice);
    return true;
}

static bool isFeatureSupported(uint8_t feature)
{
    if (opentcoCameraProfile()->supportedFeatures & feature)
        return true;

    return false;
}

static void opentcoCamProcessMode()
{
    if (camDevice->serialPort == NULL)
        return ;

    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
        
        if (IS_RC_MODE_ACTIVE(i)) {
            // check last state of this mode, if it's true, then ignore it.
            // Here is a logic to make a toggle control for this mode
            if (switchStates[switchIndex].isActivated) {
                continue;
            }

            uint8_t behavior = 0;
            bool boxModeChange = false;
            switch (i) {
            case BOXCAMERA1:
                if (isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_WIFI_BTN)) {
                    behavior = OPENTCO_CAM_CONTROL_SIMULATE_WIFI_BTN;
                    boxModeChange = true;
                }
                break;
            case BOXCAMERA2:
                if (isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_POWER_BTN)) {
                    behavior = OPENTCO_CAM_CONTROL_SIMULATE_POWER_BTN;
                    boxModeChange = true;
                }
                break;
            case BOXCAMERA3:
                if (isFeatureSupported(OPENTCO_CAM_FEATURE_CHANGE_MODE)) {
                    behavior = OPENTCO_CAM_CONTROL_SIMULATE_CHANGE_MODE;
                    boxModeChange = true;
                }
                break;
            default:
                behavior = 0;
                break;
            }

            if (boxModeChange) {
                opentcoCamControl(camDevice, behavior);
                switchStates[switchIndex].isActivated = true;
            }
        } else {
            switchStates[switchIndex].isActivated = false;
        }
    }
}

void opentcoCamSimulate5KeyCablePress(cameraControlKey_e key)
{
    UNUSED(key);
}

void opentcoCam5KeyOSDCableUpdate(uint32_t currentTimeUs)
{
    UNUSED(currentTimeUs);
}

void opentcoCamProcess(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    // process camera custom mode if has any changed
    if (isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_POWER_BTN) ||
        isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_WIFI_BTN) ||
        isFeatureSupported(OPENTCO_CAM_FEATURE_CHANGE_MODE)) {
        opentcoCamProcessMode();
    }

    if (isFeatureSupported(OPENTCO_CAM_FEATURE_SIMULATE_5KEY_OSD_CABLE)) {
        if (IS_MID(THROTTLE) && IS_HI(YAW) && IS_HI(PITCH) && !ARMING_FLAG(ARMED)) {
            if (!fpvCameraOSDMenuVisible) {
                opentcoCamSimulate5KeyCablePress(CAMERA_CONTROL_KEY_ENTER);
                setArmingDisabled(ARMING_DISABLED_OPENCAM_OSD_MENU);
            }
        }

        opentcoCam5KeyOSDCableUpdate(currentTimeUs);
    }
}

bool opentcoCamInit(void)
{
    // open serial port
    camDevice->id = OPENTCO_DEVICE_CAM;
    
    if (!opentcoInit(camDevice)) {
        return false;
    }

    opentcoCamQuerySupportedFeatures();

    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
        switchStates[switchIndex].boxId = 1 << i;
        switchStates[switchIndex].isActivated = true; 
    }
    
    return true;
}