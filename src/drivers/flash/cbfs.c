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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
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
	if (media->context)
		free(media->context);

	return 0;
}

static size_t add_offset(struct cbfs_media *media,
			size_t offset, size_t count)
{
	if (media->context) {
		FmapArea *area = (FmapArea *)media->context;

		if (offset + count > area->size)
			return offset;
		return offset + area->offset;
	}
	return offset;
}

static size_t cbfs_media_read(struct cbfs_media *media,
			      void *dest, size_t offset,
			      size_t count)
{
	offset = add_offset(media, offset, count);
	uint8_t *cache = flash_read(offset, count);
	if (!cache)
		return 0;
	memcpy(dest, cache, count);
	return count;
}

static void *cbfs_media_map(struct cbfs_media *media,
			    size_t offset, size_t count)
{
	offset = add_offset(media, offset, count);
	void *ptr = flash_read(offset, count);
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

int cbfs_media_from_fmap(struct cbfs_media *media, const char *name)
{
	FmapArea *area = malloc(sizeof(*area));

	if (area == NULL) {
		printf("couldn't allocate memory for FmapArea\n");
		return -1;
	}
	if (fmap_find_area(name, area)) {
		printf("'%s' not found\n", name);
		return -1;
	}

	media->context = area;

	media->open = cbfs_media_open;
	media->close = cbfs_media_close;
	media->read = cbfs_media_read;
	media->map = cbfs_media_map;
	media->unmap = cbfs_media_unmap;

	return 0;
}
