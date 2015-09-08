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

void *flash_read_ops(FlashOps *ops, uint32_t offset, uint32_t size)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	return ops->read(ops, offset, size);
}

int flash_write_ops(FlashOps *ops, uint32_t offset, uint32_t size,
		    const void *buffer)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	if (ops->write)
		return ops->write(ops, buffer, offset, size);

	return 0;
}

int flash_erase_ops(FlashOps *ops, uint32_t offset, uint32_t size)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	if (ops->erase)
		return ops->erase(ops, offset, size);

	return 0;
}

static inline int flash_write_status_ops(FlashOps *ops, uint8_t status)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	if (ops->write_status)
		return ops->write_status(ops, status);

	return 0;
}

static inline int flash_read_status_ops(FlashOps *ops)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	if (ops->read_status)
		return ops->read_status(ops);

	return -1;
}

static inline int flash_is_wp_enabled_ops(FlashOps *ops)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	if (ops->is_wp_enabled)
		return ops->is_wp_enabled(ops);

	return -1;
}

static inline uint32_t flash_sector_size_ops(FlashOps *ops)
{
	return ops->sector_size;
}

int flash_rewrite_ops(FlashOps *ops, uint32_t start, uint32_t length,
		      const void *buffer)
{
	uint32_t sector_size = flash_sector_size_ops(ops);
	uint32_t initial_start = ALIGN_DOWN(start, sector_size);
	uint32_t final_end = ALIGN_UP(start + length, sector_size);
	uint32_t full_length = final_end - initial_start;
	if (initial_start != start || final_end != full_length) {
		char *dev_buffer = flash_read_ops(ops, initial_start,
						  full_length);
		memcpy(dev_buffer + (start - initial_start), buffer, length);
		buffer = dev_buffer;
	}
	int ret = flash_erase_ops(ops, initial_start, full_length);
	if (ret != full_length) {
		printf("rewriting SPI GPT failed in erase ret=%d\n", ret);
		return ret;
	}
	ret = flash_write_ops(ops, initial_start, full_length, buffer);
	if (ret != full_length) {
		printf("rewriting failed in write ret=%d\n", ret);
		/* Can't return ret directly because it might equal length
		 * and be interpreted as success. */
		return ret == length ? -1 : ret;
	}
	return length;
}

static FlashOps *flash_ops;

void flash_set_ops(FlashOps *ops)
{
	die_if(flash_ops, "Flash ops already set.\n");
	flash_ops = ops;
}

void *flash_read(uint32_t offset, uint32_t size)
{
	return flash_read_ops(flash_ops, offset, size);
}

int flash_write(uint32_t offset, uint32_t size, const void *buffer)
{
	return flash_write_ops(flash_ops, offset, size, buffer);
}

int flash_erase(uint32_t offset, uint32_t size)
{
	return flash_erase_ops(flash_ops, offset, size);
}

uint32_t flash_sector_size(void)
{
	return flash_sector_size_ops(flash_ops);
}

int flash_rewrite(uint32_t start, uint32_t length, const void *buffer)
{
	return flash_rewrite_ops(flash_ops, start, length, buffer);
}

int flash_write_status(uint8_t status)
{
	return flash_write_status_ops(flash_ops, status);
}

int flash_read_status(void)
{
	return flash_read_status_ops(flash_ops);
}

int flash_is_wp_enabled(void)
{
	return flash_is_wp_enabled_ops(flash_ops);
}
