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

#include "io/rcsplit.h"
#include "io/rcsplit_packet_helper.h"

// communicate with camera device variables
serialPort_t *rcSplitSerialPort = NULL;
rcsplit_switch_state_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];
rcsplit_state_e cameraState = RCSPLIT_STATE_UNKNOWN;

uint8_t crc_high_first(uint8_t *ptr, uint8_t len)
{
    uint8_t i; 
    uint8_t crc=0x00;
    while (len--) {
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
    if (!rcSplitSerialPort)
        return ;

    if (argument == 1) {
        // uint8_t screenBuffer[RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT] = {
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // 'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        // };

        // sbuf_t buf;
        // uint16_t expectedPacketSize = rcCamOSDGenerateDrawScreenPacket(NULL, screenBuffer);
        // uint8_t *base = (uint8_t*)malloc(expectedPacketSize);
        // buf.ptr = base;
        // uint16_t actualPacketSize = rcCamOSDGenerateDrawScreenPacket(&buf, screenBuffer);
        // serialWriteBuf(rcSplitSerialPort, base, actualPacketSize);
        // return ;

        // for (int i = 0; i < 16; i++) {
        //     sbuf_t buf;
        //     uint8_t *base = NULL;
        //     uint16_t expectedPacketSize = 0, actualPacketSize = 0;
        //     expectedPacketSize = rcCamOSDGenerateDrawStringPacket(NULL, 0, i, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 30);
        //     base = (uint8_t*)malloc(expectedPacketSize);
        //     buf.ptr = base;
        //     actualPacketSize = rcCamOSDGenerateDrawStringPacket(&buf, 0, i, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 30);
        //     serialWriteBuf(rcSplitSerialPort, base, actualPacketSize);
        //     free(base);
        // }

        // display logo and help
        // int x = 3;
        // int y = 1;
        // char fontOffset = 160;
        // for (int row = 0; row < 4; row++) {
        //     for (int column = 0; column < 24; column++) {
        //         if (fontOffset != 255)  {// FIXME magic number
        //             sbuf_t buf;
        //             uint16_t expectedPacketSize = 0;
        //             uint16_t actualPacketSize = 0;
        //             uint8_t *base = NULL;

        //             fontOffset++;
        //             expectedPacketSize = rcCamOSDGenerateDrawStringPacket(NULL, x + column, y + row, "?", 1);
        //             base = (uint8_t*)malloc(expectedPacketSize);
        //             buf.ptr = base;
        //             actualPacketSize = rcCamOSDGenerateDrawStringPacket(&buf, x + column, y + row, "?", 1);
        //             serialWriteBuf(rcSplitSerialPort, base, actualPacketSize);
        //             free(base);
        //         }
        //     }
        // }

        // return ;
    }
    // beeperConfirmationBeeps(3);
    uint8_t uart_buffer[5] = {0};
    uint8_t crc = 0;

    uart_buffer[0] = RCSPLIT_PACKET_HEADER;
    uart_buffer[1] = RCSPLIT_PACKET_CMD_CTRL;
    uart_buffer[2] = argument;
    uart_buffer[3] = RCSPLIT_PACKET_TAIL;
    crc = crc_high_first(uart_buffer, 4);

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

    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
        if (IS_RC_MODE_ACTIVE(i)) {
            // check last state of this mode, if it's true, then ignore it. 
            // Here is a logic to make a toggle control for this mode
            if (switchStates[switchIndex].isActivated) {
                continue;
            }

            uint8_t argument = RCSPLIT_CTRL_ARGU_INVALID;
            switch (i) {
            case BOXCAMERA1:
                argument = RCSPLIT_CTRL_ARGU_WIFI_BTN;
                break;
            case BOXCAMERA2:
                argument = RCSPLIT_CTRL_ARGU_POWER_BTN;
                break;
            case BOXCAMERA3:
                argument = RCSPLIT_CTRL_ARGU_CHANGE_MODE;
                break;
            default:
                argument = RCSPLIT_CTRL_ARGU_INVALID;
                break;
            }
            
            if (argument != RCSPLIT_CTRL_ARGU_INVALID) {
                sendCtrlCommand(argument);
                switchStates[switchIndex].isActivated = true;
            }
        } else {
            switchStates[switchIndex].isActivated = false;
        }
    }
}

bool rcSplitInit(void)
{
    // found the port config with FUNCTION_RUNCAM_SPLIT_CONTROL
    // User must set some UART inteface with RunCam Split at peripherals column in Ports tab
    if (rcSplitSerialPort == NULL) {
        serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RCSPLIT);
        if (portConfig) {
            rcSplitSerialPort = openSerialPort(portConfig->identifier, FUNCTION_RCSPLIT, NULL, 115200, MODE_RXTX, 0);
        }
    }

    if (!rcSplitSerialPort) {
        return false;
    }

    // set init value to true, to avoid the action auto run when the flight board start and the switch is on.
    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
        switchStates[switchIndex].boxId = 1 << i;
        switchStates[switchIndex].isActivated = true; 
    }
    
    cameraState = RCSPLIT_STATE_IS_READY;

#ifdef USE_RCSPLIT
    setTaskEnabled(TASK_RCSPLIT, true);
#endif

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
