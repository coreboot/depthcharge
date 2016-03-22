/*
 * Copyright 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * This file provides a common CBFS wrapper for flash storage.
 */

#include <libpayload.h>
#include <cbfs.h>
#include "image/fmap.h"
#include "drivers/flash/flash.h"

/* flash as CBFS media. */

static int cbfs_media_open(struct cbfs_media *media)
{
	return 0;
}

static int cbfs_media_close(struct cbfs_media *media)
{
	return 0;
}

/*
 * Translate offset is technically only needed for non-CBFS_DEFAULT_MEDIA
 * under Chrome OS current construction. The reason is that the firmware
 * will set the appropriate current CBFS in the coreboot tables and
 * libpayload will honor those for CBFS_DEFAULT_MEDIA. However, when passing
 * a non-CBFS_DEFAULT_MEDIA pointer into libpayload's CBFS driver it will
 * need to perform the translation since it attempts to locate the CBFS
 * master header in order to derive its size and offset. The offsets used
 * when locating the master header are negative numbers. Thus, translate
 * the negative offset into one which is within the size of the region.
 */
static size_t translate_offset(struct cbfs_media *media, size_t offset)
{
	FmapArea *area;
	ssize_t soffset;

	if (media->context == NULL)
		return offset;

	area = media->context;
	soffset = offset;

	/*
	 * When the offset passed in is less than zero libpayload is attempting
	 * to locate the master header. Therefore translate that into an
	 * absolute offset within the RO fmap area. There will be 2 calls:
	 * offset = -4 and signed 32-bit relative value found at -4 offset.
	 */
	if (soffset < 0)
		offset = soffset + area->size + area->offset;

	return offset;
}

static size_t cbfs_media_read(struct cbfs_media *media,
			      void *dest, size_t offset,
			      size_t count)
{
	uint8_t *cache;

	offset = translate_offset(media, offset);
	cache = flash_read(offset, count);
	if (!cache)
		return 0;
	memcpy(dest, cache, count);
	return count;
}

static void *cbfs_media_map(struct cbfs_media *media,
			    size_t offset, size_t count)
{
	void *ptr;

	offset = translate_offset(media, offset);
	ptr = flash_read(offset, count);
	if (!ptr)
		ptr = CBFS_MEDIA_INVALID_MAP_ADDRESS;
	return ptr;
}

static void *cbfs_media_unmap(struct cbfs_media *media, const void *address)
{
	return 0;
}

int libpayload_init_default_cbfs_media(struct cbfs_media *media)
{
	media->context = NULL;
	media->open = cbfs_media_open;
	media->close = cbfs_media_close;
	media->read = cbfs_media_read;
	media->map = cbfs_media_map;
	media->unmap = cbfs_media_unmap;

	return 0;
}

struct cbfs_media *cbfs_ro_media(void)
{
	FmapArea area;
	struct cbfs_media *media;

	/* The FMAP entries for the RO CBFS are either COREBOOT or BOOT_STUB. */
	if (fmap_find_area("COREBOOT", &area) &&
	    fmap_find_area("BOOT_STUB", &area))
		return NULL;

	media = xmalloc(sizeof(*media));

	libpayload_init_default_cbfs_media(media);

	media->context = xmalloc(sizeof(area));

	memcpy(media->context, &area, sizeof(area));

	return media;
}
