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

#include "drivers/serial.h"

#include "rcsplit/rcsplit.h"


#define STATIC_ASSERT(condition, name) \
    typedef char assert_failed_ ## name [(condition) ? 1 : -1 ]

// This is public setting in the RunCam Split tab of CleanFlight Configurator
// The user can bind the following mode with some aux channel in special range.
static const box_t rcsplitBoxes[RCSPLIT_CHECKBOX_ITEM_COUNT] = {
    { RCSPLIT_BOX_SIM_WIFI_BUTTON, "Wi-Fi Button", 0 },
    { RCSPLIT_BOX_SIM_POWER_BUTTON, "Power Button", 1 },
    { RCSPLIT_BOX_SIM_CHANGE_MODE, "Change Mode", 2 },
};

// mask of enabled RunCam Split Box IDs, calculated on startup based on enabled features. rcsplitBoxId_e is used as bit index
uint32_t rcsplitModeActivationMask; // one bit per mode defined in rcsplitBoxId_e


static uint32_t rcsplitActiveBoxIds;
STATIC_ASSERT(sizeof(rcsplitActiveBoxIds) * 8 >= RCSPLIT_CHECKBOX_ITEM_COUNT, RCSPLIT_CHECKBOX_ITEMS_wont_fit_in_activeBoxIds);

// the rcsplit mode setting in the EEPRom
PG_REGISTER_ARRAY(modeActivationCondition_t, MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT, rcsplitModeActivationConditions, PG_RCSPLIT_MODE_ACTIVATION_PROFILE, 0);

// communicate with camera device variables
static serialPort_t *rcsplitSerialPort = NULL;
static rcsplit_switch_state_t switchStates[RCSPLIT_CHECKBOX_ITEM_COUNT];
static rcsplit_state_e cameraState = RCSPLIT_STATE_UNKNOWN;

// response packet variables
typedef enum {
    S_WAIT_HEADER = 0,      // Waiting for the packet header flag
    S_WAIT_CMD,   // Waiting for the command in the packet
    S_WAIT_DATA,      // Waiting for the argument of command
    S_WAIT_CRC,           // Waiting for the packet crc
    S_WAIT_TAIL,        // Waiting for a packet tail flag
} rcsplitReceiveState_e;
static rcsplitReceiveState_e rcsplitReceiveState = S_WAIT_HEADER;
static int rcsplitReceivePos = 0;
static uint8_t rcsplitRespBuffer[5];

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

static unsigned char rcsplit_verify_data(unsigned char *dat)
{
	unsigned char buffer[4] = {0};
	unsigned char ret = 0;
	unsigned char calc_crc = 0;
	unsigned char crc = dat[3];
	
	buffer[0] = dat[0];
	buffer[1] = dat[1];
	buffer[2] = dat[2];
	buffer[3] = dat[4];

	if((RCSPLIT_PACKET_HEADER == buffer[0]) && (RCSPLIT_PACKET_TAIL == buffer[3])) {
		calc_crc = crc_high_first(buffer,4);
		if(crc == calc_crc) {
			ret = 1;
		}
	}
	return ret;
}

static void sendCtrlCommand(rcsplit_ctrl_cmd_argument_e argument)
{
    if (!rcsplitSerialPort)
        return ;

    uint8_t tmpBuffer[4];
    unsigned char uart_buffer[5] = {0};
    unsigned char crc = 0;

    // first calc crc with the request, 
    // note: the componnents that need to crc is [header]+[command]+[argument]+[tail], 
    // no need calc  with crc field, event the crc field is zero
    tmpBuffer[0] = RCSPLIT_PACKET_HEADER;
    tmpBuffer[1] = RCSPLIT_PACKET_CMD_CTRL;
    tmpBuffer[2] = argument;
    tmpBuffer[3] = RCSPLIT_PACKET_TAIL;

    crc = crc_high_first(tmpBuffer, 4);

    // build up a full request [header]+[command]+[argument]+[crc]+[tail]
    uart_buffer[0] = RCSPLIT_PACKET_HEADER;
    uart_buffer[1] = RCSPLIT_PACKET_CMD_CTRL;
    uart_buffer[2] = argument;
    uart_buffer[3] = crc;
    uart_buffer[4] = RCSPLIT_PACKET_TAIL;

    // write to device
    serialWriteBuf(rcsplitSerialPort, uart_buffer, 5);
}

static void rcsplitProcessMode() 
{
    // if the device not ready, do not handle any mode change event
    if (RCSPLIT_STATE_IS_READY != cameraState) 
        return ;

    for (rcsplitBoxId_e i = 0; i < RCSPLIT_CHECKBOX_ITEM_COUNT; i++) {
        // simulate wifi button press action
        if (IS_RCSPLIT_MODE_ACTIVE(i)) {
            // check last state of this mode, if it's true, then ignore it. 
            // Here is a logic to make a toggle control for this mode
            if (switchStates[i].isActivited) {
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
                switchStates[i].isActivited = true;
            } else {
            }
        } else {
            switchStates[i].isActivited = false;
        }
    }
}

static void rcsplitHandleResponse()
{
    uint8_t cmd = rcsplitRespBuffer[1];
    uint8_t arg = rcsplitRespBuffer[2];
    if (cmd != RCSPLIT_PACKET_CMD_CTRL) {
        return ;
    }

    if (arg == 0xFF) { 
        // this is the response of RCSPLIT_PACKET_CMD_CTRL with argument RCSPLIT_CTRL_ARGU_WHO_ARE_YOU
        // so here we set the camera state to RCSPLIT_STATE_IS_READY, means camera is ready to work
        cameraState = RCSPLIT_STATE_IS_READY;
    }
}

static void rcsplitResetReceiver()
{
    rcsplitReceiveState = S_WAIT_HEADER;
    rcsplitReceivePos = 0;
}

static void rcsplitReceive()
{
    if (!rcsplitSerialPort)
        return ;

    // receive data loop
    while (serialRxBytesWaiting(rcsplitSerialPort)) {
        uint8_t c = serialRead(rcsplitSerialPort);
        rcsplitRespBuffer[rcsplitReceivePos++] = c;

        switch(rcsplitReceiveState) {
        case S_WAIT_HEADER:
            if (c == RCSPLIT_PACKET_HEADER) {
                rcsplitReceiveState = S_WAIT_CMD;
            } else {
                rcsplitReceivePos = 0;
            }
            break;

        case S_WAIT_CMD:
            if (c == RCSPLIT_PACKET_CMD_CTRL) { // current we only handle RCSPLIT_PACKET_CMD_CTRL
                rcsplitReceiveState = S_WAIT_DATA;
            } else {
                rcsplitResetReceiver();
            }
            break;

        case S_WAIT_DATA:
            rcsplitReceiveState = S_WAIT_CRC;
            break;
        case S_WAIT_CRC:
            rcsplitReceiveState = S_WAIT_TAIL;
            break;
        case S_WAIT_TAIL: 
            // receive packet tail, means the packet is receive done.
            // we need verify the packet
            if (rcsplit_verify_data(rcsplitRespBuffer)) { 
                // crc is valid, start handle this response
                rcsplitHandleResponse();
            }

            rcsplitResetReceiver();
            break;
        default:
            rcsplitResetReceiver();
        }
    }
}

rcsplit_state_e unitTestRCsplitState()
{
    return cameraState;
}

bool unitTestIsSwitchActivited(rcsplitBoxId_e boxId)
{
    rcsplit_switch_state_t switchState = switchStates[boxId];

    return switchState.isActivited;
}

void unitTestResetRCSplit()
{
    rcsplitSerialPort = NULL;
    cameraState = RCSPLIT_STATE_UNKNOWN;
    rcsplitReceiveState = S_WAIT_HEADER;
    rcsplitReceivePos = 0;
}

void unitTestUpdateActiveBoxIds(uint32_t activeBoxIDs)
{
    rcsplitActiveBoxIds = activeBoxIDs;
}

uint32_t unitTestGetActiveBoxIds()
{
    return rcsplitActiveBoxIds;
}

bool rcsplitInit(void)
{
    // found the port config with FUNCTION_RUNCAM_SPLIT_CONTROL
    // User must set some UART inteface with RunCam Split at peripherals column in Ports tab
    serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RUNCAM_SPLIT_CONTROL);
    if (portConfig) {
        rcsplitSerialPort = openSerialPort(portConfig->identifier, FUNCTION_RX_SERIAL, NULL, 115200, MODE_RXTX, 0);
    }

    if (!rcsplitSerialPort) {
        return false;
    }

    // set init value to true, to avoid the action auto run when the flight board start and the switch is on.
    for (rcsplitBoxId_e i = 0; i < RCSPLIT_CHECKBOX_ITEM_COUNT; i++) {
        switchStates[i].boxId = 1 << i;
        switchStates[i].isActivited = true; 
    }
    
    // send ctrl command with RCSPLIT_CTRL_ARGU_WHO_ARE_YOU to device, if we got response,
    // means the device was ready.
    sendCtrlCommand(RCSPLIT_CTRL_ARGU_WHO_ARE_YOU);
    cameraState = RCSPLIT_STATE_IS_READY; // update camera state to initializing

    return true;
}

void rcsplitProcess(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    if (rcsplitSerialPort == NULL)
        return ;

    // process the response from RunCam Split
    rcsplitReceive();

    // process rcsplit custom mode if has any changed
    rcsplitProcessMode();
}


const box_t *findRCSplitBoxByBoxId(rcsplitBoxId_e boxId)
{
    for (unsigned i = 0; i < ARRAYLEN(rcsplitBoxes); i++) {
        const box_t *candidate = &rcsplitBoxes[i];
        if (candidate->boxId == boxId)
            return candidate;
    }
    return NULL;
}

const box_t *findRCSplitBoxByPermanentId(uint8_t permanentId)
{
    for (unsigned i = 0; i < ARRAYLEN(rcsplitBoxes); i++) {
        const box_t *candidate = &rcsplitBoxes[i];
        if (candidate->permanentId == permanentId)
            return candidate;
    }
    return NULL;
}

// following functions is use for RunCam Split
void serializeRCSplitBoxNamesReply(sbuf_t *dst)
{
    for (rcsplitBoxId_e id = 0; id < RCSPLIT_CHECKBOX_ITEM_COUNT; id++) {
        if(rcsplitActiveBoxIds & (1 << id)) {
            
            const box_t *box = findRCSplitBoxByBoxId(id);
            sbufWriteString(dst, box->boxName);
            sbufWriteU8(dst, ';');
        }
    }
}

void serializeRCSplitBoxIdsReply(sbuf_t *dst)
{
    for (rcsplitBoxId_e id = 0; id < RCSPLIT_CHECKBOX_ITEM_COUNT; id++) {
        if(rcsplitActiveBoxIds & (1 << id)) {
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

    rcsplitActiveBoxIds = ena;
}

void updateRCSplitActivatedModes(void)
{
    rcsplitModeActivationMask = 0;

    for (int index = 0; index < MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT; index++) {
        const modeActivationCondition_t *modeActivationCondition = rcsplitModeActivationConditions(index);

        if (isRangeActive(modeActivationCondition->auxChannelIndex, &modeActivationCondition->range)) {
            ACTIVATE_RCSPLIT_MODE(modeActivationCondition->modeId);
        }
    }
}