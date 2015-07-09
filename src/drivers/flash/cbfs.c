/*
 * This file is part of the coreboot project.
 *
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

static size_t cbfs_media_read(struct cbfs_media *media,
			      void *dest, size_t offset,
			      size_t count)
{
	uint8_t *cache;

	if (offset == 0 - sizeof(uint32_t) && count == sizeof(uint32_t))
		cache = (uint8_t *)&lib_sysinfo.cbfs_header_offset;
	else
		cache = flash_read(offset, count);
	if (!cache)
		return 0;
	memcpy(dest, cache, count);

	return count;
}

static void *cbfs_media_map(struct cbfs_media *media,
			    size_t offset, size_t count)
{
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
	media->open = cbfs_media_open;
	media->close = cbfs_media_close;
	media->read = cbfs_media_read;
	media->map = cbfs_media_map;
	media->unmap = cbfs_media_unmap;

	return 0;
}
