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
rcsplit_osd_camera_info_t cameraInfo = { RCSPLIT_VIDEOFMT_UNKNOWN  };

static uint8_t rcsplitRespBuffer[255];
static uint8_t resplitRespDataBlockLength = 0;

typedef enum {
    RCSPLIT_RECV_STATUS_WAIT_HEADER = 0,
    RCSPLIT_RECV_STATUS_WAIT_COMMAND,
    RCSPLIT_RECV_STATUS_WAIT_LENGTH,
    RCSPLIT_RECV_STATUS_WAIT_DATA,
    RCSPLIT_RECV_STATUS_WAIT_CRC
} rcsplitRecvStatus_e;

static int rcsplitReceivePos = 0;
rcsplitRecvStatus_e rcsplitReceiveState = RCSPLIT_RECV_STATUS_WAIT_HEADER;

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

static void retriveCameraInfo()
{
    sbuf_t buf;
    uint16_t expectedPacketSize = 0;
    uint8_t *base = NULL;
    expectedPacketSize = rcCamOSDGenerateGetCameraInfoPacket(NULL);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    
    rcCamOSDGenerateGetCameraInfoPacket(&buf);
    serialWriteBuf(rcSplitSerialPort, base, expectedPacketSize);
    free(base);
}

static void sendCtrlCommand(rcsplit_ctrl_argument_e argument)
{
    if (!rcSplitSerialPort)
        return ;

    if (argument == 1) {
        uint8_t testbuf1 = { 0x55, 0x22, 0xe0, 0x02, 0x55, 0x1a, 0x02, 0x56, 0x1b, 0x02, 0x57, 0x1c, 0x02, 0x58, 0x1d, 0x02, 0x59, 0x00, 0x03, 0x5a, 0x01, 0x03, 0x5b, 0x02, 0x03, 0x5c, 0x03, 0x03, 0x5d, 0x04, 0x03, 0x5e, 0x05, 0x03, 0x5f, 0x06, 0x03, 0x60, 0x07, 0x03, 0x61, 0x08, 0x03, 0x62, 0x09, 0x03, 0x63, 0x0a, 0x03, 0x64, 0x0b, 0x03, 0x65, 0x0c, 0x03, 0x66, 0x0d, 0x03, 0x67, 0x0e, 0x03, 0x68, 0x0f, 0x03, 0x69, 0x10, 0x03, 0x6a, 0x11, 0x03, 0x6b, 0x12, 0x03, 0x6c, 0x13, 0x03, 0x6d, 0x14, 0x03, 0x6e, 0x15, 0x03, 0x6f, 0x16, 0x03, 0x70, 0x17, 0x03, 0x71, 0x18, 0x03, 0x72, 0x19, 0x03, 0x73, 0x1a, 0x03, 0x74, 0x1b, 0x03, 0x75, 0x1c, 0x03, 0x76, 0x1d, 0x03, 0x77, 0x00, 0x04, 0x78, 0x01, 0x04, 0x79, 0x02, 0x04, 0x7a, 0x03, 0x04, 0x7b, 0x04, 0x04, 0x7c, 0x05, 0x04, 0x7d, 0x06, 0x04, 0x7e, 0x07, 0x04, 0x7f, 0x08, 0x04, 0x80, 0x09, 0x04, 0x81, 0x0a, 0x04, 0x82, 0x0b, 0x04, 0x83, 0x0c, 0x04, 0x84, 0x0d, 0x04, 0x85, 0x0e, 0x04, 0x86, 0x0f, 0x04, 0x87, 0x10, 0x04, 0x88, 0x11, 0x04, 0x89, 0x12, 0x04, 0x8a, 0x13, 0x04, 0x8b, 0x14, 0x04, 0x8c, 0x15, 0x04, 0x8d, 0x16, 0x04, 0x8e, 0x17, 0x04, 0x8f, 0x18, 0x04, 0x90, 0x19, 0x04, 0x91, 0x1a, 0x04, 0x92, 0x1b, 0x04, 0x93, 0x1c, 0x04, 0x94, 0x1d, 0x04, 0x95, 0x00, 0x05, 0x96, 0x01, 0x05, 0x97, 0x02, 0x05, 0x98, 0x03, 0x05, 0x99, 0x04, 0x05, 0x9a, 0x05, 0x05, 0x9b, 0x06, 0x05, 0x9c, 0x07, 0x05, 0x9d, 0x08, 0x05, 0x9e, 0x09, 0x05, 0x9f, 0xb7 };
        serialWriteBuf(rcSplitSerialPort, testbuf1, sizeof(testbuf1));

        uint8_t testbuf2 = { 0x55, 0x22, 0x60, 0x02, 0x55, 0x1a, 0x02, 0x56, 0x1b, 0x02, 0x57, 0x1c, 0x02, 0x58, 0x1d, 0x02, 0x59, 0x00, 0x03, 0x5a, 0x01, 0x03, 0x5b, 0x02, 0x03, 0x5c, 0x03, 0x03, 0x5d, 0x04, 0x03, 0x5e, 0x05, 0x03, 0x5f, 0x06, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0x03, 0x68, 0x0f, 0x03, 0x69, 0x10, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0x03, 0x72, 0x19, 0x03, 0x73, 0x1a, 0xd0, 0xd1, 0xd2, 0x57 };
        serialWriteBuf(rcSplitSerialPort, testbuf2, sizeof(testbuf2));

        return ;
    }

    uint8_t uart_buffer[5] = {0};
    uint8_t crc = 0;

    uart_buffer[0] = RCSPLIT_PACKET_HEADER;
    uart_buffer[1] = RCSPLIT_PACKET_CMD_V1_CTRL;
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

    cameraState = RCSPLIT_STATE_INITIALIZING;

    // set init value to true, to avoid the action auto run when the flight board start and the switch is on.
    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
        switchStates[switchIndex].boxId = 1 << i;
        switchStates[switchIndex].isActivated = true; 
    }
    
    retriveCameraInfo();

#ifdef USE_RCSPLIT
    setTaskEnabled(TASK_RCSPLIT, true);
#endif

    return true;
}

static void rcsplitResetReceiver()
{
    rcsplitReceiveState = RCSPLIT_RECV_STATUS_WAIT_HEADER;
    rcsplitReceivePos = 0;
}

static void rcsplitHandleResponse(void)
{
    uint8_t commandID = rcsplitRespBuffer[1] & 0x0F;
    uint8_t dataLen = rcsplitRespBuffer[2];
    switch (commandID) {
    case RCSPLIT_PACKET_CMD_GET_CAMERA_INFO:
        {
            if (dataLen < sizeof(rcsplit_osd_camera_info_t))
                return ;
                
            memcpy(&cameraInfo, rcsplitRespBuffer + 3, sizeof(rcsplit_osd_camera_info_t));

            cameraState = RCSPLIT_STATE_IS_READY;
        }
        break;
    case 100:
        break;
    }

    return ;
}

void rcSplitReceive(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    if (!rcSplitSerialPort)
        return ;

    while (serialRxBytesWaiting(rcSplitSerialPort)) {      
        uint8_t c = serialRead(rcSplitSerialPort);
        rcsplitRespBuffer[rcsplitReceivePos++] = c;

        switch(rcsplitReceiveState) {
            case RCSPLIT_RECV_STATUS_WAIT_HEADER:
                if (c == RCSPLIT_PACKET_HEADER) {
                    rcsplitReceiveState = RCSPLIT_RECV_STATUS_WAIT_COMMAND;
                } else {
                    rcsplitReceivePos = 0;
                }
                break;

            case RCSPLIT_RECV_STATUS_WAIT_COMMAND:
                {
                    uint8_t deviceID = (c & 0xF0) >> 4;
                    if (deviceID != 0x2) { // not camera device id
                        rcsplitResetReceiver();                        
                    } else {
                        rcsplitReceiveState = RCSPLIT_RECV_STATUS_WAIT_LENGTH; 
                    }
                }
                break;

            case RCSPLIT_RECV_STATUS_WAIT_LENGTH:
                rcsplitReceiveState = RCSPLIT_RECV_STATUS_WAIT_DATA;
                resplitRespDataBlockLength = c;
                break;

            case RCSPLIT_RECV_STATUS_WAIT_DATA:
                if ((rcsplitReceivePos - resplitRespDataBlockLength) == 3) {
                    rcsplitReceiveState = RCSPLIT_RECV_STATUS_WAIT_CRC;
                } 
                break;
            
            case RCSPLIT_RECV_STATUS_WAIT_CRC:
            {
                uint8_t crc = crc_high_first(rcsplitRespBuffer, rcsplitReceivePos - 1);
                if (crc == c) {
                    rcsplitHandleResponse();
                }

                rcsplitResetReceiver();
            }                
                break;

            default:
                rcsplitResetReceiver();
            }
    }

    return ;
}

void rcSplitProcess(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    if (rcSplitSerialPort == NULL)
        return ;

    rcSplitReceive(currentTimeUs);

    // process rcsplit custom mode if has any changed
    rcSplitProcessMode();
}
