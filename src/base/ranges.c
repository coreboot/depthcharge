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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>

#include "base/physmem.h"
#include "base/ranges.h"

/*
 * This implementation tracks a collection of ranges by keeping a linked list
 * of the edges between ranges in the collection and the space between them.
 * New ranges take precedence over older ranges they overlap with.
 */

static void ranges_insert_between(RangesEdge *before, RangesEdge *after,
				  uint64_t pos)
{
	RangesEdge *new_edge = xmalloc(sizeof(*new_edge));

	assert(before != after);

	new_edge->next = after;
	new_edge->pos = pos;
	before->next = new_edge;
}

void ranges_init(Ranges *ranges)
{
	ranges->head.next = NULL;
	ranges->head.pos = 0;
}

void ranges_teardown(Ranges *ranges)
{
	RangesEdge *edge = ranges->head.next;

	while (edge) {
		RangesEdge *next = edge->next;
		free(edge);
		edge = next;
	}
	ranges->head.next = NULL;
}

static void ranges_set_region_to(Ranges *ranges, uint64_t start,
				 uint64_t end, int new_included)
{
	/* whether the current region was originally going to be included. */
	int included = 0;

	assert(start != end);

	/* prev is never NULL, but cur might be. */
	RangesEdge *prev = &ranges->head;
	RangesEdge *cur = prev->next;

	/*
	 * Find the start of the new region. After this loop, prev will be
	 * before the start of the new region, and cur will be after it or
	 * overlapping start. If they overlap, this ensures that the existing
	 * edge is deleted and we don't end up with two edges in the same spot.
	 */
	while (cur && cur->pos < start) {
		prev = cur;
		cur = cur->next;
		included = !included;
	}

	/* Add the "start" edge between prev and cur, if needed. */
	if (new_included != included) {
		ranges_insert_between(prev, cur, start);
		prev = prev->next;
	}

	/*
	 * Delete any edges obscured by the new region. After this loop, prev
	 * will be before the end of the new region or overlapping it, and cur
	 * will be after if, if there is a edge after it. For the same
	 * reason as above, we want to ensure that we end up with one edge if
	 * there's an overlap.
	 */
	while (cur && cur->pos <= end) {
		cur = cur->next;
		free(prev->next);
		prev->next = cur;
		included = !included;
	}

	/* Add the "end" edge between prev and cur, if needed. */
	if (included != new_included)
		ranges_insert_between(prev, cur, end);
}

/* Add a range to a collection of ranges. */
void ranges_add(Ranges *ranges, uint64_t start, uint64_t end)
{
	ranges_set_region_to(ranges, start, end, 1);
}

/* Subtract a range. */
void ranges_sub(Ranges *ranges, uint64_t start, uint64_t end)
{
	ranges_set_region_to(ranges, start, end, 0);
}

/* Run a function on each range in Ranges. */
void ranges_for_each(Ranges *ranges, RangesForEachFunc func, void *data)
{
	for (RangesEdge *cur = ranges->head.next; cur; cur = cur->next->next) {
		if (!cur->next) {
			printf("Odd number of range edges!\n");
			return;
		}

		func(cur->pos, cur->next->pos, data);
	}
}
