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

    #include "config/parameter_group.h"
    #include "config/parameter_group_ids.h"

    #include "fc/rc_controls.h"
    #include "fc/rc_modes.h"
    

    #include "io/beeper.h"
    #include "io/serial.h"

    #include "scheduler/scheduler.h"
    #include "drivers/serial.h"
    #include "drivers/display.h"
    #include "io/rcsplit.h"
    #include "io/rcsplit_packet_helper.h"
    #include "io/displayport_rccamera.h"

    #include "rx/rx.h"

    #include "cms/cms.h"
    #include "build/version.h"

    int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];     // interval [1000;2000]

    rcsplit_state_e unitTestRCsplitState()
    {
        return cameraState;
    }

    bool unitTestIsSwitchActivited(boxId_e boxId)
    {
        uint8_t adjustBoxID = boxId - BOXCAMERA1;
        rcsplit_switch_state_t switchState = switchStates[adjustBoxID];
        return switchState.isActivated;
    }

    void unitTestResetRCSplit()
    {
        rcSplitSerialPort = NULL;
        cameraState = RCSPLIT_STATE_UNKNOWN;
    }

    bool rcCamOSDPasrePacket(sbuf_t *src, rcsplit_packet_v2_t *outPacket)
    {
        if (src == NULL || outPacket == NULL) {
            return false;
        }

        uint16_t crcFieldOffset = 0;
        uint8_t *base = src->ptr;
        uint8_t command = 0;
        outPacket->header = sbufReadU8(src);
        command = sbufReadU8(src);
        outPacket->deviceID = command >> 4;
        if (outPacket->deviceID != RCSPLIT_OPENCTO_CAMERA_DEVICE) {
            printf("device id incorrect\n");
            return false;
        }
            

        outPacket->command = command & 0x0F;
        outPacket->dataLen = sbufReadU8(src);
        uint8_t *data = (uint8_t*)malloc(outPacket->dataLen);
        sbufReadData(src, data, outPacket->dataLen);
        sbufAdvance(src, outPacket->dataLen); 
        outPacket->data = data;

        crcFieldOffset = sbufConstPtr(src) - base;
        outPacket->crc8 = sbufReadU8(src);
        uint8_t crc = rcCamCalcPacketCRC(src, base, crcFieldOffset, sizeof(uint8_t));
        if (crc != outPacket->crc8) {
            return false;
        }

        if (outPacket->header != RCSPLIT_PACKET_HEADER) {
            return false;
        }

        sbufSwitchToReader(src, base);

        return true;
    }
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
    bool result = rcSplitInit();
    EXPECT_EQ(false, result);
    EXPECT_EQ(RCSPLIT_STATE_UNKNOWN, unitTestRCsplitState());
}

TEST(RCSplitTest, TestRCSplitInitWithoutOpenPortConfigurated)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = false;
    testData.isRunCamSplitPortConfigurated = true;

    bool result = rcSplitInit();
    EXPECT_EQ(false, result);
    EXPECT_EQ(RCSPLIT_STATE_UNKNOWN, unitTestRCsplitState());
}

TEST(RCSplitTest, TestRCSplitInit)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;

    bool result = rcSplitInit();
    EXPECT_EQ(true, result);
    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());
}

TEST(RCSplitTest, TestRecvWhoAreYouResponse)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    
    bool result = rcSplitInit();
    EXPECT_EQ(true, result);

    // here will generate a number in [6-255], it's make the serialRxBytesWaiting() and serialRead() run at least 5 times, 
    // so the "who are you response" will full received, and cause the state change to RCSPLIT_STATE_IS_READY;
    int8_t randNum = rand() % 127 + 6; 
    testData.maxTimesOfRespDataAvailable = randNum;
    rcSplitProcess((timeUs_t)0);

    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());
}

TEST(RCSplitTest, TestWifiModeChangeWithDeviceUnready)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = rcSplitInit();
    EXPECT_EQ(true, result);

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
    rcData[modeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1800;
    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 900;
    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 900;

    updateActivatedModes();

    // runn process loop
    rcSplitProcess(0);

    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));
}

TEST(RCSplitTest, TestWifiModeChangeWithDeviceReady)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = rcSplitInit();
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
    rcSplitProcess((timeUs_t)0);

    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());

    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));
}

TEST(RCSplitTest, TestWifiModeChangeCombine)
{
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.maxTimesOfRespDataAvailable = 0;
    
    bool result = rcSplitInit();
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
    rcSplitProcess((timeUs_t)0);

    EXPECT_EQ(RCSPLIT_STATE_IS_READY, unitTestRCsplitState());

    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));


    // // make the binded mode inactive
    rcData[modeActivationConditions(0)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1500;
    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1300;
    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1900;
    updateActivatedModes();
    rcSplitProcess((timeUs_t)0);
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA3));


    rcData[modeActivationConditions(2)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 1899;
    updateActivatedModes();
    rcSplitProcess((timeUs_t)0);
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));

    rcData[modeActivationConditions(1)->auxChannelIndex + NON_AUX_CHANNEL_COUNT] = 2001;
    updateActivatedModes();
    rcSplitProcess((timeUs_t)0);
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA1));
    EXPECT_EQ(true, unitTestIsSwitchActivited(BOXCAMERA2));
    EXPECT_EQ(false, unitTestIsSwitchActivited(BOXCAMERA3));
}

static void osdDrawLogo(displayPort_t *osdDisplayPort, int x, int y)
{
    // display logo and help
    char fontOffset = 160;
    for (int row = 0; row < 4; row++) {
        for (int column = 0; column < 24; column++) {
            if (fontOffset != 255) // FIXME magic number
                displayWriteChar(osdDisplayPort, x + column, y + row, fontOffset++);
        }
    }
}

TEST(RCSplitTest, TestPacketGenerate)
{
    sbuf_t buf;
    
    bool result = false;
    uint16_t expectedPacketSize = 0;
    uint16_t actualPacketSize = 0;
    uint8_t *p = NULL;
    uint8_t *base = NULL;
    memset(&testData, 0, sizeof(testData));
    unitTestResetRCSplit();

    testData.isAllowBufferReadWrite = true;
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    
    result = rcSplitInit();
    EXPECT_EQ(true, result);

    // for (int i = 0; i < 1; i++) {
    //     expectedPacketSize = rcCamOSDGenerateDrawStringPacket(NULL, 5, 5, "Hello", 5);
    //     base = (uint8_t*)malloc(expectedPacketSize);
    //     buf.ptr = base;
    //     actualPacketSize = rcCamOSDGenerateDrawStringPacket(&buf, 5, 5, "Hello", 5);
    //     p = buf.ptr;
    //     for (int i = 0; i < actualPacketSize; i++) {
    //         printf("%02x ", *p++);
    //     }
    //     printf("\n");
    //     free(base);
    // }
    
    
    // expectedPacketSize = rcCamOSDGenerateClearPacket(NULL);
    // base = (uint8_t*)malloc(expectedPacketSize);
    // buf.ptr = base;
    // actualPacketSize = rcCamOSDGenerateClearPacket(&buf);
    // p = base;
    // printf("clear cmd11(%d):", expectedPacketSize);
    // for (int i = 0; i < actualPacketSize; i++) {
    //     printf("%02x ", *p++);
    // }
    // printf("\n");

    // base = buf.ptr = NULL;


    rcsplit_packet_v2_t packet;
    displayPort_t *osdDisplayPort = rccameraDisplayPortInit(rcSplitSerialPort);
    EXPECT_EQ(true, osdDisplayPort != NULL);

    // displayClearScreen(osdDisplayPort);
    // osdDrawLogo(osdDisplayPort, 3, 1);
    // displayWrite(osdDisplayPort, 7, 8,  CMS_STARTUP_HELP_TEXT1);
    // displayWrite(osdDisplayPort, 11, 9, CMS_STARTUP_HELP_TEXT2);
    // displayWrite(osdDisplayPort, 11, 10, CMS_STARTUP_HELP_TEXT3);

    // expectedPacketSize = rcCamOSDGenerateDrawScreenPacket(NULL, rcsplitOSDScreenBuffer);
    // base = (uint8_t*)malloc(expectedPacketSize);
    // buf.ptr = base;
    // actualPacketSize = rcCamOSDGenerateDrawScreenPacket(&buf, rcsplitOSDScreenBuffer);
    // p = buf.ptr;
    // printf("drawlogo:");
    // for (int i = 0; i < actualPacketSize; i++) {
    //     printf("%02x ", *p++);
    // }
    // printf("\n");

    // displayClearScreen(osdDisplayPort);
    // for (int i = 0; i < RCCAMERA_SCREEN_CHARACTER_ROW_COUNT; i++) {
    //     for (int j = 0; j < RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT; j++) {
    //         displayWrite(osdDisplayPort, j, i, "A");
    //     }
    // }

    expectedPacketSize = rcCamOSDGenerateControlPacket(NULL, RCSPLIT_CTRL_ARGU_WIFI_BTN);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    actualPacketSize = rcCamOSDGenerateControlPacket(&buf, RCSPLIT_CTRL_ARGU_WIFI_BTN);
    EXPECT_EQ(expectedPacketSize, actualPacketSize); 
    p = buf.ptr;
    printf("wifi button:");
    for (int i = 0; i < actualPacketSize; i++) {
        printf("%02x ", *p++);
    }
    printf("\n");

    expectedPacketSize = rcCamOSDGenerateControlPacket(NULL, RCSPLIT_CTRL_ARGU_POWER_BTN);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    actualPacketSize = rcCamOSDGenerateControlPacket(&buf, RCSPLIT_CTRL_ARGU_POWER_BTN);
    EXPECT_EQ(expectedPacketSize, actualPacketSize); 
    p = buf.ptr;
    printf("power button:");
    for (int i = 0; i < actualPacketSize; i++) {
        printf("%02x ", *p++);
    }
    printf("\n");

    expectedPacketSize = rcCamOSDGenerateControlPacket(NULL, RCSPLIT_CTRL_ARGU_CHANGE_MODE);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    actualPacketSize = rcCamOSDGenerateControlPacket(&buf, RCSPLIT_CTRL_ARGU_CHANGE_MODE);
    EXPECT_EQ(expectedPacketSize, actualPacketSize); 
    p = buf.ptr;
    printf("change mode button:");
    for (int i = 0; i < actualPacketSize; i++) {
        printf("%02x ", *p++);
    }
    printf("\n");

    expectedPacketSize = rcCamOSDGenerateClearPacket(NULL);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    actualPacketSize = rcCamOSDGenerateClearPacket(&buf);
    EXPECT_EQ(expectedPacketSize, actualPacketSize); 
    p = buf.ptr;
    printf("clear fullscreen:");
    for (int i = 0; i < actualPacketSize; i++) {
        printf("%02x ", *p++);
    }
    printf("\n");

    // parse the packet, check the fields is correct or not.
    result = rcCamOSDPasrePacket(&buf, &packet);
    EXPECT_EQ(true, result);
    EXPECT_EQ(RCSPLIT_PACKET_CMD_OSD_CLEAR, packet.command);

    free(base);
    base = buf.ptr = NULL;


    // x:10, y:10; 字符:A
    // y:50, y:50; 字符:B
    // y:80, y:80; 字符:C
    rcsplit_osd_particle_screen_data_t testParticalChanges[] = { {10, 10, 'A'}, {20, 20, 'B'}, {30, 30, 'C'}, };
    int testDataCount = sizeof(testParticalChanges) / sizeof(rcsplit_osd_particle_screen_data_t);
    uint8_t *dataBuf = (uint8_t*)malloc(sizeof(testParticalChanges));
    uint8_t pos = 0;
    for (int i = 0; i < testDataCount; i++) {
        rcsplit_osd_particle_screen_data_t data = testParticalChanges[i];
        dataBuf[pos++] = data.x;
        dataBuf[pos++] = data.y;
        dataBuf[pos++] = data.c;
    }

    expectedPacketSize = rcCamOSDGenerateDrawParticleScreenPacket(NULL, dataBuf, pos);
    base = (uint8_t*)malloc(expectedPacketSize);
    buf.ptr = base;
    actualPacketSize = rcCamOSDGenerateDrawParticleScreenPacket(&buf, dataBuf, pos);
    p = buf.ptr;
    printf("praticle data:");
    for (int i = 0; i < actualPacketSize; i++) {
        printf("%02x ", *p++);
    }
    printf("\n");
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
            printf("in write data: %d\n", len);
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

    bool feature(uint32_t) { return false;}
    void serialWriteBuf(serialPort_t *instance, const uint8_t *data, int count) { UNUSED(instance); UNUSED(data); UNUSED(count); }
}