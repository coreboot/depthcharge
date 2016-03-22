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
 */

#ifndef __IMAGE_FMAP_H__
#define __IMAGE_FMAP_H__

#include <stdint.h>

typedef struct FmapArea {
	uint32_t offset;
	uint32_t size;
	uint8_t name[32];
	uint16_t flags;
} __attribute__ ((packed)) FmapArea;


typedef struct Fmap {
	uint8_t signature[8];
	uint8_t ver_major;
	uint8_t ver_minor;
	uint64_t base;
	uint32_t size;
	uint8_t name[32];
	uint16_t nareas;
	FmapArea areas[0];
} __attribute__ ((packed)) Fmap;

#define FMAP_SIGNATURE "__FMAP__"

const int fmap_find_area(const char *name, FmapArea *area);
const char *fmap_find_string(const char *name, int *size);

const Fmap *fmap_base(void);

#endif /* __IMAGE_FMAP_H__ */
