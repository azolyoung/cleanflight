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

#pragma once

#ifdef __GNUC__
  #define RCPACKED( __Declaration__ ) __Declaration__ __attribute__((packed))
#else
  #define RCPACKED( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif

#define RCCAMERA_SCREEN_WIDTH 320
#define RCCAMERA_SCREEN_HEIGHT 240

#define RCCAMERA_FONT_WIDTH (10)
#define RCCAMERA_FONT_HEIGHT 15
#define RCCAMERA_HORIZONTAL_PADDING 0
#define RCCAMERA_VERTICAL_PADDING 0

#define RCCAMERA_CHARACTER_WIDTH_TOTAL (RCCAMERA_FONT_WIDTH + RCCAMERA_HORIZONTAL_PADDING)
#define RCCAMERA_CHARACTER_HEIGHT_TOTAL (RCCAMERA_FONT_HEIGHT + RCCAMERA_VERTICAL_PADDING)

// #define RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT (RCCAMERA_SCREEN_WIDTH / RCCAMERA_CHARACTER_WIDTH_TOTAL)
// #define RCCAMERA_SCREEN_CHARACTER_ROW_COUNT (RCCAMERA_SCREEN_HEIGHT / RCCAMERA_CHARACTER_HEIGHT_TOTAL)
#define RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT 30
#define RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_PAL 16
#define RCCAMERA_SCREEN_CHARACTER_ROW_COUNT_NTSC 13

// packet header and tail
#define RCSPLIT_PACKET_HEADER           0x55
#define RCSPLIT_PACKET_TAIL             0xaa // it's decrepted on Protocol V2 for split
#define RCSPLIT_OPENCTO_CAMERA_DEVICE   0x2

typedef struct {
    uint8_t boxId;
    bool isActivated;
} rcsplit_switch_state_t;

typedef enum {
    RCSPLIT_STATE_UNKNOWN = 0,
    RCSPLIT_STATE_INITIALIZING,
    RCSPLIT_STATE_IS_READY,
} rcsplit_state_e;

typedef enum {
    RCSPLIT_PACKET_CMD_CTRL =                           0x00,
    RCSPLIT_PACKET_CMD_OSD_CLEAR =                      0x01,
    RCSPLIT_PACKET_CMD_OSD_DRAW_PARTICLE_SCREEN_DATA =  0x02, // draw partial screen buffer to rcsplit
    RCSPLIT_PACKET_CMD_GET_CAMERA_INFO =                0x03, // get the base info of camera, e.g video_format(N/P)
    RCSPLIT_PACKET_CMD_GET_CONFIGURATIONS =             0x04,
} rcsplit_packet_cmd_e;

typedef enum {
    RCSPLIT_VIDEOFMT_UNKNOWN = 0,
    RCSPLIT_VIDEOFMT_PAL,
    RCSPLIT_VIDEOFMT_NTSC
} rcsplit_video_format_e;

// the commands of RunCam Split serial protocol
typedef enum {
    RCSPLIT_CTRL_ARGU_INVALID = 0x0,
    RCSPLIT_CTRL_ARGU_WIFI_BTN = 0x1,
    RCSPLIT_CTRL_ARGU_POWER_BTN = 0x2,
    RCSPLIT_CTRL_ARGU_CHANGE_MODE = 0x3,
    RCSPLIT_CTRL_ARGU_WHO_ARE_YOU = 0xFF,
} rcsplit_ctrl_argument_e;

// The V1 packet struct for runcam split
RCPACKED(
typedef struct {
    uint8_t header;
    uint8_t command;
    uint8_t argument;
    uint8_t crc8;
    uint8_t tail;
}) rcsplit_packet_v1_t;

// The V2 packet struct for runcam split
RCPACKED(
typedef struct {
    uint8_t header;
    uint8_t deviceID : 4;
    uint8_t command : 4;
    uint8_t dataLen;
    uint8_t *data;
    uint8_t crc8;
}) rcsplit_packet_v2_t;

// The data struct of command RCSPLIT_PACKET_CMD_OSD_DRAW_PARTICLE_SCREEN_DATA
RCPACKED(
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t c;
}) rcsplit_osd_particle_screen_data_t;

RCPACKED(
typedef struct {
    uint8_t video_format;
}) rcsplit_osd_camera_info_t;

RCPACKED(
typedef struct {
    uint8_t menuNameLength : 4;
    uint8_t menuType : 4;
    char *menuName;
}) rcsplit_cms_menu_data_t;
