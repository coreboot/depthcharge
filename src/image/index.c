/*
 * Copyright 2013 Google Inc.
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

#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/index.h"

const SectionIndex *index_from_fmap(const FmapArea *area)
{
	if (area->size < sizeof(uint32_t)) {
		printf("FMAP section too small.\n");
		return NULL;
	}
	SectionIndex *index = flash_read(area->offset, sizeof(uint32_t));

	uint32_t index_size = (index->count * 2 + 1) * sizeof(uint32_t);
	if (area->size < index_size) {
		printf("FMAP section too small.\n");
		return NULL;
	}
	return flash_read(area->offset, index_size);
}

const void *index_subsection(const FmapArea *area, const int entry,
			     uint32_t *entry_size)
{
	const SectionIndex *index = index_from_fmap(area);
	if (index->count <= entry) {
		printf("Asked for index entry %d, but there are only %d.\n",
			entry, index->count);
		return NULL;
	}
	uint32_t offset = index->entries[entry].offset;
	uint32_t size = index->entries[entry].size;
	if (offset >= area->size || offset + size > area->size) {
		printf("Index entry %d beyond end of FMAP section.\n", entry);
		return NULL;
	}
	if (entry_size)
		*entry_size = size;
	return flash_read(area->offset + offset, size);
}
