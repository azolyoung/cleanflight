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

// communicate with camera device variables
static serialPort_t *rcsplitSerialPort = NULL;
static rcsplit_switch_state_t switchStates[BOXRCSPLITCHANGEMODE - BOXRCSPLITWIFI + 1];
static rcsplit_state_e cameraState = RCSPLIT_STATE_UNKNOWN;

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

static void sendCtrlCommand(rcsplit_ctrl_argument_e argument)
{
    if (!rcsplitSerialPort)
        return ;

    unsigned char uart_buffer[5] = {0};
    unsigned char crc = 0;

    uart_buffer[0] = RCSPLIT_PACKET_HEADER;
    uart_buffer[1] = RCSPLIT_PACKET_CMD_CTRL;
    uart_buffer[2] = argument;
    uart_buffer[3] = RCSPLIT_PACKET_TAIL;
    crc = crc_high_first(uart_buffer, 4);

    // build up a full request [header]+[command]+[argument]+[crc]+[tail]
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

    for (boxId_e i = BOXRCSPLITWIFI; i <= BOXRCSPLITCHANGEMODE; i++) {
        if (IS_RC_MODE_ACTIVE(i)) {
            // check last state of this mode, if it's true, then ignore it. 
            // Here is a logic to make a toggle control for this mode
            if (switchStates[i].isActivited) {
                continue;
            }

            uint8_t argument = RCSPLIT_CTRL_ARGU_INVALID;
            switch (i) {
                case BOXRCSPLITWIFI:
                    argument = RCSPLIT_CTRL_ARGU_WIFI_BTN;
                    break;
                case BOXRCSPLITPOWER:
                    argument = RCSPLIT_CTRL_ARGU_POWER_BTN;
                    break;
                case BOXRCSPLITCHANGEMODE:
                    argument = RCSPLIT_CTRL_ARGU_CHANGE_MODE;
                    break;
                default:
                    argument = RCSPLIT_CTRL_ARGU_INVALID;
                    break;
            }
            
            if (argument != RCSPLIT_CTRL_ARGU_INVALID) {
                sendCtrlCommand(argument);
                switchStates[i].isActivited = true;
            } else {
            }
        } else {
            switchStates[i].isActivited = false;
        }
    }
}

rcsplit_state_e unitTestRCsplitState()
{
    return cameraState;
}

bool unitTestIsSwitchActivited(boxId_e boxId)
{
    rcsplit_switch_state_t switchState = switchStates[boxId - BOXRCSPLITWIFI];

    return switchState.isActivited;
}

void unitTestResetRCSplit()
{
    rcsplitSerialPort = NULL;
    cameraState = RCSPLIT_STATE_UNKNOWN;
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
    for (boxId_e i = BOXRCSPLITWIFI; i <= BOXRCSPLITCHANGEMODE; i++) {
        switchStates[i].boxId = 1 << i;
        switchStates[i].isActivited = true; 
    }
    
    cameraState = RCSPLIT_STATE_IS_READY;

    return true;
}

void rcsplitProcess(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    if (rcsplitSerialPort == NULL)
        return ;

    // process rcsplit custom mode if has any changed
    rcsplitProcessMode();
}