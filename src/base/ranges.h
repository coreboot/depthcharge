/*
 * Copyright 2012 Google Inc.
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

#ifndef __BASE_RANGES_H__
#define __BASE_RANGES_H__

#include <stdint.h>

/* A node in a linked list of edges, each at position "pos". */
typedef struct RangesEdge {
	struct RangesEdge *next;
	uint64_t pos;
} RangesEdge;

/*
 * Data describing ranges. Contains a linked list of edges between the ranges
 * and the empty space between them.
 */
typedef struct Ranges {
	RangesEdge head;
} Ranges;

/*
 * Initializes the Ranges structure.
 *
 * @param ranges	Ranges structure to initialize.
 */
void ranges_init(Ranges *ranges);

/*
 * Free memory associated with a Ranges structure, not including the structure
 * itself.
 *
 * @param ranges	Ranges structure to tear down.
 */
void ranges_teardown(Ranges *ranges);

/*
 * Adds a range to a collection of ranges.
 *
 * @param ranges	Ranges structure to add the range to.
 * @param start		The start of the range.
 * @param end		The end of the range.
 */
void ranges_add(Ranges *ranges, uint64_t start, uint64_t end);

/*
 * Subtracts a range.
 *
 * @param ranges	Ranges structure to subtract the range from.
 * @param start		The start of the region.
 * @param end		The end of the region.
 */
void ranges_sub(Ranges *ranges, uint64_t start, uint64_t end);

typedef void (*RangesForEachFunc)(uint64_t start, uint64_t end, void *data);

/*
 * Runs a function on each range in Ranges.
 *
 * @param ranges	Ranges structure to iterate over.
 */
void ranges_for_each(Ranges *ranges, RangesForEachFunc func, void *data);

#endif /* __BASE_RANGES_H__ */
