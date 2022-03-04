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
 */

#include <libpayload.h>

#include "drivers/flash/flash.h"

int __must_check flash_read_ops(FlashOps *ops, void *buffer, uint32_t offset,
				uint32_t size)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	return ops->read(ops, buffer, offset, size);
}

int __must_check flash_write_ops(FlashOps *ops, const void *buffer,
				 uint32_t offset, uint32_t size)
{
	die_if(!ops, "%s: No flash ops set.\n", __func__);
	if (ops->write)
		return ops->write(ops, buffer, offset, size);

	return 0;
}

int __must_check flash_erase_ops(FlashOps *ops, uint32_t offset, uint32_t size)
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
	int value = flash_read_status_ops(ops);

	/*
	* NOR flash chips we currently support write-protection for al have SRP0
	* in bit position 7. If this changes, we'll need to read the chip ID and
	* take appropriate action (like with error checking).
	*/
	return value & (1 << 7) ? 1 : 0;
}

static inline uint32_t flash_sector_size_ops(FlashOps *ops)
{
	return ops->sector_size;
}

int __must_check flash_rewrite_ops(FlashOps *ops, const void *buffer,
				   uint32_t start, uint32_t length)
{
	uint32_t sector_size = flash_sector_size_ops(ops);
	uint32_t initial_start = ALIGN_DOWN(start, sector_size);
	uint32_t final_end = ALIGN_UP(start + length, sector_size);
	uint32_t full_length = final_end - initial_start;
	void *dev_buffer = NULL;
	int result = -1;
	int ret;

	if (initial_start != start || final_end != full_length) {
		dev_buffer = xzalloc(full_length);
		ret = flash_read_ops(ops, dev_buffer, initial_start,
				     full_length);
		if (ret != full_length) {
			printf("rewriting failed in read ret=%d\n", ret);
			goto end;
		}
		memcpy(dev_buffer + (start - initial_start), buffer, length);
		buffer = dev_buffer;
	}

	ret = flash_erase_ops(ops, initial_start, full_length);
	if (ret != full_length) {
		printf("rewriting SPI GPT failed in erase ret=%d\n", ret);
		goto end;
	}

	ret = flash_write_ops(ops, buffer, initial_start, full_length);
	if (ret != full_length) {
		printf("rewriting failed in write ret=%d\n", ret);
		goto end;
	}

	result = length;
end:
	free(dev_buffer);
	return result;
}

static FlashOps *flash_ops;

void flash_set_ops(FlashOps *ops)
{
	die_if(flash_ops, "Flash ops already set.\n");
	flash_ops = ops;
}

int __must_check flash_read(void *buffer, uint32_t offset, uint32_t size)
{
	return flash_read_ops(flash_ops, buffer, offset, size);
}

int __must_check flash_write(const void *buffer, uint32_t offset, uint32_t size)
{
	return flash_write_ops(flash_ops, buffer, offset, size);
}

int __must_check flash_erase(uint32_t offset, uint32_t size)
{
	return flash_erase_ops(flash_ops, offset, size);
}

uint32_t flash_sector_size(void)
{
	return flash_sector_size_ops(flash_ops);
}

int __must_check flash_rewrite(const void *buffer, uint32_t start,
			       uint32_t length)
{
	return flash_rewrite_ops(flash_ops, buffer, start, length);
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

JedecFlashId flash_read_id(void)
{
	JedecFlashId empty = { 0 };

	die_if(!flash_ops, "%s: No flash ops set.\n", __func__);
	if (flash_ops->read_id)
		return flash_ops->read_id(flash_ops);

	return empty;
}

int flash_set_wp_enabled(void)
{
	JedecFlashId id;
	int i;

	die_if(!flash_ops, "%s: No flash ops set.\n", __func__);

	id = flash_read_id();

	for (i = 0; flash_protection_list[i].id.vendor != 0; i++) {
		if (flash_protection_list[i].id.vendor != id.vendor ||
		    flash_protection_list[i].id.model != id.model)
			continue;

		return flash_write_status(
				flash_protection_list[i].wp_status_value);
	}

	return -1;
}
