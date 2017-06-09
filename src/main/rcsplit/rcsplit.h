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


typedef enum {
    RCSPLIT_BOX_SIM_WIFI_BUTTON = 0,
    RCSPLIT_BOX_SIM_POWER_BUTTON,
    RCSPLIT_BOX_SIM_CHANGE_MODE,
    RCSPLIT_CHECKBOX_ITEM_COUNT,
    RCSPLIT_BOX_INVALID = 255,
} rcSplitBoxId_e;

typedef struct {
    uint8_t boxId;
    bool isActivated;
} rcsplit_switch_state_t;

typedef enum {
    RCSPLIT_STATE_UNKNOWN = 0,
    RCSPLIT_STATE_INITIALIZING,
    RCSPLIT_STATE_IS_READY,
} rcsplit_state_e;

// packet header and tail
#define RCSPLIT_PACKET_HEADER           0x55
#define RCSPLIT_PACKET_CMD_CTRL  0x01
#define RCSPLIT_PACKET_TAIL     0xaa


// the argumens of RCSPLIT_PACKET_CMD_CTRL command
typedef enum {
    RCSPLIT_CTRL_ARGU_PRESS_WIFI_BUTTON = 0x1,
    RCSPLIT_CTRL_ARGU_PRESS_POWER_BUTTON = 0x2,
    RCSPLIT_CTRL_ARGU_PRESS_CHANGE_MODE_BUTTON = 0x3,
    RCSPLIT_CTRL_ARGU_WHO_ARE_YOU = 0xFF,
} rcsplit_ctrl_cmd_argument_e;



#define MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT 4

extern serialPort_t *rcSplitSerialPort;
extern uint32_t rcSplitModeActivationMask;
extern rcsplit_state_e cameraState;
extern rcsplit_switch_state_t switchStates[RCSPLIT_CHECKBOX_ITEM_COUNT];
extern uint32_t rcSplitActiveBoxIds;

#define IS_RCSPLIT_MODE_ACTIVE(modeId) ((1 << (modeId)) & rcSplitModeActivationMask)
#define ACTIVATE_RCSPLIT_MODE(modeId) (rcSplitModeActivationMask |= (1 << modeId))

PG_DECLARE_ARRAY(modeActivationCondition_t, MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT, rcSplitModeActivationConditions);

bool rcSplitInit(void);

void rcSplitProcess(timeUs_t currentTimeUs);
void initRCSplitActiveBoxIds();

const box_t *findRCSplitBoxByBoxId(rcSplitBoxId_e boxId);
const box_t *findRCSplitBoxByPermanentId(uint8_t permanentId);
void serializeRCSplitBoxNamesReply(sbuf_t *dst);
void serializeRCSplitBoxIdsReply(sbuf_t *dst);

void updateRCSplitActivatedModes(void);
