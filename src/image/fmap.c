/*
 * Copyright 2012 Google LLC
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

#include "base/init_funcs.h"
#include "drivers/flash/flash.h"
#include "image/fmap.h"

static Fmap *main_fmap;

static int fmap_check_signature(Fmap *fmap)
{
	return memcmp(fmap->signature, (uint8_t *)FMAP_SIGNATURE,
		      sizeof(fmap->signature));
}

static void *get_fmap_cache(void) {
	Fmap *fmap = phys_to_virt(lib_sysinfo.fmap_cache);
	if (fmap && fmap_check_signature(fmap)) {
		printf("Bad signature on the FMAP.\n");
		fmap = NULL;
	}
	return fmap;
}

static void *read_fmap_from_flash(void) {
	Fmap fmap;

	if (flash_read(&fmap, lib_sysinfo.fmap_offset, sizeof(Fmap)) !=
	    sizeof(Fmap))
		return NULL;
	if (fmap_check_signature(&fmap)) {
		printf("Bad signature on the FMAP.\n");
		return NULL;
	}

	const uint32_t remaining = fmap.nareas * sizeof(FmapArea);
	uint8_t *full_fmap = xzalloc(sizeof(fmap) + remaining);

	memcpy(full_fmap, &fmap, sizeof(fmap));
	if (flash_read(full_fmap + sizeof(fmap),
		       lib_sysinfo.fmap_offset + sizeof(fmap),
		       remaining) != remaining) {
		free(full_fmap);
		printf("Failed to read full FMAP @%#llx(@%#x).\n",
		       lib_sysinfo.fmap_offset, remaining);
		return NULL;
	}

	return full_fmap;
}

static void fmap_init(void)
{
	static int init_done = 0;

	if (init_done)
		return;

	/* check if it's cached first */
	main_fmap = get_fmap_cache();

	/* not cached, so try reading from flash */
	if (!main_fmap)
		main_fmap = read_fmap_from_flash();

	if (!main_fmap)
		halt();

	init_done = 1;
	return;
}

const Fmap *fmap_base(void)
{
	fmap_init();
	return main_fmap;
}

const int fmap_find_area(const char *name, FmapArea *area)
{
	fmap_init();
	for (int i = 0; i < main_fmap->nareas; i++) {
		const FmapArea *cur = &(main_fmap->areas[i]);
		if (!strncmp(name, (const char *)cur->name,
				sizeof(cur->name))) {
			memcpy(area, cur, sizeof(FmapArea));
			return 0;
		}
	}
	return 1;
}

const char *fmap_find_string(const char *name, int *size)
{
	assert(size);

	FmapArea area;

	*size = 0;
	if (fmap_find_area(name, &area)) {
		return NULL;
	}

	char *buffer = xzalloc(area.size);
	if (flash_read(buffer, area.offset, area.size) != area.size) {
		free(buffer);
		return NULL;
	}

	*size = area.size;
	return buffer;
}
