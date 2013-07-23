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

static FlashOps *flash_ops;

int flash_set_ops(FlashOps *ops)
{
	if (flash_ops) {
		printf("Flash ops already set.\n");
		return -1;
	}
	flash_ops = ops;

	return 0;
}

void *flash_read(uint32_t offset, uint32_t size)
{
	return flash_ops->read(flash_ops, offset, size);
}
