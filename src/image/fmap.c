/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#include "config.h"
#include "image/fmap.h"

Fmap * const main_fmap = (Fmap *)(uintptr_t)CONFIG_FMAP_ADDRESS;
uintptr_t main_rom_base;

const char *fmap_ro_fwid;
int fmap_ro_fwid_size;
const char *fmap_rwa_fwid;
int fmap_rwa_fwid_size;
const char *fmap_rwb_fwid;
int fmap_rwb_fwid_size;

const char *fmap_find_string(Fmap *fmap, const char *name, int *size)
{
	assert(size);

	FmapArea *area = fmap_find_area(fmap, name);
	if (!area) {
		*size = 0;
		return NULL;
	}
	*size = area->size;
	return (const char *)(uintptr_t)(main_rom_base + area->offset);
}

int fmap_init(void)
{
	if (fmap_check_signature(main_fmap))
		return 1;

	main_rom_base = (uintptr_t)(-main_fmap->size);

	fmap_ro_fwid = fmap_find_string(main_fmap, "RO_FRID",
					&fmap_ro_fwid_size);
	fmap_rwa_fwid = fmap_find_string(main_fmap, "RW_FWID_A",
					 &fmap_rwa_fwid_size);
	fmap_rwb_fwid =	fmap_find_string(main_fmap, "RW_FWID_B",
					 &fmap_rwb_fwid_size);
	return 0;
}

int fmap_check_signature(Fmap *fmap)
{
	return memcmp(fmap->signature, (uint8_t *)FMAP_SIGNATURE,
		      sizeof(fmap->signature));
}

FmapArea *fmap_find_area(Fmap *fmap, const char *name)
{
	for (int i = 0; i < fmap->nareas; i++) {
		FmapArea *area = &(fmap->areas[i]);
		if (!strncmp(name, (const char *)area->name,
				sizeof(area->name))) {
			return area;
		}
	}
	return NULL;
}
