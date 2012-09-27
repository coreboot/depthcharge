/*
 * Copyright (c) 2011-2012 The Chromium OS Authors.
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

/*
 * Memory wipe library for easily set and exclude memory regions that need
 * to be cleared.
 *
 * The following methods must be called in order:
 *   memory_wipe_init
 *   memory_wipe_exclude
 *   memory_wipe_execute
 */

#ifndef _MEMORY_WIPE_H_
#define _MEMORY_WIPE_H_

#include <physmem.h>

/* A node in a linked list of edges, each at position "pos". */
typedef struct memory_wipe_edge_t {
	struct memory_wipe_edge_t *next;
	phys_addr_t pos;
} memory_wipe_edge_t;

/*
 * Data describing memory to wipe. Contains a linked list of edges between the
 * regions of memory to wipe and not wipe.
 */
typedef struct memory_wipe_t {
	memory_wipe_edge_t head;
} memory_wipe_t;

/*
 * Initializes the memory region that needs to be cleared.
 *
 * @param wipe		Wipe structure to initialize.
 */
void memory_wipe_init(memory_wipe_t *wipe);

/*
 * Adds a memory region to be cleared.
 *
 * @param wipe		Wipe structure to add the region to.
 * @param start		The start of the region.
 * @param end		The end of the region.
 */
void memory_wipe_add(memory_wipe_t *wipe, phys_addr_t start, phys_addr_t end);

/*
 * Subtracts a memory region.
 *
 * @param wipe		Wipe structure to subtract the region from.
 * @param start		The start of the region.
 * @param end		The end of the region.
 */
void memory_wipe_sub(memory_wipe_t *wipe, phys_addr_t start, phys_addr_t end);

/*
 * Executes the memory wipe.
 *
 * @param wipe		Wipe structure to execute.
 */
void memory_wipe_execute(memory_wipe_t *wipe);

#endif /* _MEMORY_WIPE_H_ */
