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
#include "io/displayport_rccamera.h"
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

void rcSplitReceive(timeUs_t currentTimeUs);

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

    // if (argument == 1) {
    //     uint8_t clearcmd[] = { 0x55, 0x21, 0x00, 0x11 };
    //     serialWriteBuf(rcSplitSerialPort, clearcmd, sizeof(clearcmd));

    //     uint8_t testbuf1[] = { 0x55, 0x22, 0xf0, 0x00, 0x02, 0x00, 0x01, 0x02, 0x01, 0x02, 0x02, 0x02, 0x03, 0x02, 0x03, 0x04, 0x02, 0x04, 0x05, 0x02, 0x05, 0x06, 0x02, 0x06, 0x07, 0x02, 0x07, 0x08, 0x02, 0x08, 0x09, 0x02, 0x09, 0x0a, 0x02, 0x0a, 0x0b, 0x02, 0x0b, 0x0c, 0x02, 0x0c, 0x0d, 0x02, 0x0d, 0x0e, 0x02, 0x0e, 0x0f, 0x02, 0x0f, 0x10, 0x02, 0x10, 0x11, 0x02, 0x11, 0x12, 0x02, 0x12, 0x13, 0x02, 0x13, 0x14, 0x02, 0x14, 0x15, 0x02, 0x15, 0x16, 0x02, 0x16, 0x17, 0x02, 0x17, 0x18, 0x02, 0x18, 0x19, 0x02, 0x19, 0x1a, 0x02, 0x1a, 0x1b, 0x02, 0x1b, 0x1c, 0x02, 0x1c, 0x1d, 0x02, 0x1d, 0x00, 0x03, 0x1e, 0x01, 0x03, 0x1f, 0x02, 0x03, 0x20, 0x03, 0x03, 0x21, 0x04, 0x03, 0x22, 0x05, 0x03, 0x23, 0x06, 0x03, 0x24, 0x07, 0x03, 0x25, 0x08, 0x03, 0x26, 0x09, 0x03, 0x27, 0x0a, 0x03, 0x28, 0x0b, 0x03, 0x29, 0x0c, 0x03, 0x2a, 0x0d, 0x03, 0x2b, 0x0e, 0x03, 0x2c, 0x0f, 0x03, 0x2d, 0x10, 0x03, 0x2e, 0x11, 0x03, 0x2f, 0x12, 0x03, 0x30, 0x13, 0x03, 0x31, 0x14, 0x03, 0x32, 0x15, 0x03, 0x33, 0x16, 0x03, 0x34, 0x17, 0x03, 0x35, 0x18, 0x03, 0x36, 0x19, 0x03, 0x37, 0x1a, 0x03, 0x38, 0x1b, 0x03, 0x39, 0x1c, 0x03, 0x3a, 0x1d, 0x03, 0x3b, 0x00, 0x04, 0x3c, 0x01, 0x04, 0x3d, 0x02, 0x04, 0x3e, 0x03, 0x04, 0x3f, 0x04, 0x04, 0x40, 0x05, 0x04, 0x41, 0x06, 0x04, 0x42, 0x07, 0x04, 0x43, 0x08, 0x04, 0x44, 0x09, 0x04, 0x45, 0x0a, 0x04, 0x46, 0x0b, 0x04, 0x47, 0x0c, 0x04, 0x48, 0x0d, 0x04, 0x49, 0x0e, 0x04, 0x4a, 0x0f, 0x04, 0x4b, 0x10, 0x04, 0x4c, 0x11, 0x04, 0x4d, 0x12, 0x04, 0x4e, 0x13, 0x04, 0x4f, 0x71 };
    //     serialWriteBuf(rcSplitSerialPort, testbuf1, sizeof(testbuf1));

    //     uint8_t testbuf2[] = { 0x55, 0x22, 0xf0, 0x14, 0x04, 0x50, 0x15, 0x04, 0x51, 0x16, 0x04, 0x52, 0x17, 0x04, 0x53, 0x18, 0x04, 0x54, 0x19, 0x04, 0x55, 0x1a, 0x04, 0x56, 0x1b, 0x04, 0x57, 0x1c, 0x04, 0x58, 0x1d, 0x04, 0x59, 0x00, 0x05, 0x5a, 0x01, 0x05, 0x5b, 0x02, 0x05, 0x5c, 0x03, 0x05, 0x5d, 0x04, 0x05, 0x5e, 0x05, 0x05, 0x5f, 0x06, 0x05, 0x60, 0x07, 0x05, 0x61, 0x08, 0x05, 0x62, 0x09, 0x05, 0x63, 0x0a, 0x05, 0x64, 0x0b, 0x05, 0x65, 0x0c, 0x05, 0x66, 0x0d, 0x05, 0x67, 0x0e, 0x05, 0x68, 0x0f, 0x05, 0x69, 0x10, 0x05, 0x6a, 0x11, 0x05, 0x6b, 0x12, 0x05, 0x6c, 0x13, 0x05, 0x6d, 0x14, 0x05, 0x6e, 0x15, 0x05, 0x6f, 0x16, 0x05, 0x70, 0x17, 0x05, 0x71, 0x18, 0x05, 0x72, 0x19, 0x05, 0x73, 0x1a, 0x05, 0x74, 0x1b, 0x05, 0x75, 0x1c, 0x05, 0x76, 0x1d, 0x05, 0x77, 0x00, 0x06, 0x78, 0x01, 0x06, 0x79, 0x02, 0x06, 0x7a, 0x03, 0x06, 0x7b, 0x04, 0x06, 0x7c, 0x05, 0x06, 0x7d, 0x06, 0x06, 0x7e, 0x07, 0x06, 0x7f, 0x08, 0x06, 0x80, 0x09, 0x06, 0x81, 0x0a, 0x06, 0x82, 0x0b, 0x06, 0x83, 0x0c, 0x06, 0x84, 0x0d, 0x06, 0x85, 0x0e, 0x06, 0x86, 0x0f, 0x06, 0x87, 0x10, 0x06, 0x88, 0x11, 0x06, 0x89, 0x12, 0x06, 0x8a, 0x13, 0x06, 0x8b, 0x14, 0x06, 0x8c, 0x15, 0x06, 0x8d, 0x16, 0x06, 0x8e, 0x17, 0x06, 0x8f, 0x18, 0x06, 0x90, 0x19, 0x06, 0x91, 0x1a, 0x06, 0x92, 0x1b, 0x06, 0x93, 0x1c, 0x06, 0x94, 0x1d, 0x06, 0x95, 0x00, 0x07, 0x96, 0x01, 0x07, 0x97, 0x02, 0x07, 0x98, 0x03, 0x07, 0x99, 0x04, 0x07, 0x9a, 0x05, 0x07, 0x9b, 0x06, 0x07, 0x9c, 0x07, 0x07, 0x9d, 0x08, 0x07, 0x9e, 0x09, 0x07, 0x9f, 0x6f };
    //     serialWriteBuf(rcSplitSerialPort, testbuf2, sizeof(testbuf2));

    //     uint8_t testbuf3[] = { 0x55, 0x22, 0x90, 0x00, 0x08, 0xa0, 0x01, 0x08, 0xa1, 0x02, 0x08, 0xa2, 0x03, 0x08, 0xa3, 0x04, 0x08, 0xa4, 0x05, 0x08, 0xa5, 0x06, 0x08, 0xa6, 0x07, 0x08, 0xa7, 0x08, 0x08, 0xa8, 0x09, 0x08, 0xa9, 0x0a, 0x08, 0xaa, 0x0b, 0x08, 0xab, 0x0c, 0x08, 0xac, 0x0d, 0x08, 0xad, 0x0e, 0x08, 0xae, 0x0f, 0x08, 0xaf, 0x10, 0x08, 0xb0, 0x11, 0x08, 0xb1, 0x12, 0x08, 0xb2, 0x13, 0x08, 0xb3, 0x14, 0x08, 0xb4, 0x15, 0x08, 0xb5, 0x16, 0x08, 0xb6, 0x17, 0x08, 0xb7, 0x00, 0x09, 0xb8, 0x01, 0x09, 0xb9, 0x02, 0x09, 0xba, 0x03, 0x09, 0xbb, 0x04, 0x09, 0xbc, 0x05, 0x09, 0xbd, 0x06, 0x09, 0xbe, 0x07, 0x09, 0xbf, 0x08, 0x09, 0xc0, 0x09, 0x09, 0xc1, 0x0a, 0x09, 0xc2, 0x0b, 0x09, 0xc3, 0x0c, 0x09, 0xc4, 0x0d, 0x09, 0xc5, 0x0e, 0x09, 0xc6, 0x0f, 0x09, 0xc7, 0x10, 0x09, 0xc8, 0x11, 0x09, 0xc9, 0x12, 0x09, 0xca, 0x13, 0x09, 0xcb, 0x14, 0x09, 0xcc, 0x15, 0x09, 0xcd, 0x16, 0x09, 0xce, 0x17, 0x09, 0xcf, 0x9a };
    //     serialWriteBuf(rcSplitSerialPort, testbuf3, sizeof(testbuf3));

    //     uint8_t testbuf4[] = { 0x55, 0x22, 0x90, 0x00, 0x0a, 0xd0, 0x01, 0x0a, 0xd1, 0x02, 0x0a, 0xd2, 0x03, 0x0a, 0xd3, 0x04, 0x0a, 0xd4, 0x05, 0x0a, 0xd5, 0x06, 0x0a, 0xd6, 0x07, 0x0a, 0xd7, 0x08, 0x0a, 0xd8, 0x09, 0x0a, 0xd9, 0x0a, 0x0a, 0xda, 0x0b, 0x0a, 0xdb, 0x0c, 0x0a, 0xdc, 0x0d, 0x0a, 0xdd, 0x0e, 0x0a, 0xde, 0x0f, 0x0a, 0xdf, 0x10, 0x0a, 0xe0, 0x11, 0x0a, 0xe1, 0x12, 0x0a, 0xe2, 0x13, 0x0a, 0xe3, 0x14, 0x0a, 0xe4, 0x15, 0x0a, 0xe5, 0x16, 0x0a, 0xe6, 0x17, 0x0a, 0xe7, 0x00, 0x0b, 0xe8, 0x01, 0x0b, 0xe9, 0x02, 0x0b, 0xea, 0x03, 0x0b, 0xeb, 0x04, 0x0b, 0xec, 0x05, 0x0b, 0xed, 0x06, 0x0b, 0xee, 0x07, 0x0b, 0xef, 0x08, 0x0b, 0xf0, 0x09, 0x0b, 0xf1, 0x0a, 0x0b, 0xf2, 0x0b, 0x0b, 0xf3, 0x0c, 0x0b, 0xf4, 0x0d, 0x0b, 0xf5, 0x0e, 0x0b, 0xf6, 0x0f, 0x0b, 0xf7, 0x10, 0x0b, 0xf8, 0x11, 0x0b, 0xf9, 0x12, 0x0b, 0xfa, 0x13, 0x0b, 0xfb, 0x14, 0x0b, 0xfc, 0x15, 0x0b, 0xfd, 0x16, 0x0b, 0xfe, 0x17, 0x0b, 0xff, 0x5a, 0x3 };
    //     serialWriteBuf(rcSplitSerialPort, testbuf4, sizeof(testbuf4));

    //     return ;
    // } else if (argument == 2) {
    //     uint8_t testbuf0[] = { 0x55, 0x23, 0x01, 0x02, 0x68 };
    //     serialWriteBuf(rcSplitSerialPort, testbuf0, sizeof(testbuf0));
    //     return ;
    // } else if (argument == 3) {
    //     uint8_t testbuf0[] = { 0x55, 0x23, 0x01, 0x03, 0x59 };
    //     serialWriteBuf(rcSplitSerialPort, testbuf0, sizeof(testbuf0));
    //     return ;
    // }

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
    if (rcSplitSerialPort)
        return true;

    // found the port config with FUNCTION_RUNCAM_SPLIT_CONTROL
    // User must set some UART inteface with RunCam Split at peripherals column in Ports tab
    serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RCSPLIT);
    if (portConfig) {
        rcSplitSerialPort = openSerialPort(portConfig->identifier, FUNCTION_RCSPLIT, NULL, 115200, MODE_RXTX, 0);
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

bool isCameraReady()
{
    return cameraState == RCSPLIT_STATE_IS_READY;
}