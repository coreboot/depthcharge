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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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
