/*
 * Copyright 2015 Google Inc.
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

#include <assert.h>
#include <libpayload.h>

#include "base/gpt.h"

GptData *alloc_gpt(BlockDev *bdev)
{
	assert(bdev);

	GptData *gpt = xzalloc(sizeof(*gpt));

	gpt->sector_bytes = bdev->block_size;
	gpt->streaming_drive_sectors = bdev->block_count;
	gpt->gpt_drive_sectors = bdev->block_count;

	if (AllocAndReadGptData(bdev, gpt) != 0) {
		free(gpt);
		return NULL;
	}

	if (GptInit(gpt) != GPT_SUCCESS) {
		free(gpt);
		gpt = NULL;
	}

	return gpt;
}

void free_gpt(BlockDev *bdev, GptData *gpt)
{
	assert(bdev && gpt);

	WriteAndFreeGptData(bdev, gpt);
	free(gpt);
}

static void one_byte(char *dest, uint8_t val)
{
	dest[0] = "0123456789abcdef"[(val >> 4) & 0x0F];
	dest[1] = "0123456789abcdef"[val & 0x0F];
}

char *guid_to_string(const uint8_t *guid, char *dest, size_t dest_size)
{
	if (dest_size < GUID_STRLEN)
		return NULL;

	one_byte(dest, guid[3]); dest += 2;
	one_byte(dest, guid[2]); dest += 2;
	one_byte(dest, guid[1]); dest += 2;
	one_byte(dest, guid[0]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[5]); dest += 2;
	one_byte(dest, guid[4]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[7]); dest += 2;
	one_byte(dest, guid[6]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[8]); dest += 2;
	one_byte(dest, guid[9]); dest += 2;
	*dest++ = '-';
	one_byte(dest, guid[10]); dest += 2;
	one_byte(dest, guid[11]); dest += 2;
	one_byte(dest, guid[12]); dest += 2;
	one_byte(dest, guid[13]); dest += 2;
	one_byte(dest, guid[14]); dest += 2;
	one_byte(dest, guid[15]); dest += 2;

	*dest = '\0';

	return dest;
}
