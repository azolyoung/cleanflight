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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "common/crc.h"
#include "common/maths.h"
#include "drivers/time.h"
#include "rcdevice.h"

#include "config/feature.h"
#include "fc/config.h"

#include "io/serial.h"

// #include "io/rcdevice_cam.h"

typedef enum {
    RCDP_SETTING_PARSE_WAITING_ID,
    RCDP_SETTING_PARSE_WAITING_NAME,
    RCDP_SETTING_PARSE_WAITING_VALUE,
} runcamDeviceSettingParseStep_e;

// the crc calc function for rcsplit 1.0 and 1.1, this function will
// deprecate in feature
static uint8_t crc_high_first(uint8_t *ptr, uint8_t len)
{
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *ptr++;
        for (int i = 8; i > 0; --i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return (crc);
}

// a send packet method for rcsplit 1.0 and 1.1, this function will
// deprecate in feature
static void rcsplitSendCtrlCommand(runcamDevice_t *device, rcsplit_ctrl_argument_e argument)
{
    if (!device->serialPort) {
        return;
    }

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
    serialWriteBuf(device->serialPort, uart_buffer, 5);
}

// decode the device info
static bool runcamDeviceReceiveDeviceInfo(runcamDevice_t *device)
{
    const uint8_t expectedDataLen = 3;
    uint8_t dataPos = 0;
    uint8_t data[expectedDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);

    // wait 1000ms for reply
    timeMs_t timeout = millis() + 1000;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            uint8_t c = serialRead(device->serialPort);
            crc = crc8_dvb_s2(crc, c);
            data[dataPos++] = c;
        }

        if (dataPos >= expectedDataLen) {
            break;
        }
    }

    // check crc
    if (crc != 0) {
        return false;
    }

    uint8_t protocolVersion = data[0];

    if (protocolVersion >= RCDEVICE_PROTOCOL_UNKNOWN) {
        return false;
    }

    device->info.protocolVersion = protocolVersion;

    uint8_t featureLowBits = data[1];
    uint8_t featureHighBits = data[2];
    device->info.features = (featureHighBits << 8) | featureLowBits;

    return true;
}

// decode the empty response
static bool runcamDeviceReceiveEmptyResponse(runcamDevice_t *device)
{
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);

    timeMs_t timeout = millis() + 1000;
    uint8_t dataLen = 0;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            uint8_t c = serialRead(device->serialPort);
            crc = crc8_dvb_s2(crc, c);
            dataLen++;
        }

        if (dataLen >= 1) {
            break;
        }
    }

    if (crc != 0) {
        return false;
    }

    return true;
}

// decode the write setting response
static bool runcamDeviceReceiveWriteSettingResponse(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);

    const uint8_t expectedDataLen = 2;
    uint8_t data[expectedDataLen];
    timeMs_t timeout = millis() + 1000;
    uint8_t dataLen = 0;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            uint8_t c = serialRead(device->serialPort);
            crc = crc8_dvb_s2(crc, c);
            data[dataLen++] = c;
        }

        if (dataLen >= 3) {
            break;
        }
    }

    uint8_t errorCode = data[0];
    if (errorCode != 0) {
        return false;
    }

    if (crc != 0) {
        return false;
    }

    if (outputBufferLen && (*outputBufferLen >= dataLen) && outputBuffer) {
        memcpy(outputBuffer, data, dataLen);
        *outputBufferLen = dataLen;
    }

    return true;
}

// decode the connection event response
static bool runcamDeviceReceiveConnectionEventResponse(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    const uint8_t expectedDataLen = 2;
    uint8_t dataPos = 0;
    uint8_t data[expectedDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    timeMs_t timeout = millis() + 1000;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            uint8_t c = serialRead(device->serialPort);
            crc = crc8_dvb_s2(crc, c);
            data[dataPos++] = c;
        }

        if (dataPos >= expectedDataLen) {
            break;
        }
    }

    if (crc != 0) {
        return false;
    }

    uint8_t c = data[0];
    if (outputBufferLen && (*outputBufferLen >= 1) && outputBuffer) {
        *outputBuffer = c;
        *outputBufferLen = 1;
    }

    return true;
}

// decode the device setting response
static bool runcamDeviceReceiveSettings(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    UNUSED(device);

    const uint8_t maxDataLen = 63;
    uint8_t dataPos = 0;
    uint8_t data[maxDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    uint8_t settingDataLength = 0xFF;

    timeMs_t timeout = millis() + 1000;
    bool settingDataLengthReceived = false;
    while (millis() < timeout) {
        if (!settingDataLengthReceived) {
            // get setting datalen first
            if (serialRxBytesWaiting(device->serialPort) > 2) {
                // skip the remaining chunk count to get the setting data length
                uint8_t c = serialRead(device->serialPort);
                crc = crc8_dvb_s2(crc, c);
                data[dataPos++] = c;
        
                settingDataLength = serialRead(device->serialPort);
                crc = crc8_dvb_s2(crc, settingDataLength);
                data[dataPos++] = settingDataLength;
            }
        
            if (settingDataLength > 60) {
                return false;
            }

            settingDataLengthReceived = true;
        } else {
            if (serialRxBytesWaiting(device->serialPort) > 0) {
                uint8_t c = serialRead(device->serialPort);
                crc = crc8_dvb_s2(crc, c);
                data[dataPos++] = c;
            }

            // the reason of add 3 bytes is: remaining chunk count field + setting data length field + crc field
            if (dataPos >= (settingDataLength + 3)) { 
                break;
            }
        }
    }

    if (crc != 0) {
        return false;
    }

    if (outputBufferLen && (*outputBufferLen >= (dataPos - 1)) && outputBuffer) {
        memcpy(outputBuffer, data, dataPos - 1); // return the data expect the crc field
        *outputBufferLen = dataPos - 1;
    }

    return true;
}

// decode the device setting response
static bool runcamDeviceReceiveSettingDetail(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    const uint8_t maxDataLen = 63;
    uint8_t dataPos = 0;
    uint8_t data[maxDataLen];
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    uint8_t settingType = RCDEVICE_PROTOCOL_SETTINGTYPE_UNKNOWN;
    timeMs_t timeout = millis() + 1000;
    bool isFoundANullTeminatedChar = false;
    uint8_t expecteRemainingDataLen = 2;
    uint8_t expectedPacketLen = 0xFF;
    bool settingTypeReceived = false;
    bool receiveCurrentValueForTextSelectionType = false;
    while (millis() < timeout) {
        if (!settingTypeReceived) {
            if (serialRxBytesWaiting(device->serialPort) >= 2) {
                // skip the remaining chunk count to get the setting data length
                uint8_t c = serialRead(device->serialPort);
                crc = crc8_dvb_s2(crc, c);
                data[dataPos++] = c;

                settingType = serialRead(device->serialPort);
                crc = crc8_dvb_s2(crc, settingType);
                data[dataPos++] = settingType;

                switch (settingType) {
                case RCDEVICE_PROTOCOL_SETTINGTYPE_UINT8:
                case RCDEVICE_PROTOCOL_SETTINGTYPE_INT8:
                    expectedPacketLen = 7; // [value]+[min value]+[max value]+[step size]+[crc], all of them(expect crc) are type of UINT8 or INT8
                    break;
                case RCDEVICE_PROTOCOL_SETTINGTYPE_UINT16:
                case RCDEVICE_PROTOCOL_SETTINGTYPE_INT16:
                    expectedPacketLen = 11; // [value]+[min value]+[max value]+[step size]+[crc], all of them(expect crc) are type of UINT16 or INT16
                    break;
                case RCDEVICE_PROTOCOL_SETTINGTYPE_FLOAT:
                    expectedPacketLen = 9; // [value:UINT8]+[min value:UINT8]+[max value:UINT8]+[decimal point:UINT16]+[step size:UINT8]+[crc:UINT8]
                    break;
                case RCDEVICE_PROTOCOL_SETTINGTYPE_TEXT_SELECTION:
                    expectedPacketLen = 63; // [value:UINT8]+[text_selections]+[crc:UINT8], text_selections is a null-terminated string
                    break;
                case RCDEVICE_PROTOCOL_SETTINGTYPE_STRING:
                    expectedPacketLen = 63; // [value]+[max string size:UINT8]+[crc:UINT8], value is a null-terminated string
                    break;
                case RCDEVICE_PROTOCOL_SETTINGTYPE_FOLDER:
                case RCDEVICE_PROTOCOL_SETTINGTYPE_INFO:
                    expectedPacketLen = 3; // expect [remain chunk count]+[setting type], there is only crc field in it
                    break;
                default:
                    return false; // unexpected setting type
                }

                settingTypeReceived = 1;
            }
        } else {
            if (serialRxBytesWaiting(device->serialPort) > 0) {
                uint8_t c = serialRead(device->serialPort);
                crc = crc8_dvb_s2(crc, c);
                data[dataPos++] = c;

                if (isFoundANullTeminatedChar) {
                    expecteRemainingDataLen--;
                    if (expecteRemainingDataLen == 0) {
                        break;;
                    }
                }

                if (settingType == RCDEVICE_PROTOCOL_SETTINGTYPE_TEXT_SELECTION) {
                    if (!receiveCurrentValueForTextSelectionType) {
                        receiveCurrentValueForTextSelectionType = true;
                    } else {
                        if (c == 0) {
                            // found the null teminated char, and get next char(crc field), then break the loop
                            isFoundANullTeminatedChar = true; 
                            expecteRemainingDataLen = 1;
                        }
                    }
                } else if (settingType == RCDEVICE_PROTOCOL_SETTINGTYPE_STRING) {
                    // found the null teminated char, and get max string size and crc field, then break the loop
                    isFoundANullTeminatedChar = true; 
                    expecteRemainingDataLen = 2;
                }
            }

            if (dataPos >= expectedPacketLen) { 
                break;
            }
        }
    }

    if (crc != 0) {
        return false;
    }

    if (outputBufferLen && (*outputBufferLen >= (dataPos - 1)) && outputBuffer) {
        memcpy(outputBuffer, data,
               dataPos - 1); // return the data expect the crc field
        *outputBufferLen = dataPos - 1;
    }

    return true;
}

// every time send packet to device, and want to get something from device,
// it'd better call the method to clear the rx buffer before the packet send,
// else may be the useless data in rx buffer will cause the response decoding
// failed.
static void runcamDeviceFlushRxBuffer(runcamDevice_t *device)
{
    while (serialRxBytesWaiting(device->serialPort) > 0) {
        serialRead(device->serialPort);
    }
}

// a common way to send packet to device
static void runcamDeviceSendPacket(runcamDevice_t *device, uint8_t command, uint8_t *paramData, int paramDataLen)
{
    // is this device open?
    if (!device->serialPort) {
        return;
    }

    sbuf_t buf;
    // prepare pointer
    buf.ptr = device->buffer;
    buf.end = ARRAYEND(device->buffer);

    sbufWriteU8(&buf, RCDEVICE_PROTOCOL_HEADER);
    sbufWriteU8(&buf, command);

    if (paramData) {
        sbufWriteData(&buf, paramData, paramDataLen);
    }

    // add crc over (all) data
    crc8_dvb_s2_sbuf_append(&buf, device->buffer);

    // switch to reader
    sbufSwitchToReader(&buf, device->buffer);

    // send data if possible
    serialPort_t *serialPort = device->serialPort;
    if (!serialPort->locked) {
        serialPort->locked = true;
        serialWriteBuf(device->serialPort, sbufPtr(&buf), sbufBytesRemaining(&buf));
        serialPort->locked = false;
    }
}

// a common way to receive data from device, and will decode the response,
// and save the response data(all the fields from response except the header and
// crc field) to outputBuffer if the outputBuffer not a NULL pointer.
static bool runcamDeviceSerialReceive(runcamDevice_t *device, uint8_t command, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    // wait 1000ms for reply
    bool headerReceived = false;
    timeMs_t timeout = millis() + 1000;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            if (!headerReceived) {
                uint8_t c = serialRead(device->serialPort);
                if (c == RCDEVICE_PROTOCOL_HEADER) {
                    headerReceived = true;
                }
            } else {
                bool decodeResult = false;
                switch (command) {
                case RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO:
                    decodeResult = runcamDeviceReceiveDeviceInfo(device);
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
                case RCDEVICE_PROTOCOL_COMMAND_READ_SETTING_DETAIL:
                    decodeResult = runcamDeviceReceiveSettingDetail(device, outputBuffer, outputBufferLen);
                    break;
                case RCDEVICE_PROTOCOL_COMMAND_WRITE_SETTING:
                    decodeResult = runcamDeviceReceiveWriteSettingResponse(device, outputBuffer, outputBufferLen);
                    break;
                }

                if (decodeResult) {
                    return true;
                }

                break;
            }
        }
    }

    return false;
}

// a common way to send a packet to device, and get response from the device.
static bool runcamDeviceSendRequestAndWaitingResp(runcamDevice_t *device, uint8_t commandID, uint8_t *paramData, uint8_t paramDataLen, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    uint32_t max_retries = 3;
    while (max_retries--) {
        // flush rx buffer
        runcamDeviceFlushRxBuffer(device);

        // send packet
        runcamDeviceSendPacket(device, commandID, paramData, paramDataLen);

        // waiting response
        if (runcamDeviceSerialReceive(device, commandID, outputBuffer, outputBufferLen)) {
            return true;
        }
    }

    return false;
}

// get the device info(firmware version, protocol version and features, see the
// definition of runcamDeviceInfo_t to know more)
static bool runcamDeviceGetDeviceInfo(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen) { return runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO, NULL, 0, outputBuffer, outputBufferLen); }

static bool runcamDeviceVerify(runcamDevice_t *device)
{
    // Note: This code is used to detect the old version firmware, and if
    // detected, FC will using old version protocol to tx/rx,  but in the
    // future, it will be remove, so in that time, it's means user must update
    // their firmware to newest that using RunCam Device Protocol. The detect
    // logic is: Send 'who are you' command to device to detect the device is
    // using rcsplit firmware 1.1, if so, then mark the device protocol version
    // as RCDEVICE_PROTOCOL_RCSPLIT_VERSION
    // runcamDeviceFlushRxBuffer(device);
    // rcsplitSendCtrlCommand(device, RCSPLIT_CTRL_ARGU_WHO_ARE_YOU);
    // timeMs_t timeout = millis() + 500;
    // uint8_t dataPos = 0;
    // uint8_t data[5];
    // while (millis() < timeout) {
    //     if ((serialRxBytesWaiting(device->serialPort) > 0) && dataPos < 5) {
    //         uint8_t c = serialRead(device->serialPort);
    //         data[dataPos++] = c;
    //     }
    // }
    // if (dataPos >= 5) {
    //     // swap the tail field and crc field, and verify the crc
    //     uint8_t t = data[3];
    //     data[3] = data[4];
    //     data[4] = t;
    //     if (crc_high_first(data, 5) == 0) {
    //         device->info.protocolVersion = RCDEVICE_PROTOCOL_RCSPLIT_VERSION;
    //         return true;
    //     }
    // }

    // try to send RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO to device,
    // if the response is expected, then mark the device protocol version as
    // RCDEVICE_PROTOCOL_VERSION_1_0
    if (runcamDeviceGetDeviceInfo(device, NULL, 0)) {
        return true;
    }

    return false;
}

static bool runcamDeviceSend5KeyOSDCableConnectionEvent(runcamDevice_t *device, uint8_t operation, uint8_t *outActionID, uint8_t *outErrorCode)
{
    uint8_t result = 0;
    uint8_t outputDataLen = 1;
    if (!runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_5KEY_CONNECTION, &operation, sizeof(uint8_t), &result, &outputDataLen)) {
        return false;
    }

    // the high 4 bits is the operationID that we sent
    // the low 4 bits is the result code
    uint8_t operationID = (result & 0xF0) >> 4;
    bool errorCode = (result & 0x0F);
    if (outActionID) {
        *outActionID = operationID;
    }

    if (outErrorCode) {
        *outErrorCode = errorCode;
    }

    return true;
}

// init the runcam device, it'll search the UART port with FUNCTION_RCDEVICE id
// this function will delay 400ms in the first loop to wait the device prepared,
// as we know, there are has some camera need about 200~400ms to initialization,
// and then we can send/receive from it.
bool runcamDeviceInit(runcamDevice_t *device)
{
    serialPortFunction_e portID = FUNCTION_RCDEVICE;
    serialPortConfig_t *portConfig = findSerialPortConfig(portID);
    while (portConfig != NULL) {
        device->serialPort = openSerialPort(portConfig->identifier, portID, NULL, 115200, MODE_RXTX, SERIAL_NOT_INVERTED);

        if (device->serialPort != NULL) {
            // check the device is using rcsplit firmware 1.1(or 1.0) or 'RunCam
            // Device Protocol', so we can access it in correct way.
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

bool runcamDeviceSimulateCameraButton(runcamDevice_t *device, uint8_t operation)
{
    if (device->info.protocolVersion == RCDEVICE_PROTOCOL_RCSPLIT_VERSION) {
        if (operation < RCDEVICE_PROTOCOL_FEATURE_SIMULATE_POWER_BUTTON && operation > RCDEVICE_PROTOCOL_FEATURE_CHANGE_MODE) {
            return false;
        }
        rcsplitSendCtrlCommand(device, operation + 1);
    } else {
        runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_CAMERA_CONTROL, &operation, sizeof(operation));
    }

    return true;
}

// every time start to control the OSD menu of camera, must call this method to
// camera
bool runcamDeviceOpen5KeyOSDCableConnection(runcamDevice_t *device)
{
    uint8_t actionID = 0xFF;
    uint8_t code = 0xFF;
    bool r = runcamDeviceSend5KeyOSDCableConnectionEvent(device, RCDEVICE_PROTOCOL_5KEY_CONNECTION_OPEN, &actionID, &code);
    return r && (code == 1) && (actionID == RCDEVICE_PROTOCOL_5KEY_CONNECTION_OPEN);
}

// when the control was stop, must call this method to the camera to disconnect
// with camera.
bool runcamDeviceClose5KeyOSDCableConnection(runcamDevice_t *device)
{
    uint8_t actionID = 0xFF;
    uint8_t code = 0xFF;
    bool r = runcamDeviceSend5KeyOSDCableConnectionEvent(device, RCDEVICE_PROTOCOL_5KEY_CONNECTION_CLOSE, &actionID, &code);

    return r && (code == 1) && (actionID == RCDEVICE_PROTOCOL_5KEY_CONNECTION_CLOSE);
}

// simulate button press event of 5 key osd cable with special button
bool runcamDeviceSimulate5KeyOSDCableButtonPress(runcamDevice_t *device, uint8_t operation)
{
    if (operation == RCDEVICE_PROTOCOL_5KEY_SIMULATION_NONE) {
        return false;
    }

    if (runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_5KEY_SIMULATION_PRESS, &operation, sizeof(uint8_t), NULL, NULL)) {
        return true;
    }

    return false;
}

// simulate button release event of 5 key osd cable
bool runcamDeviceSimulate5KeyOSDCableButtonRelease(runcamDevice_t *device)
{
    return runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_5KEY_SIMULATION_RELEASE, NULL, 0, NULL, NULL);
}

// fill a region with same char on screen, this is used to DisplayPort feature
// support
void runcamDeviceDispFillRegion(runcamDevice_t *device, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t c)
{
    uint8_t paramsBuf[5];

    // fill parameters buf
    paramsBuf[0] = x;
    paramsBuf[1] = y;
    paramsBuf[2] = width;
    paramsBuf[3] = height;
    paramsBuf[4] = c;

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_FILL_REGION, paramsBuf, sizeof(paramsBuf));
}

// draw a single char on special position on screen, this is used to DisplayPort
// feature support
void runcamDeviceDispWriteChar(runcamDevice_t *device, uint8_t x, uint8_t y, uint8_t c)
{
    uint8_t paramsBuf[3];

    // fill parameters buf
    paramsBuf[0] = x;
    paramsBuf[1] = y;
    paramsBuf[2] = c;

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_WRITE_CHAR, paramsBuf, sizeof(paramsBuf));
}

// draw a string on special position on screen, this is used to DisplayPort
// feature support
void runcamDeviceDispWriteHortString(runcamDevice_t *device, uint8_t x, uint8_t y, const char *text)
{
    uint8_t textLen = strlen(text);
    if (textLen > 60) { // if text len more then 60 chars, cut it to 60
        textLen = 60;
    }

    uint8_t paramsBufLen = 3 + textLen;
    uint8_t paramsBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];

    paramsBuf[0] = paramsBufLen - 1;
    paramsBuf[1] = x;
    paramsBuf[2] = y;
    memcpy(paramsBuf + 3, text, textLen);

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_WRITE_HORT_STRING, paramsBuf, paramsBufLen);
}

void runcamDeviceDispWriteVertString(runcamDevice_t *device, uint8_t x, uint8_t y, const char *text)
{
    uint8_t textLen = strlen(text);
    if (textLen > 60) { // if text len more then 60 chars, cut it to 60
        textLen = 60;
    }

    uint8_t paramsBufLen = 3 + textLen;
    uint8_t paramsBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];

    paramsBuf[0] = paramsBufLen - 1;
    paramsBuf[1] = x;
    paramsBuf[2] = y;
    memcpy(paramsBuf + 3, text, textLen);

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_WRITE_VERT_STRING, paramsBuf, paramsBufLen);
}

void runcamDeviceDispWriteChars(runcamDevice_t *device, uint8_t *data, uint8_t datalen)
{
    uint8_t adjustedDataLen = datalen;
    if (adjustedDataLen > 60) { // if data len more then 60 chars, cut it to 60
        adjustedDataLen = 60;
    }

    uint8_t paramsBufLen = adjustedDataLen + 1;
    uint8_t paramsBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];

    paramsBuf[0] = adjustedDataLen;
    memcpy(paramsBuf + 1, data, adjustedDataLen);

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_WRITE_CHARS, paramsBuf, paramsBufLen);
}

static bool runcamDeviceDecodeSettings(sbuf_t *buf, runcamDeviceSetting_t outSettingList[RCDEVICE_PROTOCOL_MAX_MENUITEM_PER_PAGE])
{
    if (outSettingList == NULL) {
        return false;
    }

    runcamDeviceSettingParseStep_e parseStep = RCDP_SETTING_PARSE_WAITING_ID;
    runcamDeviceSetting_t *settingIterator = outSettingList;
    int i = 0;
    while (sbufBytesRemaining(buf) > 0) {
        if (i >= RCDEVICE_PROTOCOL_MAX_MENUITEM_PER_PAGE) {
            break;
        }

        switch (parseStep) {
        case RCDP_SETTING_PARSE_WAITING_ID: {
            uint8_t c = sbufReadU8(buf);
            settingIterator->id = c;
            parseStep = RCDP_SETTING_PARSE_WAITING_NAME;
        } break;
        case RCDP_SETTING_PARSE_WAITING_NAME: {
            const char *str = (const char *)sbufConstPtr(buf);
            uint8_t nameLen = strlen(str) + 1;
            memset(settingIterator->name, 0, RCDEVICE_PROTOCOL_MAX_SETTING_NAME_LENGTH);
            strcpy(settingIterator->name, str);
            sbufAdvance(buf, nameLen);
            parseStep = RCDP_SETTING_PARSE_WAITING_VALUE;
        } break;
        case RCDP_SETTING_PARSE_WAITING_VALUE: {
            const char *str = (const char *)sbufConstPtr(buf);
            uint8_t valueLen = strlen(str) + 1;
            memset(settingIterator->value, 0, RCDEVICE_PROTOCOL_MAX_SETTING_VALUE_LENGTH);
            strcpy(settingIterator->value, str);
            sbufAdvance(buf, valueLen);
            parseStep = RCDP_SETTING_PARSE_WAITING_ID;

            settingIterator++;
            i++;
        } break;
        }
    }

    if (RCDP_SETTING_PARSE_WAITING_ID != parseStep) {
        return false;
    }

    return true;
}

// get settings with parent setting id, the type of parent setting must be a
// FOLDER after this function called, the settings will fill into outSettingList
// argument
bool runcamDeviceGetSettings(runcamDevice_t *device, uint8_t parentSettingID, runcamDeviceSetting_t outSettingList[RCDEVICE_PROTOCOL_MAX_MENUITEM_PER_PAGE])
{
    uint8_t paramsBuf[2];
    uint8_t chunkIndex = 0;

    if (outSettingList == NULL) {
        return false;
    }

    // fill parameters buf
    paramsBuf[0] = parentSettingID; // parent setting id
    paramsBuf[1] = chunkIndex;      // chunk index

    uint8_t outputBufLen = RCDEVICE_PROTOCOL_MAX_DATA_SIZE;
    uint8_t outputBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];
    bool result = runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_GET_SETTINGS, paramsBuf, sizeof(paramsBuf), outputBuf, &outputBufLen);

    if (!result) {
        return false;
    }

    uint8_t remainingChunk = outputBuf[0];
    // Every response chunk count must less than or equal to RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE
    if ((remainingChunk + 1) > RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE) {
        return false;
    }

    // save setting data to sbuf_t object
    const uint16_t maxDataLen = (RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE - 1) * RCDEVICE_PROTOCOL_MAX_DATA_SIZE;
    uint8_t data[RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE];
    sbuf_t dataBuf;
    dataBuf.ptr = data;
    dataBuf.end = data + maxDataLen;
    sbufWriteData(&dataBuf, outputBuf + 2, outputBufLen - 3);

    // get the remaining chunks
    while (remainingChunk > 0) {
        paramsBuf[1] = ++chunkIndex; // chunk index

        result = runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_GET_SETTINGS, paramsBuf, sizeof(paramsBuf), outputBuf, &outputBufLen);

        if (!result) {
            return false;
        }

        // append the trailing chunk to the sbuf_t object,
        // but only append the actually setting data
        sbufWriteData(&dataBuf, outputBuf + 2, outputBufLen - 3);

        remainingChunk--;
    }

    // parse the settings data and convert them into a runcamDeviceSetting_t
    // list
    sbufSwitchToReader(&dataBuf, (uint8_t *)data);
    if (runcamDeviceDecodeSettings(&dataBuf, outSettingList)) {
        return false;
    }

    return true;
}

static bool runcamDeviceDecodeSettingDetail(sbuf_t *buf, runcamDeviceSettingDetail_t *outSettingDetail)
{
    if (outSettingDetail == NULL || sbufBytesRemaining(buf) == 0) {
        return false;
    }

    memset(outSettingDetail, 0, sizeof(runcamDeviceSettingDetail_t));
    rcdeviceSettingType_e settingType = sbufReadU8(buf);
    outSettingDetail->type = settingType;

    switch (settingType) {
    case RCDEVICE_PROTOCOL_SETTINGTYPE_UINT8:
    case RCDEVICE_PROTOCOL_SETTINGTYPE_INT8: {
        uint8_t value = sbufReadU8(buf);
        uint8_t minValue = sbufReadU8(buf);
        uint8_t maxValue = sbufReadU8(buf);
        uint8_t stepSize = sbufReadU8(buf);

        outSettingDetail->value = value;
        outSettingDetail->minValue = minValue;
        outSettingDetail->maxValue = maxValue;
        outSettingDetail->stepSize = stepSize;
    } break;
    case RCDEVICE_PROTOCOL_SETTINGTYPE_UINT16:
    case RCDEVICE_PROTOCOL_SETTINGTYPE_INT16: {
        outSettingDetail->value = sbufReadU16(buf);
        outSettingDetail->minValue = sbufReadU16(buf);
        outSettingDetail->maxValue = sbufReadU16(buf);
        outSettingDetail->stepSize = sbufReadU8(buf);
    } break;
    case RCDEVICE_PROTOCOL_SETTINGTYPE_FLOAT: {
        outSettingDetail->minValue = sbufReadU8(buf);
        outSettingDetail->maxValue = sbufReadU8(buf);
        outSettingDetail->decimalPoint = sbufReadU16(buf);
        outSettingDetail->stepSize = sbufReadU8(buf);
    } break;
    case RCDEVICE_PROTOCOL_SETTINGTYPE_TEXT_SELECTION: {
        outSettingDetail->value = sbufReadU8(buf);

        const char *tmp = (const char *)sbufConstPtr(buf);
        char textSels[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];
        memset(textSels, 0, sizeof(RCDEVICE_PROTOCOL_MAX_DATA_SIZE));
        strcpy(textSels, tmp);
        char delims[] = ";";
        char *result = strtok(textSels, delims);
        int i = 0;
        runcamDeviceSettingTextSelection_t *iterator = outSettingDetail->textSelections;
        while (result != NULL) {
            if (i >= RCDEVICE_PROTOCOL_MAX_TEXT_SELECTIONS) {
                break;
            }

            memset(iterator->text, 0, RCDEVICE_PROTOCOL_MAX_SETTING_VALUE_LENGTH);
            strcpy(iterator->text, result);
            iterator++;
            result = strtok(NULL, delims);
        }
    } break;
    case RCDEVICE_PROTOCOL_SETTINGTYPE_STRING: {
        uint8_t maxStringSize = sbufReadU8(buf);
        outSettingDetail->maxStringSize = maxStringSize;
    } break;
    case RCDEVICE_PROTOCOL_SETTINGTYPE_FOLDER:
    case RCDEVICE_PROTOCOL_SETTINGTYPE_INFO:
        break;
    case RCDEVICE_PROTOCOL_SETTINGTYPE_UNKNOWN:
        break;
    }

    return true;
}

// get the setting details with setting id
// after this function called, the setting detail will fill into
// outSettingDetail argument
bool runcamDeviceGetSettingDetail(runcamDevice_t *device, uint8_t settingID, runcamDeviceSettingDetail_t *outSettingDetail)
{
    uint8_t paramsBuf[2];
    uint8_t chunkIndex = 0;

    // fill parameters buf
    paramsBuf[0] = settingID;  // setting id
    paramsBuf[1] = chunkIndex; // chunk index

    uint8_t outputBufLen = RCDEVICE_PROTOCOL_MAX_DATA_SIZE;
    uint8_t outputBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];
    bool result = runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_READ_SETTING_DETAIL, paramsBuf, sizeof(paramsBuf), outputBuf, &outputBufLen);

    if (!result) {
        return false;
    }

    // save setting data to sbuf_t object
    uint8_t remainingChunk = outputBuf[0];
    if ((remainingChunk + 1) > RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE) {
        return false;
    }

    const uint16_t maxDataLen = RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE * RCDEVICE_PROTOCOL_MAX_DATA_SIZE;
    uint8_t data[maxDataLen];
    sbuf_t dataBuf;
    dataBuf.ptr = data;
    dataBuf.end = data + maxDataLen;
    sbufWriteData(&dataBuf, outputBuf + 1, outputBufLen - 1);

    // get the remaining chunks
    while (remainingChunk > 0) {
        paramsBuf[1] = ++chunkIndex; // chunk index

        result = runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_READ_SETTING_DETAIL, paramsBuf, sizeof(paramsBuf), outputBuf, &outputBufLen);

        if (!result) {
            return false;
        }

        // append the trailing chunk to the sbuf_t object
        sbufWriteData(&dataBuf, outputBuf + 1, outputBufLen - 1);
        remainingChunk--;
    }

    // parse the settings data and convert them into a runcamDeviceSetting_t
    // list
    sbufSwitchToReader(&dataBuf, (uint8_t *)data);
    if (!runcamDeviceDecodeSettingDetail(&dataBuf, outSettingDetail)) {
        return false;
    }

    return true;
}

// write new value with to the setting
bool runcamDeviceWriteSetting(runcamDevice_t *device, uint8_t settingID, uint8_t *paramData, uint8_t paramDataLen, runcamDeviceWriteSettingResponse_t *response)
{
    if (response == NULL) {
        return false;
    }

    uint8_t paramsBufLen = sizeof(uint8_t) + paramDataLen;
    uint8_t paramsBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];
    paramsBuf[0] = settingID;
    memcpy(paramsBuf + 1, paramData, paramDataLen);

    uint8_t outputBufLen = RCDEVICE_PROTOCOL_MAX_DATA_SIZE;
    uint8_t outputBuf[RCDEVICE_PROTOCOL_MAX_DATA_SIZE];
    bool result = runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_WRITE_SETTING, paramsBuf, paramsBufLen, outputBuf, &outputBufLen);

    if (!result) {
        return false;
    }

    // save setting data to sbuf_t object
    uint8_t remainingChunk = outputBuf[0];
    if ((remainingChunk + 1) > RCDEVICE_PROTOCOL_MAX_CHUNK_PER_RESPONSE) {
        return false;
    }

    response->resultCode = outputBuf[0];
    response->needUpdateMenuItems = outputBuf[1];

    return true;
}
