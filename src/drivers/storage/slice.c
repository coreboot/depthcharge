// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include <stdio.h>

#include "drivers/storage/blockdev.h"

typedef struct BdevSlice {
	BlockDev dev;
	BlockDev *parent_dev;
	lba_t offset;
} BdevSlice;

static bool is_out_of_bounds(BlockDev *dev, lba_t start, lba_t count)
{
	return (start > dev->block_count || count > dev->block_count - start);
}

static lba_t slice_read(struct BlockDevOps *me, lba_t start, lba_t count,
			void *buffer)
{
	BdevSlice *bp = container_of(me, BdevSlice, dev.ops);

	if (is_out_of_bounds(&bp->dev, start, count))
		return 0;

	return bp->parent_dev->ops.read(&bp->parent_dev->ops, start + bp->offset,
					count, buffer);
}

static lba_t slice_write(struct BlockDevOps *me, lba_t start, lba_t count,
			 const void *buffer)
{
	BdevSlice *bp = container_of(me, BdevSlice, dev.ops);

	if (is_out_of_bounds(&bp->dev, start, count))
		return 0;

	return bp->parent_dev->ops.write(&bp->parent_dev->ops, start + bp->offset,
					 count, buffer);
}

static lba_t slice_erase(struct BlockDevOps *me, lba_t start, lba_t count)
{
	BdevSlice *bp = container_of(me, BdevSlice, dev.ops);

	if (is_out_of_bounds(&bp->dev, start, count))
		return 0;

	return bp->parent_dev->ops.erase(&bp->parent_dev->ops, start + bp->offset,
					 count);
}

static StreamOps *slice_new_stream(struct BlockDevOps *me, lba_t start,
				   lba_t count)
{
	BdevSlice *bp = container_of(me, BdevSlice, dev.ops);

	if (is_out_of_bounds(&bp->dev, start, count))
		return NULL;

	return bp->parent_dev->ops.new_stream(&bp->parent_dev->ops,
					      start + bp->offset, count);
}

BlockDev *new_blockdev_slice(BlockDev *parent, lba_t offset, lba_t size)
{
	assert(parent);

	if (offset > parent->block_count || size > parent->block_count - offset) {
		printf("%s: slice out of bounds: 0x%llx+0x%llx > 0x%llx\n",
		       parent->name, offset, size, parent->block_count);
		return NULL;
	}

	BdevSlice *slice = xzalloc(sizeof(*slice));
	slice->dev.name = "bdev_slice";
	slice->dev.ops.read = parent->ops.read ? &slice_read : NULL;
	slice->dev.ops.write =  parent->ops.write ? &slice_write : NULL;
	slice->dev.ops.erase =  parent->ops.erase ? &slice_erase : NULL;
	slice->dev.ops.new_stream =  parent->ops.new_stream ? &slice_new_stream : NULL;
	slice->dev.removable = parent->removable;
	slice->dev.block_size = parent->block_size;
	slice->dev.block_count = size;
	slice->dev.stream_block_count = 0;
	slice->parent_dev = parent;
	slice->offset = offset;

	return &slice->dev;
}
