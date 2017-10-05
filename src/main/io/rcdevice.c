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

// Parameters:
//      data: the data that has received
//      dataLen: the data len that has received
//      isDone: output parameter, it is used to indicator the response data has fully received
// Return: If the packet verify failed, it'll return false, else it will return true even the packet still is receiving.
typedef uint8_t (*runcamDeviceVariableLengthPacketsVerify)(uint8_t *data, uint8_t dataLen, bool *isDone);

// Parse the variable length response, e.g the response of settings data and the detail of setting
static uint8_t runcamDeviceIsVariableLengthResponseReceiveDone(uint8_t *data, uint8_t dataLen, bool *isDone)
{
    if (isDone == NULL) {
        return false;
    }

    uint8_t settingDataLength = 0x00;
    // get setting datalen first
    if (dataLen >= 2) {
        settingDataLength = data[1];

        if (dataLen >= (settingDataLength + 3)) {
            *isDone = true;
            return true;
        }
    }

    if (settingDataLength > 60) {
        return false;
    }


    return true;
}

// a common way to receive packet and verify it
static uint8_t runcamDeviceReceivePacket(runcamDevice_t *device, uint8_t expectedDataLen, uint8_t *data, runcamDeviceVariableLengthPacketsVerify isVariableLengthtResponseReceiveDoneCallback)
{
    uint8_t dataPos = 0;
    uint8_t crc = crc8_dvb_s2(0, RCDEVICE_PROTOCOL_HEADER);
    uint8_t result = RCDEVICE_PROTOCOL_MAX_DATA_SIZE_WITH_CRC_FIELD;

    // wait 1000ms for reply
    timeMs_t timeout = millis() + 1000;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            uint8_t c = serialRead(device->serialPort);
            crc = crc8_dvb_s2(crc, c);
            if (data) {
                data[dataPos] = c;
            }

            dataPos++;

            if (isVariableLengthtResponseReceiveDoneCallback) {
                bool isDone = false;
                if (!isVariableLengthtResponseReceiveDoneCallback(data, dataPos, &isDone)) {
                    return 0;
                }

                if (isDone) {
                    result = dataPos;
                    break;
                }
            }
        }

        // if it has been read enough data, than break
        if (dataPos >= expectedDataLen) {
            break;
        }
    }

    // check crc
    if (crc != 0 || (isVariableLengthtResponseReceiveDoneCallback == NULL && dataPos < expectedDataLen)) {
        return 0;
    }

    return result;
}

// decode the device info
static bool runcamDeviceReceiveDeviceInfo(runcamDevice_t *device)
{
    const uint8_t expectedDataLen = 4;
    uint8_t data[expectedDataLen];

    if (!runcamDeviceReceivePacket(device, expectedDataLen, data, NULL)) {
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
    return runcamDeviceReceivePacket(device, 1, NULL, NULL);
}

// decode the write setting response
static bool runcamDeviceReceiveWriteSettingResponse(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    const uint8_t expectedDataLen = 3;
    uint8_t data[expectedDataLen];
    if (!runcamDeviceReceivePacket(device, expectedDataLen, data, NULL)) {
        return false;
    }

    uint8_t errorCode = data[0];
    if (errorCode != 0) {
        return false;
    }

    if (outputBufferLen && (*outputBufferLen >= expectedDataLen) && outputBuffer) {
        memcpy(outputBuffer, data, expectedDataLen);
        *outputBufferLen = expectedDataLen;
    }

    return true;
}

// decode the connection event response
static bool runcamDeviceReceiveConnectionEventResponse(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    const uint8_t expectedDataLen = 2;
    uint8_t data[expectedDataLen];
    if (!runcamDeviceReceivePacket(device, expectedDataLen, data, NULL)) {
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
    uint8_t data[RCDEVICE_PROTOCOL_MAX_DATA_SIZE_WITH_CRC_FIELD];
    uint8_t receivedDataLen = runcamDeviceReceivePacket(device, RCDEVICE_PROTOCOL_MAX_DATA_SIZE_WITH_CRC_FIELD, data, runcamDeviceIsVariableLengthResponseReceiveDone);
    if (!receivedDataLen) {
        return false;
    }

    if (outputBufferLen && (*outputBufferLen >= (receivedDataLen - 1)) && outputBuffer) {
        memcpy(outputBuffer, data, receivedDataLen - 1); // return the data expect the crc field
        *outputBufferLen = receivedDataLen - 1;
    }

    return true;
}

// decode the device setting response
static bool runcamDeviceReceiveSettingDetail(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    uint8_t data[RCDEVICE_PROTOCOL_MAX_DATA_SIZE_WITH_CRC_FIELD];
    uint8_t receivedDataLen = runcamDeviceReceivePacket(device, RCDEVICE_PROTOCOL_MAX_DATA_SIZE_WITH_CRC_FIELD, data, runcamDeviceIsVariableLengthResponseReceiveDone);
    if (!receivedDataLen) {
        return false;
    }

    if (outputBufferLen && (*outputBufferLen >= (receivedDataLen - 1)) && outputBuffer) {
        memcpy(outputBuffer, data, receivedDataLen - 1); // return the data expect the crc field
        *outputBufferLen = receivedDataLen - 1;
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
    serialWriteBuf(device->serialPort, sbufPtr(&buf), sbufBytesRemaining(&buf));
}

// a common way to receive data from device, and will decode the response,
// and save the response data(all the fields from response except the header and
// crc field) to outputBuffer if the outputBuffer not a NULL pointer.
static bool runcamDeviceSerialReceive(runcamDevice_t *device, uint8_t command, uint8_t *outputBuffer, uint8_t *outputBufferLen)
{
    // wait 1000ms for reply
    bool isWaitingHeader = true;
    timeMs_t timeout = millis() + 1000;
    while (millis() < timeout) {
        if (serialRxBytesWaiting(device->serialPort) > 0) {
            if (isWaitingHeader) {
                uint8_t c = serialRead(device->serialPort);
                if (c == RCDEVICE_PROTOCOL_HEADER) {
                    isWaitingHeader = false;
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

                return decodeResult;
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
static bool runcamDeviceGetDeviceInfo(runcamDevice_t *device, uint8_t *outputBuffer, uint8_t *outputBufferLen) 
{
    return runcamDeviceSendRequestAndWaitingResp(device, RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO, NULL, 0, outputBuffer, outputBufferLen); 
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
            // send RCDEVICE_PROTOCOL_COMMAND_GET_DEVICE_INFO to device to retrive
            // device info, e.g protocol version, supported features
            if (runcamDeviceGetDeviceInfo(device, NULL, 0)) {
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
    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_CAMERA_CONTROL, &operation, sizeof(operation));
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
void runcamDeviceDispWriteHorizontalString(runcamDevice_t *device, uint8_t x, uint8_t y, const char *text)
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

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_WRITE_HORIZONTAL_STRING, paramsBuf, paramsBufLen);
}

void runcamDeviceDispWriteVerticalString(runcamDevice_t *device, uint8_t x, uint8_t y, const char *text)
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

    runcamDeviceSendPacket(device, RCDEVICE_PROTOCOL_COMMAND_DISP_WRITE_VERTICAL_STRING, paramsBuf, paramsBufLen);
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
