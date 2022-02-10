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
#include "drivers/flash/flash.h"

/* Function required by libpayload libcbfs implementation to access CBFS */
ssize_t boot_device_read(void *buf, size_t offset, size_t size)
{
	uint8_t *cache;

	/* buffer passed by the API should not be affected on error */
	cache = flash_read(offset, size);
	if (!cache)
		return CB_ERR;
	memcpy(buf, cache, size);
	return size;
}
