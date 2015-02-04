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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>

uint64_t arch_phys_memset(uint64_t start, int c, uint64_t size)
{
	uint64_t end = start + size;

	if (end && end < start) {
		// If the range wraps around 0, set the upper and lower parts
		// separately.
		memset(phys_to_virt(0), c, (size_t)end);
		memset(phys_to_virt(start), c, (size_t)(0 - start));
	} else {
		// No wrap, set everything at once.
		memset(phys_to_virt(start), c, (size_t)size);
	}
	return start;
}
