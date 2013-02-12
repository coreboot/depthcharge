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

#ifndef __IMAGE_INDEX_H__
#define __IMAGE_INDEX_H__

struct FmapArea;
typedef struct FmapArea FmapArea;

typedef struct SectionIndexEntry {
	uint32_t offset;
	uint32_t size;
} __attribute__ ((packed)) SectionIndexEntry;

typedef struct SectionIndex {
	uint32_t count;
	SectionIndexEntry entries[0];
} __attribute__ ((packed)) SectionIndex;

const SectionIndex *index_from_fmap(const FmapArea *area);
const void *index_subsection(const FmapArea *area, const int entry,
			     uint32_t *entry_size);

#endif /* __IMAGE_INDEX_H__ */
