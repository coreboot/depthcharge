/*
 * Copyright 2014 Google Inc.
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
#include <stddef.h>
#include <string.h>

#include <arch/cache.h>
#include "base/physmem.h"

void arch_phys_map(uint64_t start, uint64_t size, PhysMapFunc func, void *data)
{
	uint64_t max_addr = (uint64_t)((1ULL << 48) - 1);
	uint64_t end = start + size;
	assert(end <= max_addr);

	if (end < start || end > max_addr)
		size = max_addr - start;

	func(start, (void *)(uintptr_t)start, size, data);
}
