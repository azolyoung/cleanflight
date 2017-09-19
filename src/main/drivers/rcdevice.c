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
#include <string.h>

#include "common/maths.h"
#include "common/streambuf.h"
#include "drivers/time.h"
#include "common/crc.h"
#include "rcdevice.h"

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

static void rcsplitSendCtrlCommand(runcamDevice_t *device, rcsplit_ctrl_argument_e argument)
{
    if (!device->serialPort)
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
}

static bool runcamDeviceReceiveDeviceInfo(runcamDevice_t *device)
{
    const uint8_t expectedDataLen = 16;
    uint8_t dataPos = 0;
    uint8_t data[expectedDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    while (serialRxBytesWaiting(trampSerialPort) && dataPos < expectedDataLen) {
        uint8_t c = serialRead(trampSerialPort);
        crc = crc8_dvb_s2(crc, c);
        data[dataPos++] = c;
    }

    if (crc != 0) return false;

    uint8_t protocolVersion = data[RCDEVICE_PROTOCOL_VERSION_STRING_LENGTH];
    if (protocolVersion >= RCDevice_PROTOCOL_UNKNOWN) return false;

    memset(device->info.firmwareVersion, 0, RCDEVICE_PROTOCOL_VERSION_STRING_LENGTH);
    memcpy(device->info.firmwareVersion, data, RCDEVICE_PROTOCOL_VERSION_STRING_LENGTH);
    device->info.protocolVersion = protocolVersion;

    uint8_t featureLowBits = data[RCDEVICE_PROTOCOL_VERSION_STRING_LENGTH + 1];
    uint8_t featureHighBits = data[RCDEVICE_PROTOCOL_VERSION_STRING_LENGTH + 2];
    device->info.features = (featureHighBits << 8) | featureLowBits;

    return true;
}

static bool runcamDeviceReceiveEmptyResponse(runcamDevice_t *device)
{
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    if (serialRxBytesWaiting(trampSerialPort)) {
        uint8_t c = serialRead(trampSerialPort);
        crc = crc8_dvb_s2(crc, c);
    }

    if (crc != 0) return false;

    return true;
}

static bool runcamDeviceReceiveConnectionEventResponse(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t outputBufferLen)
{
    const uint8_t expectedDataLen = 2;
    uint8_t dataPos = 0;
    uint8_t data[expectedDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    while (serialRxBytesWaiting(trampSerialPort) && dataPos < expectedDataLen) {
        uint8_t c = serialRead(trampSerialPort);
        crc = crc8_dvb_s2(crc, c);
        data[dataPos++] = c;
    }

    if (crc != 0) return false;

    uint8_t c = data[0];
    if (outputBufferLen >= 1 && outputBuffer)
        *outputBuffer = c;

    return true;
}

static bool runcamDeviceReceiveSettings(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t outputBufferLen)
{
    const uint8_t expectedDataLen = 62;
    uint8_t dataPos = 0;
    uint8_t data[expectedDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    while (serialRxBytesWaiting(trampSerialPort) {

    }

    return true;
}

static void runcamDeviceFlushRxBuffer(runcamDevice_t *device)
{
    while (serialRxBytesWaiting(device->serialPort)) serialRead(device->serialPort);
}

static void runcamDeviceInitializeFrame(runcamDevice_t *device, uint8_t command)
{
    // point to the buffer
    device->sbuf = &device->streamBuffer;
    // prepare pointer
    device->sbuf->ptr = device->buffer;
    device->sbuf->end = ARRAYEND(device->buffer);

    sbufWriteU8(device->sbuf, OPENTCO_PROTOCOL_HEADER);
    sbufWriteU8(device->sbuf, command);
}

static void runcamDeviceSendFrame(runcamDevice_t *device)
{
    // is this device open?
    if (!device->serialPort) {
        return;
    }

    // add crc over (all) data
    crc8_dvb_s2_sbuf_append(device->sbuf, device->buffer);

    // switch to reader
    sbufSwitchToReader(device->sbuf, device->buffer);

    // send data if possible
    serialPort_t *serialPort = device->serialPort;
    if (!serialPort->locked) {
        serialPort->locked = true;
        serialWriteBuf(device->serialPort, sbufPtr(device->sbuf), sbufBytesRemaining(device->sbuf));
        serialPort->locked = false;
    }
}

static void runcamDeviceSerialReceive(runcamDevice_t *device, uint8_t command, uint8_t *outputBuffer, uint8_t outputBufferLen)
{
    // wait 100ms for reply
    bool headerReceived = false;
    timeMs_t timeout = millis() + 100;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            uint8_t c = serialRead(device->serialPort);

            if (!headerReceived) {
                if (c == RCDEVICE_PROTOCOL_HEADER)
                    headerReceived = true;
            } else {
                bool decodeResult = false;
                switch (command) {
                case RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO:
                    decodeResult =runcamDeviceReceiveDeviceInfo(device);
                    break;
                case RCDEVICE_PROTOCOL_COMMAND_5KEY_SIMULATION_PRESS:
                case RCDEVICE_PROTOCOL_COMMAND_5KEY_SIMULATION_RELEASE:
                    decodeResult = runcamDeviceReceiveEmptyResponse(device);
                    break;
                case RCDEVICE_PROTOCOL_COMMAND_5KEY_CONNECTION:
                    decodeResult = runcamDeviceReceiveConnectionEventResponse(device, outputBuffer, outputBufferLen);
                    break;
                case RCDEVICE_PROTOCOL_COMMAND_GET_SETTINGS:
                    decodeResult = runcamDeviceReceiveSettings(device, outputBuffer, outputBufferLen);
                    break;
                }

                if (decodeResult)
                    return true;
                else
                    break;
            }
        }
    }

    return false;
}

static bool runcamDeviceSendRequestAndWaitingResp(runcamDevice_t *device, uint8_t commandID, sbuf_t *parameters, uint8_t *outputBuffer, uint8_t outputBufferLen)
{
    uint32_t max_retries = 3;
    while (max_retries--) {
        // flush rx buffer
        runcamDeviceFlushRxBuffer(device);

        // fill header
        runcamDeviceInitializeFrame(device, commandID);
        
        // fill parameters
        if (parameters)
            sbufWriteData(device->sbuf, sbufPtr(parameters), sbufBytesRemaining(parameters))

        // send command
        runcamDeviceSendFrame(device);

        // waiting response
        if (runcamDeviceSerialReceive(device, commandID, outputBuffer, outputBufferLen))
            return true;
    }

    return false;
}

static bool runcamDeviceGetDeviceInfo(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t outputBufferLen)
{
    return runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO, NULL, outputBuffer, outputBufferLen);
}

static bool runcamDeviceVerify(runcamDevice_t *device)
{
    // Note: This code is used to detect the old version firmware, and if detected, FC will using old version
    // protocol to tx/rx,  but in the future, it will remove this logic, so in that time, 
    // means user must update their firmware to newest. 
    // The detect logic is: Send 'who are you' command to device to detect the device is using 
    // rcsplit firmware 1.1, if so, then mark the device protocol version 
    // as RCDEVICE_PROTOCOL_RCSPLIT_VERSION
    runcamDeviceFlushRxBuffer(device);
    rcsplitSendCtrlCommand(RCSPLIT_CTRL_ARGU_WHO_ARE_YOU);
    uint8_t data[5];
    uint8_t dataPos = 0;
    while (serialRxBytesWaiting(device->serialPort) && dataPos < 5) {
        uint8_t c = serialRead(device->serialPort);
        data[dataPos++]
    }
    // swap the tail field and crc field, and verify the crc
    uint8_t t = data[3];
    data[3] = data[4];
    data[4] = t;
    if (crc_high_first(data, 5) == 0) {
        device->deviceInfo.protocolVersion = RCDEVICE_PROTOCOL_RCSPLIT_VERSION
        return true;
    }

    // try to send RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO to device, 
    // if the response is expected, then mark the device protocol version as RCDEVICE_PROTOCOL_VERSION_1_0
    if (runcamDeviceGetDeviceInfo(device, RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO, NULL, 0)) {
        device->deviceInfo.protocolVersion = RCDEVICE_PROTOCOL_VERSION_1_0
        return true;
    }

    return false;
}

static runcamDeviceSend5KeyOSDCableConnectionEvent(opentcoDevice_t *device, uint8_t operation)
{
    uint8_t buf;
    sbuf_t parametersBuf;
    uint8_t result = 0;
    uint8_t *base = &buf;
    parametersBuf.ptr = base;
    parametersBuf.end = base + 1;

    sbufWriteU8(&parametersBuf, operation);
    sbufSwitchToReader(parametersBuf, &buf);

    if (!runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO, parametersBuf, &result, sizeof(uint8_t)))
        return false;

    // the high 4 bits is the operationID that we sent
    // the low 4 bits is the result code
    uint8_t operationID = (result & 0xF0) >> 4;
    bool isSuccess = (result & 0x0F) == 1;
    if (operationID == operation && isSuccess)
        return true;

    return false;
}

bool runcamDeviceInit(runcamDevice_t *device)
{
    serialPortFunction_e portID = FUNCTION_RCDEVICE;
    serialPortConfig_t *portConfig = findSerialPortConfig(portID);
    bool isFirstTimeLoad = true;

    while (portConfig != NULL) {
        device->serialPort = openSerialPort(portConfig->identifier, portID, NULL, 115200, MODE_RXTX, SERIAL_NOT_INVERTED);
        
        if (device->serialPort != NULL) {
            if (isFirstTimeLoad) {
                // wait 400 ms if the device is not prepared(in booting)
                timeMs_t timeout = millis() + 400;
                while (millis() < timeout) {}
                isFirstTimeLoad = false;
            }
            
            if (runcamDeviceVerify(device)) {
                return true;
            }

            closeSerialPort(device->serialPort);
        }

        portConfig = findNextSerialPortConfig(portID);
    }

    device->serialPort = NULL;
    return false;
}

bool runcamDeviceSimulateCameraButton(opentcoDevice_t *device, uint8_t operation)
{
    runcamDeviceInitializeFrame(device, RCDEVICE_PROTOCOL_COMMAND_CAMERA_BTN_SIMULATION);
    sbufWriteU8(device->sbuf, operation);
    runcamDeviceSendFrame(device);

    return true;
}

bool runcamDeviceOpen5KeyOSDCableConnection(opentcoDevice_t *device)
{
    return runcamDeviceSend5KeyOSDCableConnectionEvent(device, RCDEVICE_PROTOCOL_5KEY_FUNCTION_OPEN);
}

bool runcamDeviceClose5KeyOSDCableConnection(opentcoDevice_t *device)
{
    return runcamDeviceSend5KeyOSDCableConnectionEvent(device, RCDEVICE_PROTOCOL_5KEY_FUNCTION_CLOSE);
}

bool runcamDeviceSimulate5KeyOSDCablePress(opentcoDevice_t *device, uint8_t operation)
{
    uint8_t buf;
    sbuf_t parametersBuf;
    uint8_t result = 0;
    uint8_t *base = &buf;
    parametersBuf.ptr = base;
    parametersBuf.end = base + 1;

    sbufWriteU8(&parametersBuf, operation);
    sbufSwitchToReader(parametersBuf, &buf);

    if (runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_5KEY_SIMULATION_PRESS, parametersBuf, &result, sizeof(uint8_t)))
        return true;

    return false;
}

bool runcamDeviceSimulate5KeyOSDCableRelease(opentcoDevice_t *device)
{
    return runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_5KEY_SIMULATION_RELEASE, NULL, &result, sizeof(uint8_t));
}
