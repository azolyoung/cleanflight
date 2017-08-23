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


#include "gtest/gtest.h"

extern "C" {
    #include <stdbool.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
    #include <ctype.h>
    #include <math.h>

    #include "platform.h"

    #include "common/utils.h"
    #include "common/maths.h"
    #include "common/bitarray.h"
    #include "common/printf.h"
    #include "common/crc.h"

    #include "config/parameter_group.h"
    #include "config/parameter_group_ids.h"

    #include "fc/rc_controls.h"
    #include "fc/rc_modes.h"
    

    #include "io/beeper.h"
    #include "io/serial.h"

    #include "scheduler/scheduler.h"
    #include "drivers/serial.h"
    #include "drivers/display.h"
    #include "drivers/opentco.h"
    #include "drivers/opentco_cam.h"
    #include "drivers/opentco_osd.h"

    #include "rx/rx.h"

    #include "cms/cms.h"
    #include "build/version.h"

    static opentcoDevice_t OSDDevice;
    int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];     // interval [1000;2000]
    typedef struct testData_s {
        bool isRunCamSplitPortConfigurated;
        bool isRunCamSplitOpenPortSupported;
        int8_t maxTimesOfRespDataAvailable;
        bool isAllowBufferReadWrite;
        uint8_t readPos;
        uint8_t *responseData;
        uint8_t responseDataLen;
    } testData_t;

    static testData_t testData;

    bool unitTestIsSwitchActivited(boxId_e boxId)
    {
        uint8_t adjustBoxID = boxId - BOXCAMERA1;
        opentco_cam_switch_state_t switchState = switchStates[adjustBoxID];
        return switchState.isActivated;
    }

    void unitTestResetRCSplit()
    {
        camDevice->serialPort = NULL;
    }

    uint8_t RESPONSEDATA_WITH_INIT_RESP_AND_FEATURE_RESP[] = { 0x80, 0xA0, 0x00, 0x00, 0x00, 0x9E, 0x80, 0xA0, 0x02, 0x07, 0x00, 0x7C };
    void unitTestSetDeviceToReadyStatus()
    {
        testData.readPos = 0;
        testData.maxTimesOfRespDataAvailable = sizeof(RESPONSEDATA_WITH_INIT_RESP_AND_FEATURE_RESP);
        testData.responseData = RESPONSEDATA_WITH_INIT_RESP_AND_FEATURE_RESP;
        testData.responseDataLen = sizeof(RESPONSEDATA_WITH_INIT_RESP_AND_FEATURE_RESP);
    }

    int opentcoOSDWriteString_H(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *buff);
    int opentcoOSDWriteString_V(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *buff);
}

TEST(OpenTCOCamTest, TestRCSplitInitWithoutPortConfigurated)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    bool result = opentcoCamInit();
    EXPECT_EQ(false, result);
}

TEST(OpenTCOCamTest, TestRCSplitInitWithoutOpenPortConfigurated)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = false;
    testData.isRunCamSplitPortConfigurated = true;

    bool result = opentcoCamInit();
    EXPECT_EQ(false, result);
}

TEST(OpenTCOCamTest, TestRCSplitInit)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    unitTestSetDeviceToReadyStatus();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    bool result = opentcoCamInit();
    
    opentcoCamProcess((timeUs_t)0);

    EXPECT_EQ(true, result);
}

TEST(OpenTCOCamTest, TestRecvWhoAreYouResponse)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    unitTestSetDeviceToReadyStatus();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    
    bool result = opentcoCamInit();
    EXPECT_EQ(true, result);

    // here will generate a number in [6-255], it's make the serialRxBytesWaiting() and serialRead() run at least 5 times, 
    // so the "who are you response" will full received, and cause the state change to RCSPLIT_STATE_IS_READY;
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    uint8_t responseData[] = { 0x55, 0x01, 0xFF, 0xad, 0xaa };
    testData.responseData = responseData;
    testData.responseDataLen = sizeof(responseData);

    opentcoCamProcess((timeUs_t)0);
}

TEST(OpenTCOCamTest, TestWifiModeChangeWithDeviceUnready)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = opentcoCamInit();
    EXPECT_EQ(false, result);

    // bind aux1, aux2, aux3 channel to wifi button, power button and change mode
    for (uint8_t i = 0; i <= (BOXCAMERA3 - BOXCAMERA1); i++) {
        memset(modeActivationConditionsMutable(i), 0, sizeof(modeActivationCondition_t));
    }

    // bind aux1 to wifi button with range [900,1600]
    modeActivationConditionsMutable(0)->auxChannelIndex = 0;
    modeActivationConditionsMutable(0)->modeId = BOXCAMERA1;
    modeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MIN);
    modeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // bind aux2 to power button with range [1900, 2100]
    modeActivationConditionsMutable(1)->auxChannelIndex = 1;
    modeActivationConditionsMutable(1)->modeId = BOXCAMERA2;
    modeActivationConditionsMutable(1)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    modeActivationConditionsMutable(1)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // bind aux3 to change mode with range [1300, 1600]
    modeActivationConditionsMutable(2)->auxChannelIndex = 2;
    modeActivationConditionsMutable(2)->modeId = BOXCAMERA3;
    modeActivationConditionsMutable(2)->range.startStep = CHANNEL_VALUE_TO_STEP(1300);
    modeActivationConditionsMutable(2)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // make the binded mode inactive
    rcData[modeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1500;
    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1900;
    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 900;

    updateActivatedModes();

    // runn process loop
    opentcoCamProcess(0);

    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));
}

TEST(OpenTCOCamTest, TestWifiModeChangeWithDeviceReady)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    unitTestSetDeviceToReadyStatus();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = opentcoCamInit();
    EXPECT_EQ(true, result);

    // bind aux1, aux2, aux3 channel to wifi button, power button and change mode
    for (uint8_t i = 0; i <= BOXCAMERA3 - BOXCAMERA1; i++) {
        memset(modeActivationConditionsMutable(i), 0, sizeof(modeActivationCondition_t));
    }
    

    // bind aux1 to wifi button with range [900,1600]
    modeActivationConditionsMutable(0)->auxChannelIndex = 0;
    modeActivationConditionsMutable(0)->modeId = BOXCAMERA1;
    modeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MIN);
    modeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // bind aux2 to power button with range [1900, 2100]
    modeActivationConditionsMutable(1)->auxChannelIndex = 1;
    modeActivationConditionsMutable(1)->modeId = BOXCAMERA2;
    modeActivationConditionsMutable(1)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    modeActivationConditionsMutable(1)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // bind aux3 to change mode with range [1300, 1600]
    modeActivationConditionsMutable(2)->auxChannelIndex = 2;
    modeActivationConditionsMutable(2)->modeId = BOXCAMERA3;
    modeActivationConditionsMutable(2)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    modeActivationConditionsMutable(2)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    rcData[modeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;
    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2000;
    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;

    updateActivatedModes();

    // runn process loop
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    uint8_t responseData[] = { 0x55, 0x01, 0xFF, 0xad, 0xaa };
    testData.responseData = responseData;
    testData.responseDataLen = sizeof(responseData);
    opentcoCamProcess((timeUs_t)0);

    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));
}

TEST(OpenTCOCamTest, TestWifiModeChangeCombine)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    unitTestSetDeviceToReadyStatus();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = opentcoCamInit();
    EXPECT_EQ(true, result);

    // bind aux1, aux2, aux3 channel to wifi button, power button and change mode
    for (uint8_t i = 0; i <= BOXCAMERA3 - BOXCAMERA1; i++) {
        memset(modeActivationConditionsMutable(i), 0, sizeof(modeActivationCondition_t));
    }
    

    // bind aux1 to wifi button with range [900,1600]
    modeActivationConditionsMutable(0)->auxChannelIndex = 0;
    modeActivationConditionsMutable(0)->modeId = BOXCAMERA1;
    modeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MIN);
    modeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // bind aux2 to power button with range [1900, 2100]
    modeActivationConditionsMutable(1)->auxChannelIndex = 1;
    modeActivationConditionsMutable(1)->modeId = BOXCAMERA2;
    modeActivationConditionsMutable(1)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    modeActivationConditionsMutable(1)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // bind aux3 to change mode with range [1300, 1600]
    modeActivationConditionsMutable(2)->auxChannelIndex = 2;
    modeActivationConditionsMutable(2)->modeId = BOXCAMERA3;
    modeActivationConditionsMutable(2)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    modeActivationConditionsMutable(2)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // // make the binded mode inactive
    rcData[modeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;
    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2000;
    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;
    updateActivatedModes();

    // runn process loop
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    uint8_t responseData[] = { 0x55, 0x01, 0xFF, 0xad, 0xaa };
    testData.responseData = responseData;
    testData.responseDataLen = sizeof(responseData);
    opentcoCamProcess((timeUs_t)0);

    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));


    // // make the binded mode inactive
    rcData[modeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1500;
    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1300;
    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1900;
    updateActivatedModes();
    opentcoCamProcess((timeUs_t)0);
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA3));


    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1899;
    updateActivatedModes();
    opentcoCamProcess((timeUs_t)0);
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));

    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2001;
    updateActivatedModes();
    opentcoCamProcess((timeUs_t)0);
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));
}

TEST(OpenTCOCamTest, TestOpenTcoPacket)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    unitTestSetDeviceToReadyStatus();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    testData.isAllowBufferReadWrite = true;

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    opentcoOSDWriteChar(NULL, 5, 5, 'E');
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    uint8_t bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd write char(%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    opentcoOSDFillRegion(NULL, 10, 10, 2, 2, 'A');
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd fill region(%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    opentcoOSDWriteString_H(NULL, 7, 2, "ABCDEF1234");
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd write string hort(%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    opentcoOSDWriteString_V(NULL, 3, 3, "ABCDEF1234");
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd write string vert(%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    opentcoWriteRegister(&OSDDevice, OPENTCO_OSD_REGISTER_VIDEO_FORMAT | OPENTCO_REGISTER_ACCESS_MODE_READ, 0);
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd query video format (%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    opentcoWriteRegister(&OSDDevice, OPENTCO_CAM_REGISTER_STATUS | OPENTCO_REGISTER_ACCESS_MODE_READ, 0);
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd read register status (%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_OSD;
    uint16_t camRegisterFlag = 0;
    opentcoWriteRegister(&OSDDevice, OPENTCO_CAM_REGISTER_STATUS, camRegisterFlag);
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("osd write register status (%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_CAM;
    opentcoWriteRegister(&OSDDevice, OPENTCO_CAM_REGISTER_SUPPORTED_FEATURES | OPENTCO_REGISTER_ACCESS_MODE_READ, 0);
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("cam query supported feature (%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_CAM;
    opentcoWriteRegister(&OSDDevice, OPENTCO_CAM_REGISTER_STATUS | OPENTCO_REGISTER_ACCESS_MODE_READ, 0);
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("cam read register status (%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");

    OSDDevice.id = OPENTCO_DEVICE_CAM;
    camRegisterFlag = OPENTCO_CAM_FEATURE_SIMULATE_POWER_BTN | OPENTCO_CAM_FEATURE_SIMULATE_WIFI_BTN;
    opentcoWriteRegister(&OSDDevice, OPENTCO_CAM_REGISTER_STATUS, camRegisterFlag);
    sbufSwitchToReader(OSDDevice.sbuf, OSDDevice.buffer);
    bufferLen = sbufBytesRemaining(OSDDevice.sbuf);
    printf("cam write register status (%d): ", bufferLen);
    for (int i = 0; i < bufferLen; i++) {
        printf("%02x ", sbufReadU8(OSDDevice.sbuf)); 
    }
    printf("\n");
    
}

extern "C" {
    serialPort_t *openSerialPort(serialPortIdentifier_e identifier, serialPortFunction_e functionMask, serialReceiveCallbackPtr callback, uint32_t baudRate, portMode_e mode, portOptions_e options)
    {
        UNUSED(identifier);
        UNUSED(functionMask);
        UNUSED(baudRate);
        UNUSED(callback);
        UNUSED(mode);
        UNUSED(options);

        if (testData.isRunCamSplitOpenPortSupported) {
            static serialPort_t s;
            s.vTable = NULL;

            // common serial initialisation code should move to serialPort::init()
            s.rxBufferHead = s.rxBufferTail = 0;
            s.txBufferHead = s.txBufferTail = 0;
            s.rxBufferSize = 0;
            s.txBufferSize = 0;
            s.rxBuffer = s.rxBuffer;
            s.txBuffer = s.txBuffer;

            // callback works for IRQ-based RX ONLY
            s.rxCallback = NULL;
            s.baudRate = 0;

            return (serialPort_t *)&s;
        }
        return NULL;
    }

    serialPortConfig_t *findSerialPortConfig(serialPortFunction_e function)
    {
        UNUSED(function);
        if (testData.isRunCamSplitPortConfigurated) {
            static serialPortConfig_t portConfig;

            portConfig.identifier = SERIAL_PORT_USART3;
            portConfig.msp_baudrateIndex = BAUD_115200;
            portConfig.gps_baudrateIndex = BAUD_57600;
            portConfig.telemetry_baudrateIndex = BAUD_AUTO;
            portConfig.blackbox_baudrateIndex = BAUD_115200;
            portConfig.functionMask = FUNCTION_MSP;

            return &portConfig;
        }

        return NULL;
    }

    uint32_t serialRxBytesWaiting(const serialPort_t *instance) 
    { 
        UNUSED(instance);

        testData.maxTimesOfRespDataAvailable--;
        if (testData.maxTimesOfRespDataAvailable > 0) {
            return testData.maxTimesOfRespDataAvailable;
        }

        return 0;
    }

    uint8_t serialRead(serialPort_t *instance) 
    { 
        UNUSED(instance); 

        if (testData.maxTimesOfRespDataAvailable > 0) {
            static uint8_t *buffer = testData.responseData;

            if (testData.readPos >= testData.responseDataLen) {
                testData.readPos = 0;
            }

            return buffer[testData.readPos++];
        }

        return 0; 
    }

    void sbufWriteString(sbuf_t *dst, const char *string) 
    { 
        UNUSED(dst); UNUSED(string); 

        if (testData.isAllowBufferReadWrite) {
            sbufWriteData(dst, string, strlen(string));
        }
    }
    void sbufWriteU8(sbuf_t *dst, uint8_t val) 
    { 
        UNUSED(dst); UNUSED(val); 

        if (testData.isAllowBufferReadWrite) {
            *dst->ptr++ = val;
        }
    }

    void sbufWriteData(sbuf_t *dst, const void *data, int len)
    {
        UNUSED(dst); UNUSED(data); UNUSED(len); 

        if (testData.isAllowBufferReadWrite) {
            memcpy(dst->ptr, data, len);
            dst->ptr += len;
            
        }
    }

    // modifies streambuf so that written data are prepared for reading
    void sbufSwitchToReader(sbuf_t *buf, uint8_t *base)
    {
        UNUSED(buf); UNUSED(base); 

        if (testData.isAllowBufferReadWrite) {
            buf->end = buf->ptr;
            buf->ptr = base;
        }
    }

    uint8_t sbufReadU8(sbuf_t *src)
    {
        if (testData.isAllowBufferReadWrite) {
            return *src->ptr++;
        }

        return 0;
    }

    void sbufAdvance(sbuf_t *buf, int size)
    {
        if (testData.isAllowBufferReadWrite) {
            buf->ptr += size;
        }
    }

    int sbufBytesRemaining(sbuf_t *buf)
    {
        if (testData.isAllowBufferReadWrite) {
            return buf->end - buf->ptr;
        }
        return 0;
    }

    const uint8_t* sbufConstPtr(const sbuf_t *buf)
    {
        return buf->ptr;
    }

    void sbufReadData(sbuf_t *src, void *data, int len)
    {
        if (testData.isAllowBufferReadWrite) {
            memcpy(data, src->ptr, len);
        }
    }

    uint16_t sbufReadU16(sbuf_t *src)
    {
        uint16_t ret;
        ret = sbufReadU8(src);
        ret |= sbufReadU8(src) << 8;
        return ret;
    }

    void sbufWriteU16(sbuf_t *dst, uint16_t val)
    {
        sbufWriteU8(dst, val >> 0);
        sbufWriteU8(dst, val >> 8);
    }

    void sbufWriteU16BigEndian(sbuf_t *dst, uint16_t val)
    {
        sbufWriteU8(dst, val >> 8);
        sbufWriteU8(dst, (uint8_t)val);
    }

    bool feature(uint32_t) { return false; }

    void serialWriteBuf(serialPort_t *instance, const uint8_t *data, int count) 
    { 
        UNUSED(instance); UNUSED(data); UNUSED(count); 
        for (const uint8_t *p = data; count > 0; count--, p++) {
            printf("111 %02x", *p);
            // serialWrite(instance, *p);
        }
    }

    void displayInit(displayPort_t *instance, const displayPortVTable_t *vTable)
    {
        UNUSED(instance); UNUSED(vTable);
    }

    void opentcoSendFrame(opentcoDevice_t *device)
    {
        UNUSED(device);
    }

    serialPortConfig_t *findNextSerialPortConfig(serialPortFunction_e function)
    {
        UNUSED(function);

        return NULL;
    }

    void closeSerialPort(serialPort_t *serialPort)
    {
        UNUSED(serialPort);
    }

    uint32_t millis(void) {return 0;}

    int opentcoOSDWriteChar(displayPort_t *displayPort, uint8_t x, uint8_t y, uint8_t c)
    {
        UNUSED(displayPort);
    
        // start frame
        opentcoInitializeFrame(&OSDDevice, OPENTCO_OSD_COMMAND_WRITE);

        // add x/y start coordinate
        sbufWriteU8(OSDDevice.sbuf, x);
        sbufWriteU8(OSDDevice.sbuf, y);
    
        // add char
        sbufWriteU8(OSDDevice.sbuf, c);
    
        crc8_dvb_s2_sbuf_append(OSDDevice.sbuf, OSDDevice.buffer);

        // done
        return 0;
    }

    int opentcoOSDFillRegion(displayPort_t *displayPort, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t value)
    {
        UNUSED(displayPort);
    
        opentcoDevice_t *device = &OSDDevice;

        // start frame
        opentcoInitializeFrame(device, OPENTCO_OSD_COMMAND_FILL_REGION);
    
        // start coordinates
        sbufWriteU8(device->sbuf, x);
        sbufWriteU8(device->sbuf, y);
    
        // width and height of region
        sbufWriteU8(device->sbuf, width);
        sbufWriteU8(device->sbuf, height);
    
        // fill value
        sbufWriteU8(device->sbuf, value);
    
        crc8_dvb_s2_sbuf_append(OSDDevice.sbuf, OSDDevice.buffer);
    
        // done
        return 0;
    }

    int opentcoOSDWriteString_H(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *buff)
    {
        UNUSED(displayPort);
    
        opentcoDevice_t *device = &OSDDevice;

        // start frame
        // FIXME: add vertical mode
        // opentcoInitializeFrame(device->sbuf, OPENTCO_DEVICE_OSD, OPENTCO_OSD_COMMAND_WRITE_BUFFER_V);
        opentcoInitializeFrame(device, OPENTCO_OSD_COMMAND_WRITE_BUFFER_H);
    
        // automatic calc of frame length
        uint8_t *lengthPtr = sbufPtr(device->sbuf);
        sbufWriteU8(device->sbuf, 0);
    
        // add x/y start coordinate
        sbufWriteU8(device->sbuf, x);
        sbufWriteU8(device->sbuf, y);
    
        // add string
        sbufWriteString(device->sbuf, buff);
    
        // add automatic length
        *lengthPtr = sbufPtr(device->sbuf) - lengthPtr - 1;
    
        // send
        crc8_dvb_s2_sbuf_append(OSDDevice.sbuf, OSDDevice.buffer);
    
        // done
        return 0;
    }

    int opentcoOSDWriteString_V(displayPort_t *displayPort, uint8_t x, uint8_t y, const char *buff)
    {
        UNUSED(displayPort);
    
        opentcoDevice_t *device = &OSDDevice;

        // start frame
        // FIXME: add vertical mode
        // opentcoInitializeFrame(device->sbuf, OPENTCO_DEVICE_OSD, OPENTCO_OSD_COMMAND_WRITE_BUFFER_V);
        opentcoInitializeFrame(device, OPENTCO_OSD_COMMAND_WRITE_BUFFER_V);
    
        // automatic calc of frame length
        uint8_t *lengthPtr = sbufPtr(device->sbuf);
        sbufWriteU8(device->sbuf, 0);
    
        // add x/y start coordinate
        sbufWriteU8(device->sbuf, x);
        sbufWriteU8(device->sbuf, y);
    
        // add string
        sbufWriteString(device->sbuf, buff);
    
        // add automatic length
        *lengthPtr = sbufPtr(device->sbuf) - lengthPtr - 1;
    
        // send
        crc8_dvb_s2_sbuf_append(OSDDevice.sbuf, OSDDevice.buffer);
    
        // done
        return 0;
    }

    void opentcoInitializeFrame(opentcoDevice_t *device, uint8_t command)
    {
        // point to the buffer
        device->sbuf = &device->streamBuffer;
    
        // prepare pointer
        device->sbuf->ptr = device->buffer;
        device->sbuf->end = ARRAYEND(device->buffer);
    
        // add frame header
        sbufWriteU8(device->sbuf, OPENTCO_PROTOCOL_HEADER);
    
        // add device & command
        sbufWriteU8(device->sbuf, ((device->id & 0x0F)<<4) | (command & 0x0F));
    }

    bool opentcoInit(opentcoDevice_t *device)
    {
        
        // scan all opentco serial ports
        serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_OPENTCO);

        while (portConfig != NULL) {
            // extract baudrate

            uint32_t baudrate = baudRates[portConfig->blackbox_baudrateIndex];
            
            // open assigned serial port
            device->serialPort = openSerialPort(portConfig->identifier, FUNCTION_OPENTCO, NULL, baudrate, MODE_RXTX, SERIAL_NOT_INVERTED);
            if (device->serialPort == NULL)
                return false;
            
            // try to detect the given device:
            uint16_t tmp;
            if (opentcoReadRegister(device, 0, &tmp)){
                // success, found port for this device
                
                return true;
            }

            // device not found, close port
            closeSerialPort(device->serialPort);
    
            // find next portConfig
            portConfig = findNextSerialPortConfig(FUNCTION_OPENTCO);
        }
    
        device->serialPort = NULL;
        return false;
    }
    
    bool opentcoDecodeResponse(opentcoDevice_t *device, uint8_t requested_reg, uint16_t *reply)
    {
        // header has been checked beforehand, test the remaining 5 bytes:
        // ... [DEVICE:4|CMD:4] [REGISTER:8] [VALUE_LO:8] [VALUE_HI:8] [CRC:8]
    
        // prepare crc calc
        uint8_t crc = crc8_dvb_s2(0, OPENTCO_PROTOCOL_HEADER);
    
        // fetch data (serial buffer already contains enough bytes)
        uint8_t data[5];
        for(int i = 0; i < 5; i++) {
            uint8_t rx = serialRead(device->serialPort);
            data[i] = rx;
            crc = crc8_dvb_s2(crc, rx);
        }
    
        // check crc
        if (crc != 0) return false;
    
        // check device and command
        uint8_t valid_devcmd = ((OPENTCO_DEVICE_RESPONSE | device->id) << 4) | OPENTCO_OSD_COMMAND_REGISTER_ACCESS;
        if (data[0] != valid_devcmd) return false;
    
        // response to our request?
        if (data[1] != requested_reg) return false;
    
        // return value
        *reply = (data[3] << 8) | data[2];
    
        return true;
    }

    bool opentcoReadRegister(opentcoDevice_t *device, uint8_t reg, uint16_t *val)
    {
        uint32_t max_retries = 3;
    
        while (max_retries--) {
            // send read request

            opentcoWriteRegister(device, reg | OPENTCO_REGISTER_ACCESS_MODE_READ, 0);
    
            // wait 100ms for reply
            timeMs_t timeout = millis() + 100;
    
            bool header_received = false;
            while (millis() < timeout) {
                // register request replies will contain 6 bytes:
                // [HEADER:8] [DEVICE:4|CMD:4] [REGISTER:8] [VALUE_LO:8] [VALUE_HI:8] [CRC:8]
                if (!header_received) {
                    // read serial bytes until we find a header:
                    if (serialRxBytesWaiting(device->serialPort) > 0) {
                        uint8_t rx = serialRead(device->serialPort);
                        if (rx == OPENTCO_PROTOCOL_HEADER) {
                            header_received = true;
                        }
                    }
                } else {
                    // header found, now wait for the remaining bytes to arrive
                    if (serialRxBytesWaiting(device->serialPort) >= 5) {
                        // try to decode this packet
                        if (!opentcoDecodeResponse(device, reg, val)) {
                            // received broken / bad response
                            break;
                        }
    
                        // received valid data
                        return true;
                    }
                }
            }
        }
    
        // failed n times
        return false;
    }
    
    bool opentcoWriteRegister(opentcoDevice_t *device, uint8_t reg, uint16_t val)
    {
        // start frame
        opentcoInitializeFrame(device, OPENTCO_OSD_COMMAND_REGISTER_ACCESS);
    
        // add register
        sbufWriteU8(device->sbuf, reg);
    
        // add value
        sbufWriteU16(device->sbuf, val);
    
        // send
        crc8_dvb_s2_sbuf_append(device->sbuf, device->buffer);
    
        //FIXME: check if actually written (read response)
        return true;
    }

    uint8_t* sbufPtr(sbuf_t *buf)
    {
        return buf->ptr;
    }

    

    const uint32_t baudRates[] = {0, 9600, 19200, 38400, 57600, 115200, 230400, 250000,
        400000, 460800, 500000, 921600, 1000000, 1500000, 2000000, 2470000}; // see baudRate_e
}