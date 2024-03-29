/*
 * Generic bounce buffer implementation
 *
 * Copyright (C) 2012 Marek Vasut <marex@denx.de>
 * Copyright 2013 Google LLC
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/cache.h>
#include <errno.h>
#include <libpayload.h>
#include <malloc.h>

#include "bouncebuf.h"

// TODO(hungte) Deprecate this file after we've shown the drivers using this
// file work, and get migrated into better code to handle non-cached and aligned
// memory.

static int _debug = 0;

static int addr_aligned(struct bounce_buffer *state)
{
	const uint32_t align_mask = ARCH_DMA_MINALIGN - 1;

	// Check if start is aligned
	if ((uintptr_t)state->user_buffer & align_mask) {
		if (_debug)
			printf("Unaligned buffer address %p\n",
			       state->user_buffer);
		return 0;
	}

	// Check if length is aligned
	if (state->len != state->len_aligned) {
		if (_debug)
			printf("Unaligned buffer length %zd\n", state->len);
		return 0;
	}

	// Aligned
	return 1;
}

int bounce_buffer_start(struct bounce_buffer *state, void *data,
			size_t len, unsigned int flags)
{
	state->user_buffer = data;
	state->bounce_buffer = data;
	state->len = len;
	state->len_aligned = ROUND(len, ARCH_DMA_MINALIGN);
	state->flags = flags;

	if (dma_coherent(data))
		return 0;

	if (!addr_aligned(state)) {
		state->bounce_buffer = memalign(ARCH_DMA_MINALIGN,
						state->len_aligned);
		if (!state->bounce_buffer)
			return -1;

		if (state->flags & GEN_BB_READ)
			memcpy(state->bounce_buffer, state->user_buffer,
			       state->len);
	}

	/* Clean cache (flush to RAM) so that DMA reads can pick it up */
	if (state->flags & GEN_BB_READ)
		dcache_clean_by_mva(state->bounce_buffer, state->len_aligned);

	/* Invalidate cache so that CPU writebacks don't race with DMA writes */
	if (state->flags & GEN_BB_WRITE)
		dcache_invalidate_by_mva(state->bounce_buffer,
					 state->len_aligned);

	return 0;
}

int bounce_buffer_stop(struct bounce_buffer *state)
{
	if (dma_coherent(state->user_buffer))
		return 0;

	/* Invalidate cache so that CPU can see any newly DMA'd data */
	if (state->flags & GEN_BB_WRITE)
		dcache_invalidate_by_mva(state->bounce_buffer,
					 state->len_aligned);

	if (state->bounce_buffer == state->user_buffer)
		return 0;

	if (state->flags & GEN_BB_WRITE)
		memcpy(state->user_buffer, state->bounce_buffer, state->len);

	free(state->bounce_buffer);

	return 0;
}
