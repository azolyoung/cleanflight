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
#include <ctype.h>

#include <platform.h>

#include "common/utils.h"

#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "fc/rc_controls.h"

#include "io/beeper.h"
#include "io/serial.h"

#include "scheduler/scheduler.h"

#include "drivers/serial.h"

#include "rcsplit/rcsplit.h"


#define STATIC_ASSERT(condition, name) \
    typedef char assert_failed_ ## name [(condition) ? 1 : -1 ]

// This is public setting in the RunCam Split tab of CleanFlight Configurator
// The user can bind the following mode with some aux channel in special range.
static const box_t rcSplitBoxes[RCSPLIT_CHECKBOX_ITEM_COUNT] = {
    { RCSPLIT_BOX_SIM_WIFI_BUTTON, "Wi-Fi Button", 0 },
    { RCSPLIT_BOX_SIM_POWER_BUTTON, "Power Button", 1 },
    { RCSPLIT_BOX_SIM_CHANGE_MODE, "Change Mode", 2 },
};

// mask of enabled RunCam Split Box IDs, calculated on startup based on enabled features. rcSplitBoxId_e is used as bit index
uint32_t rcSplitModeActivationMask; // one bit per mode defined in rcSplitBoxId_e


uint32_t rcSplitActiveBoxIds;
STATIC_ASSERT(sizeof(rcSplitActiveBoxIds) * 8 >= RCSPLIT_CHECKBOX_ITEM_COUNT, RCSPLIT_CHECKBOX_ITEMS_wont_fit_in_activeBoxIds);

// the rcsplit mode setting in the EEPRom
PG_REGISTER_ARRAY(modeActivationCondition_t, MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT, rcSplitModeActivationConditions, PG_RCSPLIT_MODE_ACTIVATION_PROFILE, 0);

// communicate with camera device variables
serialPort_t *rcSplitSerialPort = NULL;
rcsplit_switch_state_t switchStates[RCSPLIT_CHECKBOX_ITEM_COUNT];
rcsplit_state_e cameraState = RCSPLIT_STATE_UNKNOWN;

static unsigned char crc_high_first(unsigned char *ptr, unsigned char len)
{
    unsigned char i; 
    unsigned char crc=0x00;
    while(len--) {
        crc ^= *ptr++;  
        for (i=8; i>0; --i) { 
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
    return (crc); 
}

static void sendCtrlCommand(rcsplit_ctrl_cmd_argument_e argument)
{
    if (!rcSplitSerialPort)
        return ;

    uint8_t tmpBuffer[4];
    unsigned char uart_buffer[5] = {0};
    unsigned char crc = 0;

    // first calc crc with the request, 
    // note: the componnents that need to crc is [header]+[command]+[argument]+[tail], 
    // no need calc  with crc field, event the crc field is zero
    uart_buffer[0] = RCSPLIT_PACKET_HEADER;
    uart_buffer[1] = RCSPLIT_PACKET_CMD_CTRL;
    uart_buffer[2] = argument;
    uart_buffer[3] = RCSPLIT_PACKET_TAIL;

    crc = crc_high_first(tmpBuffer, 4);

    // build up a full request [header]+[command]+[argument]+[crc]+[tail]
    uart_buffer[3] = crc;
    uart_buffer[4] = RCSPLIT_PACKET_TAIL;

    // write to device
    serialWriteBuf(rcSplitSerialPort, uart_buffer, 5);
}

static void rcSplitProcessMode() 
{
    // if the device not ready, do not handle any mode change event
    if (RCSPLIT_STATE_IS_READY != cameraState) 
        return ;

    for (rcSplitBoxId_e i = 0; i < RCSPLIT_CHECKBOX_ITEM_COUNT; i++) {
        // simulate wifi button press action
        if (IS_RCSPLIT_MODE_ACTIVE(i)) {
            // check last state of this mode, if it's true, then ignore it. 
            // Here is a logic to make a toggle control for this mode
            if (switchStates[i].isActivated) {
                continue;
            }

            uint8_t argument = RCSPLIT_BOX_INVALID;
            switch (i) {
                case RCSPLIT_BOX_SIM_WIFI_BUTTON:
                    argument = 0x1;
                    break;
                case RCSPLIT_BOX_SIM_POWER_BUTTON:
                    argument = 0x2;
                    break;
                case RCSPLIT_BOX_SIM_CHANGE_MODE:
                    argument = 0x3;
                    break;
                default:
                    argument = RCSPLIT_BOX_INVALID;
                    break;
            }
            
            if (argument != RCSPLIT_BOX_INVALID) {
                sendCtrlCommand(argument);
                switchStates[i].isActivated = true;
            } else {
            }
        } else {
            switchStates[i].isActivated = false;
        }
    }
}

bool rcSplitInit(void)
{
    // found the port config with FUNCTION_RCSPLIT
    // User must set some UART inteface with RunCam Split at peripherals column in Ports tab
    serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RCSPLIT);
    if (portConfig) {
        rcSplitSerialPort = openSerialPort(portConfig->identifier, FUNCTION_RX_SERIAL, NULL, 115200, MODE_RXTX, 0);
    }

    if (!rcSplitSerialPort) {
        return false;
    }

    // set init value to true, to avoid the action auto run when the flight board start and the switch is on.
    for (rcSplitBoxId_e i = 0; i < RCSPLIT_CHECKBOX_ITEM_COUNT; i++) {
        switchStates[i].boxId = 1 << i;
        switchStates[i].isActivated = true; 
    }
    
    cameraState = RCSPLIT_STATE_IS_READY; 

    setTaskEnabled(TASK_RCSPLIT, true);

    return true;
}

void rcSplitProcess(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    if (rcSplitSerialPort == NULL)
        return ;

    // process rcsplit custom mode if has any changed
    rcSplitProcessMode();
}


const box_t *findRCSplitBoxByBoxId(rcSplitBoxId_e boxId)
{
    for (unsigned i = 0; i < ARRAYLEN(rcSplitBoxes); i++) {
        const box_t *candidate = &rcSplitBoxes[i];
        if (candidate->boxId == boxId)
            return candidate;
    }
    return NULL;
}

const box_t *findRCSplitBoxByPermanentId(uint8_t permanentId)
{
    for (unsigned i = 0; i < ARRAYLEN(rcSplitBoxes); i++) {
        const box_t *candidate = &rcSplitBoxes[i];
        if (candidate->permanentId == permanentId)
            return candidate;
    }
    return NULL;
}

// following functions is use for RunCam Split
void serializeRCSplitBoxNamesReply(sbuf_t *dst)
{
    for (rcSplitBoxId_e id = 0; id < RCSPLIT_CHECKBOX_ITEM_COUNT; id++) {
        if(rcSplitActiveBoxIds & (1 << id)) {
            
            const box_t *box = findRCSplitBoxByBoxId(id);
            sbufWriteString(dst, box->boxName);
            sbufWriteU8(dst, ';');
        }
    }
}

void serializeRCSplitBoxIdsReply(sbuf_t *dst)
{
    for (rcSplitBoxId_e id = 0; id < RCSPLIT_CHECKBOX_ITEM_COUNT; id++) {
        if(rcSplitActiveBoxIds & (1 << id)) {
            const box_t *box = findRCSplitBoxByBoxId(id);
            sbufWriteU8(dst, box->permanentId);
        }
    }
}

void initRCSplitActiveBoxIds() 
{
    uint32_t ena = 0;  // temporary variable to collect result
#define BME(boxId) do { ena |= (1 << (boxId)); } while(0)
    

    BME(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    BME(RCSPLIT_BOX_SIM_POWER_BUTTON);
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);

    rcSplitActiveBoxIds = ena;
}

void updateRCSplitActivatedModes(void)
{
    rcSplitModeActivationMask = 0;

    for (int index = 0; index < MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT; index++) {
        const modeActivationCondition_t *modeActivationCondition = rcSplitModeActivationConditions(index);

        if (isRangeActive(modeActivationCondition->auxChannelIndex, &modeActivationCondition->range)) {
            ACTIVATE_RCSPLIT_MODE(modeActivationCondition->modeId);
        }
    }
}