/*
 * Copyright 2014 Google Inc.
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

#include <libpayload.h>
#include <image/fmap.h>
#include <vb2_api.h>

#include "drivers/flash/flash.h"

/*
 * nvdata storage in flash uses a block of flash memory to represent the nvdata
 * blob. Typical flash memory allows changing of individual bits from one to
 * zero. Changing bits from zero to one requires an erase operation, which
 * affects entire blocks of storage.
 *
 * In a typical case the last non-erased blob of VB2_NVDATA_SIZE bytes in
 * the dedicated block is considered the current nvdata contents. If there
 * is a need to change nvdata contents, the next blob worth of bytes is
 * written. It becomes the last non-erased blob, which is by definition the
 * current nvdata contents.
 *
 * If the entire dedicated block is empty, the first blob is used as nvdata.
 * It will be considered invalid and overwritten by vboot as required.
 *
 * If there is no room in the dedicated block to store a new blob, the entire
 * block is erased, and new nvdata is written to the lowest blob.
 */

/* FMAP descriptor of the nvdata area */
static FmapArea nvdata_area_descriptor;

/* Offset of the actual nvdata blob offset in the nvdata block. */
static int nvdata_blob_offset;

/* Local cache of the nvdata blob. */
static uint8_t nvdata_cache[VB2_NVDATA_SIZE];

static int flash_nvdata_init(void)
{
	int used_below, empty_above;
	static int nvdata_flash_is_initialized = 0;
	uint8_t empty_nvdata_block[sizeof(nvdata_cache)];
	uint8_t *block;

	if (nvdata_flash_is_initialized)
		return 0;

	if (fmap_find_area("RW_NVRAM", &nvdata_area_descriptor)) {
		printf("%s: failed to find nvdata area\n", __func__);
		return -1;
	}

	/* Prepare an empty nvdata block to compare against. */
	memset(empty_nvdata_block, 0xff, sizeof(empty_nvdata_block));

	/* Binary search for the border between used and empty */
	used_below = 0;
	empty_above = nvdata_area_descriptor.size / sizeof(nvdata_cache);

	while (used_below + 1 < empty_above) {
		int guess = (used_below + empty_above) / 2;
		block = flash_read(nvdata_area_descriptor.offset +
				   guess * sizeof(nvdata_cache),
				   sizeof(nvdata_cache));
		if (!block) {
			printf("%s: failed to read nvdata area\n", __func__);
			return -1;
		}

		if (!memcmp(block, empty_nvdata_block, sizeof(nvdata_cache)))
			empty_above = guess;
		else
			used_below = guess;
	}

	/*
	 * Offset points to the last non-empty blob.  Or if all blobs are empty
	 * (nvdata is totally erased), point to the first blob.
	 */
	nvdata_blob_offset = used_below * sizeof(nvdata_cache);

	block = flash_read(nvdata_area_descriptor.offset + nvdata_blob_offset,
			   sizeof(nvdata_cache));
	if (!block) {
		printf("%s: failed to read nvdata area\n", __func__);
		return -1;
	}
	memcpy(nvdata_cache, block, sizeof(nvdata_cache));

	nvdata_flash_is_initialized = 1;
	return 0;
}

vb2_error_t nvdata_flash_read(uint8_t *buf)
{
	if (flash_nvdata_init())
		return VB2_ERROR_NV_READ;

	memcpy(buf, nvdata_cache, sizeof(nvdata_cache));
	return VB2_SUCCESS;
}

static int erase_nvdata(void)
{
	if (flash_nvdata_init())
		return 1;

	if (flash_erase(nvdata_area_descriptor.offset,
			nvdata_area_descriptor.size) !=
					nvdata_area_descriptor.size)
		return 1;

	return 0;
}

vb2_error_t nvdata_flash_write(const uint8_t *buf)
{
	int i;

	if (flash_nvdata_init())
		return VB2_ERROR_NV_WRITE;

	/* Bail out if there have been no changes. */
	if (!memcmp(buf, nvdata_cache, sizeof(nvdata_cache)))
		return VB2_SUCCESS;

	/* See if we can overwrite the current blob with the new one. */
	for (i = 0; i < sizeof(nvdata_cache); i++)
		if ((nvdata_cache[i] & buf[i]) != buf[i])
			break;

	if (i != sizeof(nvdata_cache)) {
		int new_blob_offset;
		/*
		 * Won't be able to overwrite, need to use the next blob,
		 * let's see if it is available.
		 */
		new_blob_offset = nvdata_blob_offset + sizeof(nvdata_cache);
		if (new_blob_offset >= nvdata_area_descriptor.size) {
			printf("nvdata block is used up. "
			       "deleting it to start over\n");
			if (erase_nvdata() != VB2_SUCCESS)
				return VB2_ERROR_NV_WRITE;
			new_blob_offset = 0;
		}
		nvdata_blob_offset = new_blob_offset;
	}

	if (flash_write(nvdata_area_descriptor.offset + nvdata_blob_offset,
			sizeof(nvdata_cache), buf) != sizeof(nvdata_cache))
		return VB2_ERROR_NV_WRITE;

	memcpy(nvdata_cache, buf, sizeof(nvdata_cache));
	return VB2_SUCCESS;
}

int nvdata_flash_get_offset(void)
{
	return nvdata_blob_offset;
}

int nvdata_flash_get_blob_size(void)
{
	return sizeof(nvdata_cache);
}
