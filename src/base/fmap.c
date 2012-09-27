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

#include <libpayload.h>

#include "base/fmap.h"

int fmap_check_signature(Fmap *fmap)
{
	return memcmp(fmap->fmap_signature, (uint8_t *)FMAP_SIGNATURE,
		      sizeof(fmap->fmap_signature));
}

FmapArea *fmap_find_area(Fmap *fmap, const char *name)
{
	for (int i = 0; i < fmap->fmap_nareas; i++) {
		FmapArea *area = &(fmap->fmap_areas[i]);
		if (!strncmp(name, (const char *)area->area_name,
				sizeof(area->area_name))) {
			return area;
		}
	}
	return NULL;
}
