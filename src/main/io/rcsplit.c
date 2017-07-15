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

#include "drivers/serial.h"

#include "fc/rc_controls.h"
#include "fc/rc_modes.h"

#include "io/beeper.h"
#include "io/rcsplit.h"
#include "io/serial.h"

#include "scheduler/scheduler.h"

#include "drivers/serial.h"

#include "io/rcsplit.h"
#include "io/rcsplit_types.h"

// communicate with camera device variables
STATIC_UNIT_TESTED serialPort_t *rcSplitSerialPort = NULL;
STATIC_UNIT_TESTED rcsplitState_e cameraState = RCSPLIT_STATE_UNKNOWN;
// only for unit test
STATIC_UNIT_TESTED rcsplitSwitchState_t switchStates[BOXCAMERA3 - BOXCAMERA1 + 1];

// OSD
static uint8_t screenBuffer[VIDEO_BUFFER_CHARS_PAL+40]; // For faster writes we use memcpy so we need some space to don't overwrite buffer

static uint8_t crc_high_first(uint8_t *ptr, uint8_t len)
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
}2

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
    serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RCSPLIT);
    if (portConfig) {
        rcSplitSerialPort = openSerialPort(portConfig->identifier, FUNCTION_RCSPLIT, NULL, 115200, MODE_RXTX, 0);
    }

    if (!rcSplitSerialPort) {
        return false;
    }

    // set init value to true, to avoid the action auto run when the flight board start and the switch is on.
    for (boxId_e i = BOXCAMERA1; i <= BOXCAMERA3; i++) {
        uint8_t switchIndex = i - BOXCAMERA1;
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

static uint8_t rcCamCalcPacketCRC(sbuf_t *buf, uint8_t *base, uint16_t skipDataLocation, uint16_t skipDataLength)
{
    uint16_t offset = 0;
    uint8_t crc=0x00;
    sbufSwitchToReader(buf, base);

    while (sbufBytesRemaining(buf)) {
        if (offset == skipDataLocation) { // ingore the crc field
            sbufAdvance(buf, skipDataLength);
            continue;
        }

        uint8_t c = sbufReadU8(buf);
        crc ^= c;
        for (uint8_t i = 8; i > 0; --i) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }

        offset++;
    }

    return crc;
}

void rcCamOSDMakePacket(sbuf_t *src, uint8_t command, const uint8_t *data, uint8_t len)
{
    uint16_t pakcetLen = sizeof(rcsplit_packet_v2_t) - sizeof(uint8_t*) + len;
    uint8_t *packetBuffer = (uint8_t*)malloc(pakcetLen);
    uint8_t crcFieldOffset = 0;
    src->ptr = packetBuffer;

    sbufWriteU8(&src, RCSPLIT_PACKET_HEADER);
    sbufWriteU8(&src, command);
    sbufWriteU8(&src, len);
    sbufWriteData(&src, data, len);
    crcFieldOffset = (sbufConstPtr(&src) - packetBuffer;
    sbufWriteU8(&src, 0);
    sbufWriteU8(&src, RCSPLIT_PACKET_TAIL);

    // calc the crc of the packet, and skip the crc field
    uint8_t crc = rcCamCalcPacketCRC(&src, packetBuffer, crcFieldOffset, 1);
    src->ptr = packetBuffer;
    // write crc value to the position that it should be there
    sbufAdvance(src, crcFieldOffset);
    sbufWriteU8(&src, crc);

    // // send to to device
    // serialWriteBuf(rcSplitSerialPort, packetBuffer, pakcetLen);
}

void rcCamOSDWrite(sbuf_t *buf, uint16_t x, uint16_t y, const char *characters, uint6_t charactersLen)
{
    if (rcSplitSerialPort == NULL)
        return ;

    // fill the data struct
    uint16_t dataLen = sizeof(rcsplit_osd_write_chars_data_t) - sizeof(uint8_t*) + charactersLen;
    rcsplit_osd_write_chars_data_t *data = (rcsplit_osd_write_chars_data_t*)malloc(dataLen);
    data->align = RCSPLIT_OSD_TEXT_ALIGN_LEFT;
    data->x = x;
    data->y = y;
    memcpy(data->characters, characters, charactersLen);

    rcCamOSDSendPacket(buf, RCSPLIT_PACKET_CMD_OSD_WRITE_CHARS, (uint8_t*)data, dataLen);
}

void rcCamOSDWriteChar(uint8_t x, uint8_t y, uint8_t c)
{
    rcCamOSDWrite(x, y, c, 1);
}

void rcCamOSDClearScreen(void)
{

}