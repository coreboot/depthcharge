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

void flash_set_ops(FlashOps *ops)
{
	die_if(flash_ops, "Flash ops already set.\n");
	flash_ops = ops;
}

void *flash_read(uint32_t offset, uint32_t size)
{
	die_if(!flash_ops, "%s: No flash ops set.\n", __func__);
	return flash_ops->read(flash_ops, offset, size);
}

int flash_write(uint32_t offset, uint32_t size, const void *buffer)
{
	die_if(!flash_ops, "%s: No flash ops set.\n", __func__);
	if (flash_ops->write)
		return flash_ops->write(flash_ops, buffer, offset, size);

	return 0;
}
