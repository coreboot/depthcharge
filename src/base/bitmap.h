/*
 * Copyright 2012 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __BASE_BITMAP_H__
#define __BASE_BITMAP_H__

#include <stdint.h>

/* Main header */

typedef struct __attribute__ ((__packed__)) BitmapFileHeader {
	uint8_t signature[2];
	uint32_t file_size;
	uint16_t reserved[2];
	uint32_t bitmap_offset;
} BitmapFileHeader;


/* Bitmap version 2 */

typedef struct __attribute__ ((__packed__)) BitmapHeaderV2 {
	uint32_t header_size;
	int16_t width;
	int16_t height;
	uint16_t planes;
	uint16_t bits_per_pixel;
} BitmapHeaderV2;

typedef struct __attribute__ ((__packed__)) BitmapPaletteElementV2 {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
} BitmapPaletteElementV2;


/* Bitmap version 3 */

typedef struct __attribute__ ((__packed__)) BitmapHeaderV3 {
	uint32_t header_size;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t bits_per_pixel;
	uint32_t compression;
	uint32_t size;
	int32_t h_res;
	int32_t v_res;
	uint32_t colors_used;
	uint32_t colors_important;
} BitmapHeaderV3;

typedef struct __attribute__ ((__packed__)) BitmapPaletteElementV3 {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t reserved;
} BitmapPaletteElementV3;

typedef struct __attribute__ ((__packed__)) BitmapBitfieldMasks {
	uint32_t red;
	uint32_t green;
	uint32_t blue;
} BitmapBitfieldMasks;


/* Bitmap version 4 */

typedef struct __attribute__ ((__packed__)) BitmapHeaderV4 {
	uint32_t header_size;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t bits_per_pixel;
	uint32_t compression;
	uint32_t size;
	int32_t h_res;
	int32_t v_res;
	uint32_t colors_used;
	uint32_t colors_important;
	uint32_t red_mask;
	uint32_t green_mask;
	uint32_t blue_mask;
	uint32_t alpha_mask;
	uint32_t cs_type;
	int32_t red_x;
	int32_t red_y;
	int32_t red_z;
	int32_t green_x;
	int32_t green_y;
	int32_t green_z;
	int32_t blue_x;
	int32_t blue_y;
	int32_t blue_z;
	uint32_t gamma_red;
	uint32_t gamma_green;
	uint32_t gamma_blue;
} BitmapHeaderV4;

#endif /* __BASE_MEMORY_H__ */
