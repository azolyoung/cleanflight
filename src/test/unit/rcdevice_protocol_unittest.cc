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
    #include "common/bitarray.h"

    #include "config/parameter_group.h"
    #include "config/parameter_group_ids.h"

    #include "fc/rc_controls.h"
    #include "fc/rc_modes.h"
    

    #include "io/beeper.h"
    #include "io/serial.h"

    #include "scheduler/scheduler.h"
    #include "drivers/serial.h"
    #include "drivers/rcdevice.h"

    #include "rx/rx.h"

    int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];     // interval [1000;2000]
}

typedef struct testData_s {
    bool isRunCamSplitPortConfigurated;
    bool isRunCamSplitOpenPortSupported;
    int8_t maxTimesOfRespDataAvailable;
    bool isAllowBufferReadWrite;
    uint8_t *responesBuf;
    uint8_t responseDataLen;
    uint8_t responseDataReadPos;
    uint32_t millis;
} testData_t;

static testData_t testData;

TEST(RCSplitTest, TestRCDeviceProtocolGeneration)
{
    runcamDevice_t device;

    memset(&testData, 0, sizeof(testData));
    testData.isRunCamSplitOpenPortSupported = true;
    testData.isRunCamSplitPortConfigurated = true;
    testData.isAllowBufferReadWrite = true;
    testData.maxTimesOfRespDataAvailable = 0;
    uint8_t data[] = { 0xcc, 0x56, 0x65, 0x72, 0x31, 0x2e, 0x30, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x37, 0x5e };
    testData.responesBuf = (uint8_t*)malloc(sizeof(data));
    testData.responseDataLen = sizeof(data);
    testData.maxTimesOfRespDataAvailable = testData.responseDataLen;
    memcpy(testData.responesBuf, data, sizeof(data));

    printf("prepare send get device info:\n");
    bool result = runcamDeviceInit(&device);
    printf("\n");
    EXPECT_EQ(result, true);

    printf("prepare send wifi btn simualtion:\n");
    runcamDeviceSimulateCameraButton(&device, RCDEVICE_PROTOCOL_SIMULATE_WIFI_BTN);
    printf("\n");

    printf("prepare send power btn simualtion:\n");
    runcamDeviceSimulateCameraButton(&device, RCDEVICE_PROTOCOL_SIMULATE_POWER_BTN);
    printf("\n");

    printf("prepare change mode simualtion:\n");
    runcamDeviceSimulateCameraButton(&device, RCDEVICE_PROTOCOL_CHANGE_MODE);
    printf("\n");

    printf("prepare fill region:\n");
    runcamDeviceDispFillRegion(&device, 3, 3, 5, 5, 'E');
    printf("\n");

    printf("prepare write char:\n");
    runcamDeviceDispWriteChar(&device, 10, 10, 'Q');
    printf("\n");

    printf("prepare write string:\n");
    runcamDeviceDispWriteString(&device, 2, 2, "hahahaAAII");
    printf("\n");

    printf("prepare get settings of root level:\n");
    runcamDeviceSetting_t *settings = NULL;
    runcamDeviceGetSettings(&device, 0, &settings);
    runcamDeviceReleaseSetting(settings);
    printf("\n");

    printf("prepare get setting charset detail:\n");
    runcamDeviceSettingDetail_t *settingDetail = NULL;
    runcamDeviceGetSettingDetail(&device, 0, &settingDetail);
    runcamDeviceReleaseSettingDetail(settingDetail);
    printf("\n");

    printf("prepare update charset setting:\n");
    runcamDeviceWriteSettingResponse_t *updateSettingResponse;
    uint8_t newCharSetIndex = 1;
    runcamDeviceWriteSetting(&device, 0, &newCharSetIndex, 1, &updateSettingResponse);
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

        if (testData.maxTimesOfRespDataAvailable > 0) {
            return 1;
        }

        return 0;
    }

    uint8_t serialRead(serialPort_t *instance) 
    { 
        UNUSED(instance); 

        if (testData.maxTimesOfRespDataAvailable > 0) {
            uint8_t *buffer = testData.responesBuf;

            if (testData.responseDataReadPos >= testData.responseDataLen) {
                testData.responseDataReadPos = 0;
            }
            testData.maxTimesOfRespDataAvailable--;
            return buffer[testData.responseDataReadPos++];
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
            printf("%02x ", *p);
            // serialWrite(instance, *p);
        }

        // reset the input buffer
        testData.responseDataReadPos = 0;
        testData.maxTimesOfRespDataAvailable = testData.responseDataLen + 1;
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

    uint8_t* sbufPtr(sbuf_t *buf)
    {
        return buf->ptr;
    }

    uint32_t sbufReadU32(sbuf_t *src)
    {
        uint32_t ret;
        ret = sbufReadU8(src);
        ret |= sbufReadU8(src) <<  8;
        ret |= sbufReadU8(src) << 16;
        ret |= sbufReadU8(src) << 24;
        return ret;
    }
    

    uint32_t millis(void) { return testData.millis++; }
}
