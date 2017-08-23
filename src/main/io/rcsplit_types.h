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

#define RCSPLIT_OPENCTO_CAMERA_DEVICE   0x2

typedef enum {
    RCSPLIT_VIDEOFMT_UNKNOWN = 0,
    RCSPLIT_VIDEOFMT_PAL,
    RCSPLIT_VIDEOFMT_NTSC
} rcsplit_video_format_e;



typedef enum {
    RCSPLIT_FC_TYPE_BF = 0x2,
    RCSPLIT_FC_TYPE_CF = 0x3,
} rcsplit_fc_type_e;

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

