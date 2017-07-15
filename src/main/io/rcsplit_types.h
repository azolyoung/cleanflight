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

typedef enum {
    RCSPLIT_OSD_TEXT_ALIGN_RIGHT = 0,
    RCSPLIT_OSD_TEXT_ALIGN_LEFT = 1,
} rcsplit_osd_text_align_e;

// The V1 packet struct for runcam split
typedef struct {
    uint8_t header;
    uint8_t command;
    uint8_t argument;
    uint8_t crc8;
    uint8_t tail;
} rcsplit_packet_v1_t;

// The V2 packet struct for runcam split
typedef struct {
    uint8_t header;
    uint8_t command;
    uint8_t dataLen;
    uint8_t *data;
    uint8_t crc8;
    uint8_t tail;
} rcsplit_packet_v2_t;

// The data struct of command RCSPLIT_PACKET_CMD_OSD_WRITE_CHARS
typedef struct {
    rcsplit_osd_text_align_e align;
    uint16_t x;
    uint16_t y;
    uint8_t *characters;
} rcsplit_osd_write_chars_data_t;

// The data struct of command RCSPLIT_PACKET_CMD_OSD_CLEAR
typedef struct {
    rcsplit_osd_text_align_e textAlign;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t end_x;
    uint16_t end_y;
} rcsplit_osd_clear_screen_data_t;