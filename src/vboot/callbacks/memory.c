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

#include <libpayload.h>
#include <vboot_api.h>

void *VbExMalloc(size_t size)
{
	return malloc(size);
}

void VbExFree(void *ptr)
{
	free(ptr);
}

int Memcmp(const void *src1, const void *src2, size_t n)
{
	return memcmp(src1, src2, n);
}

void *Memset(void *d, const uint8_t c, uint64_t n)
{
	return memset(d, c, n);
}

void *Memcpy(void *dest, const void *src, uint64_t n)
{
	return memcpy(dest, src, n);
}
