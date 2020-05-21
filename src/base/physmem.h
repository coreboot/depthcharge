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

#ifndef __BASE_PHYSMEM_H__
#define __BASE_PHYSMEM_H__

#include <stdint.h>
#include <libpayload.h>

typedef void (*PhysMapFunc)(uint64_t phys_addr, void *s, uint64_t n,
			    void *data);

/*
 * Run a function on physical memory which may not be accessible directly.
 *
 * In this function, it will remapping physical memory when needed and then pass
 * then accessible pointer to the function. Due to the mapping limitation, this
 * function may split physical memory to multiple segment and call `callback`
 * multiple times separately.
 *
 * @param s			The physical address to start.
 * @param n			The number of bytes to operate.
 * @param func	The function which do the actually work.
 * @param data	The data that can be used in func.
 */
void arch_phys_map(uint64_t s, uint64_t n, PhysMapFunc func, void *data);

#endif /* __BASE_PHYSMEM_H__ */
