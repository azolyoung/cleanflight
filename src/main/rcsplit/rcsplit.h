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


// typedef enum {
//     RCSPLIT_BOXCAPTUREMODE = 0,
//     RCSPLIT_BOXSETTING,
//     RCSPLIT_BOXWIFIBUTTON,
//     RCSPLIT_RESERVED,
//     RCSPLIT_CHECKBOX_ITEM_COUNT
// } rcsplitBoxId_e;

typedef enum {
    RCSPLIT_BOX_SIM_WIFI_BUTTON = 0,
    RCSPLIT_BOX_SIM_POWER_BUTTON,
    RCSPLIT_BOX_SIM_CHANGE_MODE,
    RCSPLIT_CHECKBOX_ITEM_COUNT,
    RCSPLIT_BOX_INVALID = 255,
} rcsplitBoxId_e;

typedef struct {
    uint8_t boxId;
    bool isActivited;
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
    RCSPLIT_CTRL_ARGU_WHO_ARE_YOU = 0x4,
} rcsplit_ctrl_cmd_argument_e;



#define MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT 4

extern uint32_t rcsplitModeActivationMask;

#define IS_RCSPLIT_MODE_ACTIVE(modeId) ((1 << (modeId)) & rcsplitModeActivationMask)
#define ACTIVATE_RCSPLIT_MODE(modeId) (rcsplitModeActivationMask |= (1 << modeId))

PG_DECLARE_ARRAY(modeActivationCondition_t, MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT, rcsplitModeActivationConditions);

bool rcsplitInit(void);

void rcsplitProcess(timeUs_t currentTimeUs);
void initRCSplitActiveBoxIds();

const box_t *findRCSplitBoxByBoxId(rcsplitBoxId_e boxId);
const box_t *findRCSplitBoxByPermanentId(uint8_t permanentId);
void serializeRCSplitBoxNamesReply(sbuf_t *dst);
void serializeRCSplitBoxIdsReply(sbuf_t *dst);

void updateRCSplitActivatedModes(void);

// only for unit test
rcsplit_state_e unitTestRCsplitState();
bool unitTestIsSwitchActivited(rcsplitBoxId_e boxId);
void unitTestResetRCSplit();
void unitTestUpdateActiveBoxIds(uint32_t activeBoxIDs);
uint32_t unitTestGetActiveBoxIds();