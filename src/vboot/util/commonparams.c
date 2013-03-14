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

#include <gbb_header.h>
#include <libpayload.h>

#include "config.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/util/commonparams.h"

VbCommonParams cparams CPARAMS;
uint8_t shared_data_blob[VB_SHARED_DATA_REC_SIZE] SHARED_DATA;

static void *gbb_copy_in(uint32_t gbb_offset, uint32_t offset, uint32_t size)
{
	uint8_t *gbb_copy = cparams.gbb_data;

	if (offset > cparams.gbb_size || offset + size > cparams.gbb_size) {
		printf("GBB component not inside the GBB.\n");
		return NULL;
	}

	void *data;
	data = flash_read(gbb_offset + offset, size);
	if (!data)
		return NULL;
	memcpy(gbb_copy + offset, data, size);
	return gbb_copy + offset;
}

static int gbb_init(void)
{
	FmapArea area;
	if (fmap_find_area("GBB", &area)) {
		printf("Couldn't find the GBB.\n");
		return 1;
	}

	if (area.size > CONFIG_GBB_COPY_SIZE) {
		printf("Not enough room for a copy of the GBB.\n");
		return 1;
	}

	cparams.gbb_size = area.size;
	cparams.gbb_data = (void *)&_gbb_copy_start;
	memset(cparams.gbb_data, 0, cparams.gbb_size);

	uint32_t offset = area.offset;

	GoogleBinaryBlockHeader *header =
		gbb_copy_in(offset, 0, sizeof(GoogleBinaryBlockHeader));
	if (!header)
		return 1;
	printf("The GBB signature is at %p and is: ", header->signature);
	for (int i = 0; i < GBB_SIGNATURE_SIZE; i++)
		printf(" %02x", header->signature[i]);
	printf("\n");

	if (!gbb_copy_in(offset, header->hwid_offset, header->hwid_size))
		return 1;

	if (!gbb_copy_in(offset, header->rootkey_offset, header->rootkey_size))
		return 1;

	if (!gbb_copy_in(offset, header->recovery_key_offset,
			 header->recovery_key_size))
		return 1;

	return 0;
}

int gbb_copy_in_bmp_block(void)
{
	FmapArea area;
	if (fmap_find_area("GBB", &area)) {
		printf("Couldn't find the GBB.\n");
		return 1;
	}

	GoogleBinaryBlockHeader *header =
		(GoogleBinaryBlockHeader *)cparams.gbb_data;

	if (!gbb_copy_in(area.offset, header->bmpfv_offset,
			 header->bmpfv_size))
		return 1;

	return 0;
}

int common_params_init(int clear_shared_data)
{
	// Set up the common param structure.
	memset(&cparams, 0, sizeof(cparams));

	if (gbb_init())
		return 1;

	void *blob;
	int size;
	if (find_common_params(&blob, &size))
		return 1;

	cparams.shared_data_blob = blob;
	cparams.shared_data_size = size;
	if (clear_shared_data)
		memset(blob, 0, size);

	return 0;
}
