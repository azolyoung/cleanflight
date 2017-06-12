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
    #include <ctype.h>

    #include "platform.h"

    #include "common/utils.h"
    #include "common/maths.h"

    #include "config/parameter_group.h"
    #include "config/parameter_group_ids.h"

    #include "fc/rc_controls.h"

    #include "io/beeper.h"
    #include "io/serial.h"

    #include "drivers/serial.h"
    #include "rcsplit/rcsplit.h"

    #include "rx/rx.h"

    int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];     // interval [1000;2000]

    // PG_DECLARE_ARRAY(modeActivationCondition_t, MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT, rcsplitModeActivationConditions);
}

typedef struct testData_s {
    bool isRunCamSplitPortConfigurated;
    bool isRunCamSplitOpenPortSupported;
    int8_t maxTimesOfRespDataAvailable;
    bool isAllowBufferReadWrite;
} testData_t;

static testData_t testData;

TEST(RCSplitTest, TestRCSplitInitWithoutPortConfigurated)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    bool result = rcsplitInit();
    EXPECT_EQ(false, result);
    EXPECT_EQ(RCSPLIT_STATE_UNKNOWN, unitTestRCsplitState());
}

TEST(RCSplitTest, TestRCSplitInitWithoutOpenPortConfigurated)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = false;
    testData.isRunCamSplitPortConfigurated = true;

    bool result = rcsplitInit();
    EXPECT_EQ(false, result);
    EXPECT_EQ(RCSPLIT_STATE_UNKNOWN, unitTestRCsplitState());
}

TEST(RCSplitTest, TestRCSplitInit)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;

    bool result = rcsplitInit();
    EXPECT_EQ(true, result);
    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());
}

TEST(RCSplitTest, TestRecvWhoAreYouResponse)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    
    bool result = rcsplitInit();
    EXPECT_EQ(true, result);

    // here will generate a number in [6-255], it's make the serialRxBytesWaiting() and serialRead() run at least 5 times, 
    // so the "who are you response" will full received, and cause the state change to RCSPLIT_STATE_IS_READY;
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    rcsplitProcess((timeUs_t)0);

    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());
}

TEST(RCSplitTest, TestWifiModeChangeWithDeviceUnready)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = rcsplitInit();
    EXPECT_EQ(true, result);

    // bind aux1, aux2, aux3 channel to wifi button, power button and change mode
    // modeActivationCondition_t rcsplitModeActivationConditions[MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT];

    // memset(rcsplitModeActivationConditions, 0, sizeof(modeActivationCondition_t) * MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT);
    for (int i = 0; i < MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        memset(rcsplitModeActivationConditionsMutable(i), 0, sizeof(modeActivationCondition_t));
    }

    // bind aux1 to wifi button with range [900,1600]
    rcsplitModeActivationConditionsMutable(0)->auxChannelIndex = 0;
    rcsplitModeActivationConditionsMutable(0)->modeId = (boxId_e)RCSPLIT_BOX_SIM_WIFI_BUTTON;
    rcsplitModeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MIN);
    rcsplitModeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // bind aux2 to power button with range [1900, 2100]
    rcsplitModeActivationConditionsMutable(1)->auxChannelIndex = 1;
    rcsplitModeActivationConditionsMutable(1)->modeId = (boxId_e)RCSPLIT_BOX_SIM_POWER_BUTTON;
    rcsplitModeActivationConditionsMutable(1)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    rcsplitModeActivationConditionsMutable(1)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // bind aux3 to change mode with range [1300, 1600]
    rcsplitModeActivationConditionsMutable(2)->auxChannelIndex = 2;
    rcsplitModeActivationConditionsMutable(2)->modeId = (boxId_e)RCSPLIT_BOX_SIM_CHANGE_MODE;
    rcsplitModeActivationConditionsMutable(2)->range.startStep = CHANNEL_VALUE_TO_STEP(1300);
    rcsplitModeActivationConditionsMutable(2)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // make the binded mode inactive
    rcData[rcsplitModeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1800;
    rcData[rcsplitModeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 900;
    rcData[rcsplitModeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 900;

    updateActivatedModes();

    // runn process loop
    rcsplitProcess(0);

    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_WIFI_BUTTON));
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_POWER_BUTTON));
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_CHANGE_MODE));
}

TEST(RCSplitTest, TestWifiModeChangeWithDeviceReady)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = rcsplitInit();
    EXPECT_EQ(true, result);

    // bind aux1, aux2, aux3 channel to wifi button, power button and change mode
    // modeActivationCondition_t rcsplitModeActivationConditions[MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT];

    // memset(rcsplitModeActivationConditions, 0, sizeof(modeActivationCondition_t) * MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT);
    for (int i = 0; i < MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        memset(rcsplitModeActivationConditionsMutable(i), 0, sizeof(modeActivationCondition_t));
    }
    

    // bind aux1 to wifi button with range [900,1600]
    rcsplitModeActivationConditionsMutable(0)->auxChannelIndex = 0;
    rcsplitModeActivationConditionsMutable(0)->modeId = (boxId_e)RCSPLIT_BOX_SIM_WIFI_BUTTON;
    rcsplitModeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MIN);
    rcsplitModeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // bind aux2 to power button with range [1900, 2100]
    rcsplitModeActivationConditionsMutable(1)->auxChannelIndex = 1;
    rcsplitModeActivationConditionsMutable(1)->modeId = (boxId_e)RCSPLIT_BOX_SIM_POWER_BUTTON;
    rcsplitModeActivationConditionsMutable(1)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    rcsplitModeActivationConditionsMutable(1)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // bind aux3 to change mode with range [1300, 1600]
    rcsplitModeActivationConditionsMutable(2)->auxChannelIndex = 2;
    rcsplitModeActivationConditionsMutable(2)->modeId = (boxId_e)RCSPLIT_BOX_SIM_CHANGE_MODE;
    rcsplitModeActivationConditionsMutable(2)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    rcsplitModeActivationConditionsMutable(2)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // // make the binded mode inactive
    rcData[rcsplitModeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;
    rcData[rcsplitModeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2000;
    rcData[rcsplitModeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;

    updateActivatedModes();

    // runn process loop
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    rcsplitProcess((timeUs_t)0);

    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());

    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_WIFI_BUTTON));
    EXPECT_EQ(true, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_POWER_BUTTON));
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_CHANGE_MODE));
}

TEST(RCSplitTest, TestWifiModeChangeCombine)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = rcsplitInit();
    EXPECT_EQ(true, result);

    // bind aux1, aux2, aux3 channel to wifi button, power button and change mode
    // modeActivationCondition_t rcsplitModeActivationConditions[MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT];

    // memset(rcsplitModeActivationConditions, 0, sizeof(modeActivationCondition_t) * MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT);
    for (int i = 0; i < MAX_RC_SPLIT_MODE_ACTIVATION_CONDITION_COUNT; i++) {
        memset(rcsplitModeActivationConditionsMutable(i), 0, sizeof(modeActivationCondition_t));
    }
    

    // bind aux1 to wifi button with range [900,1600]
    rcsplitModeActivationConditionsMutable(0)->auxChannelIndex = 0;
    rcsplitModeActivationConditionsMutable(0)->modeId = (boxId_e)RCSPLIT_BOX_SIM_WIFI_BUTTON;
    rcsplitModeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(CHANNEL_RANGE_MIN);
    rcsplitModeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(1600);

    // bind aux2 to power button with range [1900, 2100]
    rcsplitModeActivationConditionsMutable(1)->auxChannelIndex = 1;
    rcsplitModeActivationConditionsMutable(1)->modeId = (boxId_e)RCSPLIT_BOX_SIM_POWER_BUTTON;
    rcsplitModeActivationConditionsMutable(1)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    rcsplitModeActivationConditionsMutable(1)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // bind aux3 to change mode with range [1300, 1600]
    rcsplitModeActivationConditionsMutable(2)->auxChannelIndex = 2;
    rcsplitModeActivationConditionsMutable(2)->modeId = (boxId_e)RCSPLIT_BOX_SIM_CHANGE_MODE;
    rcsplitModeActivationConditionsMutable(2)->range.startStep = CHANNEL_VALUE_TO_STEP(1900);
    rcsplitModeActivationConditionsMutable(2)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // // make the binded mode inactive
    rcData[rcsplitModeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;
    rcData[rcsplitModeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2000;
    rcData[rcsplitModeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1700;
    updateActivatedModes();

    // runn process loop
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    rcsplitProcess((timeUs_t)0);

    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());

    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_WIFI_BUTTON));
    EXPECT_EQ(true, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_POWER_BUTTON));
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_CHANGE_MODE));


    // // make the binded mode inactive
    rcData[rcsplitModeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1500;
    rcData[rcsplitModeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1300;
    rcData[rcsplitModeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1900;
    updateActivatedModes();
    rcsplitProcess((timeUs_t)0);
    EXPECT_EQ(true, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_WIFI_BUTTON));
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_POWER_BUTTON));
    EXPECT_EQ(true, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_CHANGE_MODE));


    rcData[rcsplitModeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1899;
    updateActivatedModes();
    rcsplitProcess((timeUs_t)0);
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_CHANGE_MODE));

    rcData[rcsplitModeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2001;
    updateActivatedModes();
    rcsplitProcess((timeUs_t)0);
    EXPECT_EQ(true, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_WIFI_BUTTON));
    EXPECT_EQ(true, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_POWER_BUTTON));
    EXPECT_EQ(false, unitTestIsSwitchActivited(RCSPLIT_BOX_SIM_CHANGE_MODE));
}

TEST(RCSplitTest, TestFindRCSplitBoxByBoxId)
{
    const box_t *wifiButtonBox = findRCSplitBoxByBoxId(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    EXPECT_EQ(RCSPLIT_BOX_SIM_WIFI_BUTTON, wifiButtonBox->boxId);

    const box_t *powerButtonBox = findRCSplitBoxByBoxId(RCSPLIT_BOX_SIM_POWER_BUTTON);
    EXPECT_EQ(RCSPLIT_BOX_SIM_POWER_BUTTON, powerButtonBox->boxId);

    const box_t *changeModeButtonBox = findRCSplitBoxByBoxId(RCSPLIT_BOX_SIM_CHANGE_MODE);
    EXPECT_EQ(RCSPLIT_BOX_SIM_CHANGE_MODE, changeModeButtonBox->boxId);

    const box_t *unknownBox = findRCSplitBoxByBoxId((rcsplitBoxId_e)100);
    EXPECT_EQ(NULL, unknownBox);

    const box_t *unknownBox2 = findRCSplitBoxByBoxId(RCSPLIT_CHECKBOX_ITEM_COUNT);
    EXPECT_EQ(NULL, unknownBox2);

    const box_t *unknownBox3 = findRCSplitBoxByBoxId(RCSPLIT_BOX_INVALID);
    EXPECT_EQ(NULL, unknownBox3);
}

TEST(RCSplitTest, TestFindRCSplitBoxByPermanentId)
{
    const box_t *wifiButtonBox = findRCSplitBoxByPermanentId(0);
    EXPECT_EQ(RCSPLIT_BOX_SIM_WIFI_BUTTON, wifiButtonBox->boxId);

    const box_t *powerButtonBox = findRCSplitBoxByPermanentId(1);
    EXPECT_EQ(RCSPLIT_BOX_SIM_POWER_BUTTON, powerButtonBox->boxId);

    const box_t *changeModeButtonBox = findRCSplitBoxByPermanentId(2);
    EXPECT_EQ(RCSPLIT_BOX_SIM_CHANGE_MODE, changeModeButtonBox->boxId);

    const box_t *unknownBox = findRCSplitBoxByPermanentId(3);
    EXPECT_EQ(NULL, unknownBox);

    const box_t *unknownBox2 = findRCSplitBoxByPermanentId(4);
    EXPECT_EQ(NULL, unknownBox2);

    const box_t *unknownBox3 = findRCSplitBoxByPermanentId(5);
    EXPECT_EQ(NULL, unknownBox3);
}

TEST(RCSplitTest, TestSerializeRCSplitBoxNamesReply)
{
    memset(&testData, 0, sizeof(testData));
    testData.isAllowBufferReadWrite = true;
    
    sbuf_t buf;
    uint8_t *base = (uint8_t*)malloc(1024 * sizeof(char));
    buf.ptr = base;

    uint32_t ena = 0;
#define BME(boxId) do { ena |= (1 << (boxId)); } while(0)
    

    BME(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    BME(RCSPLIT_BOX_SIM_POWER_BUTTON);
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxNamesReply(&buf);
    sbufSwitchToReader(&buf, base);
    EXPECT_STREQ("Wi-Fi Button;Power Button;Change Mode;", (const char*)buf.ptr);
    free(base);


    base = (uint8_t*)malloc(1024);
    memset(base, 0, 1024);
    buf.ptr = base;
    ena = 0;
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxNamesReply(&buf);
    sbufSwitchToReader(&buf, base);
    EXPECT_STREQ("Change Mode;", (const char*)buf.ptr);

    base = (uint8_t*)malloc(1024 * sizeof(char));
    memset(base, 0, 1024);
    buf.ptr = base;
    ena = 0;
    BME(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxNamesReply(&buf);
    sbufSwitchToReader(&buf, base);
    EXPECT_STREQ("Wi-Fi Button;Change Mode;", (const char*)buf.ptr);

    base = (uint8_t*)malloc(1024 * sizeof(char));
    memset(base, 0, 1024);
    buf.ptr = base;
    ena = 0;
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    BME(RCSPLIT_BOX_SIM_POWER_BUTTON);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxNamesReply(&buf);
    sbufSwitchToReader(&buf, base);
    EXPECT_STREQ("Power Button;Change Mode;", (const char*)buf.ptr);
}

TEST(RCSplitTest, TestSerializeRCSplitBoxIdsReply)
{
    memset(&testData, 0, sizeof(testData));
    testData.isAllowBufferReadWrite = true;
    
    sbuf_t buf;
    uint8_t *base = (uint8_t*)malloc(1024 * sizeof(char));
    buf.ptr = base;

    uint32_t ena = 0;
#define BME(boxId) do { ena |= (1 << (boxId)); } while(0)
    

    BME(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    BME(RCSPLIT_BOX_SIM_POWER_BUTTON);
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxIdsReply(&buf);
    sbufSwitchToReader(&buf, base);
    uint8_t expectedData1[3] = { 0, 1, 2 };
    EXPECT_EQ(expectedData1[0], buf.ptr[0]);
    EXPECT_EQ(expectedData1[1], buf.ptr[1]);
    EXPECT_EQ(expectedData1[2], buf.ptr[2]);
    free(base);


    base = (uint8_t*)malloc(1024);
    memset(base, 0, 1024);
    buf.ptr = base;
    ena = 0;
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxIdsReply(&buf);
    sbufSwitchToReader(&buf, base);
    uint8_t expectedData2[1] = { 2 };
    EXPECT_EQ(expectedData2[0], buf.ptr[0]);


    base = (uint8_t*)malloc(1024 * sizeof(char));
    memset(base, 0, 1024);
    buf.ptr = base;
    ena = 0;
    BME(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxIdsReply(&buf);
    sbufSwitchToReader(&buf, base);
    uint8_t expectedData3[2] = { 0, 2 };
    EXPECT_EQ(expectedData3[0], buf.ptr[0]);
    EXPECT_EQ(expectedData3[1], buf.ptr[1]);

    base = (uint8_t*)malloc(1024 * sizeof(char));
    memset(base, 0, 1024);
    buf.ptr = base;
    ena = 0;
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);
    BME(RCSPLIT_BOX_SIM_POWER_BUTTON);
    unitTestUpdateActiveBoxIds(ena);
    serializeRCSplitBoxIdsReply(&buf);
    sbufSwitchToReader(&buf, base);
    uint8_t expectedData4[2] = { 1, 2 };

    EXPECT_EQ(expectedData4[0], buf.ptr[0]);
    EXPECT_EQ(expectedData4[1], buf.ptr[1]);
}

TEST(RCSplitTest, TestInitActiveBoxIds) {
    uint32_t ena = 0;
#define BME(boxId) do { ena |= (1 << (boxId)); } while(0)
    

    BME(RCSPLIT_BOX_SIM_WIFI_BUTTON);
    BME(RCSPLIT_BOX_SIM_POWER_BUTTON);
    BME(RCSPLIT_BOX_SIM_CHANGE_MODE);

    EXPECT_EQ(ena, unitTestGetActiveBoxIds());
}

extern "C" {
    serialPort_t *openSerialPort(serialPortIdentifier_e identifier, serialPortFunction_e functionMask, serialReceiveCallbackPtr callback, uint32_t baudRate, portMode_t mode, portOptions_t options)
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
            return 1;
        }

        return 0;
    }

    uint8_t serialRead(serialPort_t *instance) 
    { 
        UNUSED(instance); 

        if (testData.maxTimesOfRespDataAvailable > 0) {
            static uint8_t i = 0;
            static uint8_t buffer[] = { 0x55, 0x01, 0xFF, 0xad, 0xaa };

            if (i >= 5) {
                i = 0;
            }

            return buffer[i++];
        }

        return 0; 
    }

    bool isRangeActive(uint8_t auxChannelIndex, const channelRange_t *range) {
        
        if (!IS_RANGE_USABLE(range)) {
            return false;
        }

        const uint16_t channelValue = constrain(rcData[auxChannelIndex + NON_AUX_CHANNEL_COUNT], CHANNEL_RANGE_MIN, CHANNEL_RANGE_MAX - 1);
        bool val = (channelValue >= 900 + (range->startStep * 25) &&
                channelValue < 900 + (range->endStep * 25));

        return val;
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
    
    void serialWriteBuf(serialPort_t *instance, const uint8_t *data, int count) { UNUSED(instance); UNUSED(data); UNUSED(count); }
}