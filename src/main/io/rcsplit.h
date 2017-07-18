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
#include "io/rcsplit_types.h"

bool rcSplitInit(void);
void rcSplitProcess(timeUs_t currentTimeUs);

// only for unit test
extern rcsplit_state_e cameraState;
extern serialPort_t *rcSplitSerialPort;
extern rcsplit_switch_state_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];
