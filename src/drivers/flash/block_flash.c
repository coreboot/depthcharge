/*
 * Copyright 2015 Google Inc.
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

#include "drivers/flash/block_flash.h"

static inline FlashBlockDev *block_flash_get_bdev(BlockDevOps *me)
{
	FlashBlockDev *flash = container_of(me, FlashBlockDev, dev.ops);
	return flash;
}

static lba_t block_flash_read(BlockDevOps *me, lba_t start, lba_t count,
			      void *buffer)
{
	FlashBlockDev *flash = block_flash_get_bdev(me);
	FlashOps *ops = flash->ops;
	size_t block_size = flash->dev.block_size;
	size_t size_bytes = count * block_size;

	void *data = flash_read_ops(ops, start * block_size, size_bytes);

	if (data == NULL)
		return 0;

	memcpy(buffer, data, size_bytes);

	return count;
}

static lba_t block_flash_write(BlockDevOps *me, lba_t start, lba_t count,
			       const void *buffer)
{
	FlashBlockDev *flash = block_flash_get_bdev(me);
	FlashOps *ops = flash->ops;
	size_t block_size = flash->dev.block_size;
	size_t todo = count * block_size;
	size_t curr_ptr = start * block_size;

	if (flash_rewrite_ops(ops, curr_ptr, todo, buffer) != todo)
		return 0;

	return count;
}

static lba_t block_flash_erase(BlockDevOps *me, lba_t start, lba_t count)
{
	FlashBlockDev *flash = block_flash_get_bdev(me);
	FlashOps *ops = flash->ops;
	size_t block_size = flash->dev.block_size;
	size_t todo = count * block_size;
	size_t curr_ptr = start * block_size;

	if (flash_erase_ops(ops, curr_ptr, todo) != todo)
		return 0;

	return count;
}

static lba_t block_flash_fill_write(BlockDevOps *me, lba_t start, lba_t count,
				    uint32_t fill_pattern)
{
	FlashBlockDev *flash = block_flash_get_bdev(me);
	FlashOps *ops = flash->ops;
	size_t block_size = flash->dev.block_size;
	size_t todo = count * block_size;
	size_t curr_ptr = start * block_size;
	int ret = 0;

	/*
	 * If fill_pattern is 0xFFFFFFFF, we can safely return after erase since
	 * it sets all bytes to 0xFF.
	 */
	if (fill_pattern == 0xFFFFFFFF) {
		if (flash_erase_ops(ops, curr_ptr, todo) != todo)
			return ret;
		return count;
	}

	/*
	 * We allocate max 4 MiB buffer on heap and set it to fill_byte and
	 * perform flash_write operation using this 4MiB buffer until requested
	 * size on disk is written by the fill byte.
	 */
	lba_t heap_lba = (4 * MiB) / block_size;
	lba_t buffer_lba = MIN(heap_lba, count);
	size_t buffer_bytes = buffer_lba * block_size;
	size_t buffer_words = buffer_bytes / sizeof(uint32_t);
	uint32_t *buffer = xmalloc(buffer_bytes);
	uint32_t *ptr = buffer;

	for ( ; buffer_words ; buffer_words--)
		*ptr++ = fill_pattern;

	do {
		size_t curr_bytes = MIN(buffer_bytes, todo);

		if (flash_rewrite_ops(ops, curr_ptr, curr_bytes, buffer)
		    != curr_bytes)
			goto cleanup;

		todo -= curr_bytes;
		curr_ptr += curr_bytes;
	} while (todo > 0);

	ret = count;

 cleanup:
	free(buffer);
	return ret;
}

static int block_flash_is_bdev_owned(BlockDevCtrlrOps *me, BlockDev *bdev)
{
	FlashBlockDev *flash = container_of(me, FlashBlockDev, ctrlr.ops);

	return &flash->dev == bdev;
}

FlashBlockDev *block_flash_register_nor(FlashOps *ops)
{
	FlashBlockDev *flash = xmalloc(sizeof(*flash));
	memset(flash, 0, sizeof(*flash));

	flash->ops = ops;

	flash->ctrlr.ops.update = NULL;
	flash->ctrlr.ops.is_bdev_owned = block_flash_is_bdev_owned;
	flash->ctrlr.need_update = 0;

	flash->dev.name = "spi nor flash device";
	flash->dev.removable = 0;

	flash->dev.block_size = ops->sector_size;
	flash->dev.block_count = ops->sector_count;
	flash->dev.stream_block_count = 0;
	flash->dev.ops.read = block_flash_read;
	flash->dev.ops.write = block_flash_write;
	flash->dev.ops.erase = block_flash_erase;
	flash->dev.ops.fill_write = block_flash_fill_write;
	flash->dev.ops.new_stream = NULL;

	list_insert_after(&flash->dev.list_node, &fixed_block_devices);
	list_insert_after(&flash->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return flash;
}
