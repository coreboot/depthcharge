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
	flash->dev.ops.new_stream = NULL;

	list_insert_after(&flash->dev.list_node, &fixed_block_devices);
	list_insert_after(&flash->ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return flash;
}
