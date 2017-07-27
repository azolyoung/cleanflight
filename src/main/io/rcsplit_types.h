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

#define RCCAMERA_FONT_WIDTH 14
#define RCCAMERA_FONT_HEIGHT 21
#define RCCAMERA_HORIZONTAL_PADDING 0
#define RCCAMERA_VERTICAL_PADDING 0

#define RCCAMERA_CHARACTER_WIDTH_TOTAL (RCCAMERA_FONT_WIDTH + RCCAMERA_HORIZONTAL_PADDING)
#define RCCAMERA_CHARACTER_HEIGHT_TOTAL (RCCAMERA_FONT_HEIGHT + RCCAMERA_VERTICAL_PADDING)

#define RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT (RCCAMERA_SCREEN_WIDTH / RCCAMERA_CHARACTER_WIDTH_TOTAL)
#define RCCAMERA_SCREEN_CHARACTER_ROW_COUNT (RCCAMERA_SCREEN_HEIGHT / RCCAMERA_CHARACTER_HEIGHT_TOTAL)

typedef enum {
    RCSPLIT_OSD_TEXT_ALIGN_RIGHT = 0,
    RCSPLIT_OSD_TEXT_ALIGN_LEFT = 1,
} rcsplit_osd_text_align_e;

// packet header and tail
#define RCSPLIT_PACKET_HEADER           0x55
#define RCSPLIT_PACKET_TAIL             0xaa

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
    RCSPLIT_PACKET_CMD_CTRL =                   0x01,
    RCSPLIT_PACKET_CMD_OSD_DRAW_STRING =        0x20, // write characters to OSD in rcsplit
    RCSPLIT_PACKET_CMD_OSD_CLEAR =              0x21,
    RCSPLIT_PACKET_CMD_OSD_DRAW_SCREEN =        0x22, // draw a screen buffer to rcsplit
} rcsplit_packet_cmd_e;

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
    uint8_t command;
    uint16_t dataLen;
    uint8_t *data;
    uint16_t crc16;
    uint8_t tail;
}) rcsplit_packet_v2_t;

// The data struct of command RCSPLIT_PACKET_CMD_OSD_DRAW_STRING
RCPACKED(
typedef struct {
    uint8_t align;
    uint16_t x;
    uint16_t y;
    uint8_t *characters; 
}) rcsplit_osd_draw_text_data_t;

// The data struct of command RCSPLIT_PACKET_CMD_OSD_DRAW_SCREEN
RCPACKED(
typedef struct {
    uint8_t characters[RCCAMERA_SCREEN_CHARACTER_COLUMN_COUNT * RCCAMERA_SCREEN_CHARACTER_ROW_COUNT]; 
}) rcsplit_osd_draw_screen_data_t;

// The data struct of command RCSPLIT_PACKET_CMD_OSD_CLEAR
RCPACKED(
typedef struct {
    uint8_t align;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t end_x;
    uint16_t end_y;
}) rcsplit_osd_clear_screen_data_t;