/*
 * Copyright 2012 Google LLC
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

#include "drivers/storage/blockdev.h"

#include <assert.h>
#include <libpayload.h>
#include <stdio.h>

#include "drivers/storage/bouncebuf.h"

struct list_node fixed_block_devices;
struct list_node removable_block_devices;

struct list_node fixed_block_dev_controllers;
struct list_node removable_block_dev_controllers;

typedef struct {
	StreamOps stream;
	BlockDev *blockdev;
	lba_t current_sector;
	lba_t end_sector;
	void *block_buf;
	size_t buf_used;
} SimpleStream;

/*
 * Read bytes from a stream. Returns number of bytes successfully written to
 * the buffer. If buffer is NULL, then skip 'count' number of bytes from the stream.
 */
uint64_t simple_stream_read(StreamOps *me, uint64_t count, void *buffer)
{
	SimpleStream *stream = container_of(me, SimpleStream, stream);
	unsigned block_size = stream->blockdev->block_size;
	uint64_t bytes_read = 0;

	/* Handle bytes that are left from previous partial block read */
	if (stream->buf_used < block_size) {
		bytes_read = MIN(count, block_size - stream->buf_used);
		if (buffer) {
			memcpy(buffer, stream->block_buf + stream->buf_used, bytes_read);
			buffer += bytes_read;
		}
		stream->buf_used += bytes_read;
		count -= bytes_read;
		if (stream->buf_used == block_size)
			stream->current_sector++;
	}

	if (count == 0)
		return bytes_read;

	/* Handle sectors that are block aligned */
	uint64_t sectors = count / block_size;
	if (sectors > stream->end_sector - stream->current_sector) {
		printf("read_stream_simple past the end, "
		       "end_sector=%lld, current_sector=%lld, sectors=%lld\n",
		       stream->end_sector, stream->current_sector, sectors);
		return bytes_read;
	}

	if (buffer && sectors > 0) {
		if (stream->blockdev->ops.read(&stream->blockdev->ops,
					       stream->current_sector, sectors,
					       buffer) != sectors)
			return bytes_read;
		buffer += sectors * block_size;
	}
	bytes_read += sectors * block_size;
	count -= sectors * block_size;
	stream->current_sector += sectors;

	/* Handle partial block read */
	if (count) {
		if (stream->current_sector == stream->end_sector) {
			printf("read_stream_simple past the end, "
			       "end_sector=%lld, current_sector=%lld\n",
			       stream->end_sector, stream->current_sector);
			return bytes_read;
		}
		if (stream->blockdev->ops.read(&stream->blockdev->ops,
					       stream->current_sector, 1,
					       stream->block_buf) != 1)
			return bytes_read;
		if (buffer)
			memcpy(buffer, stream->block_buf, count);
		bytes_read += count;
		stream->buf_used = count;
	}

	return bytes_read;
}

uint64_t simple_stream_skip(StreamOps *me, uint64_t count)
{
	/* Calling read with NULL buffer will skip bytes from stream */
	return simple_stream_read(me, count, NULL);
}

static void simple_stream_close(StreamOps *me)
{
	SimpleStream *stream = container_of(me, SimpleStream, stream);

	free(stream->block_buf);
	free(stream);
}

StreamOps *new_simple_stream(BlockDevOps *me, lba_t start, lba_t count)
{
	BlockDev *blockdev = (BlockDev *)me;
	SimpleStream *stream = xzalloc(sizeof(*stream));
	stream->blockdev = blockdev;
	stream->current_sector = start;
	stream->end_sector = start + count;
	stream->stream.read = simple_stream_read;
	stream->stream.close = simple_stream_close;
	stream->stream.skip = simple_stream_skip;
	/* Mark that there is no data in the block_buf to read right away */
	stream->buf_used = blockdev->block_size;
	stream->block_buf = xmalloc(blockdev->block_size);
	/* Check that block size is a power of 2 */
	assert((blockdev->block_size & (blockdev->block_size - 1)) == 0);
	return &stream->stream;
}

lba_t blockdev_fill_write(BlockDevOps *me, lba_t start, lba_t count, uint32_t fill_pattern)
{
	BlockDev *blockdev = (BlockDev *)me;
	const uint32_t block_size = blockdev->block_size;

	if (count <= 0)
		/* Nothing to fill */
		return 0;

	/*
	 * We allocate max 4 MiB buffer on heap and set it to fill_pattern and
	 * perform write operation using this 4MiB buffer until requested
	 * size on disk is written by the fill byte.
	 *
	 * 4MiB was chosen after repeating several experiments on MMC with the max
	 * buffer size to be used. Using 1 lba i.e. block_size buffer results in
	 * very large fill_write time. On the other hand, choosing 4MiB, 8MiB or
	 * even 128 Mib resulted in similar write times. With 2MiB, the
	 * fill_write time increased by several seconds. So, 4MiB was chosen as
	 * the default max buffer size.
	 */
	const lba_t heap_blocks = (4 * MiB) / block_size;
	const lba_t buffer_blocks = MIN(heap_blocks, count);
	lba_t todo = count;

	const uint64_t buffer_bytes = buffer_blocks * block_size;
	uint64_t buffer_words = buffer_bytes / sizeof(uint32_t);
	uint32_t *buffer = xmemalign(ARCH_DMA_MINALIGN, buffer_bytes);
	uint32_t *ptr = buffer;

	if (buffer == NULL) {
		printf("%s: cannot allocate buffer\n", __func__);
		return 0;
	}

	while (buffer_words--)
		*ptr++ = fill_pattern;

	lba_t blocks_to_write;
	lba_t done;

	do {
		blocks_to_write = MIN(buffer_blocks, todo);
		done = me->write(me, start, blocks_to_write, buffer);

		todo -= done;
		start += done;
	} while (todo > 0 && done == blocks_to_write);

	free(buffer);

	return count - todo;
}

int get_all_bdevs(blockdev_type_t type, struct list_node **bdevs)
{
	struct list_node *ctrlrs, *devs;

	if (type == BLOCKDEV_FIXED) {
		devs = &fixed_block_devices;
		ctrlrs = &fixed_block_dev_controllers;
	} else {
		devs = &removable_block_devices;
		ctrlrs = &removable_block_dev_controllers;
	}

	/* Update any controllers that need it. */
	BlockDevCtrlr *ctrlr;
	list_for_each(ctrlr, *ctrlrs, list_node) {
		if (ctrlr->ops.update && ctrlr->need_update &&
		    ctrlr->ops.update(&ctrlr->ops))
			printf("Updating storage controller failed.\n");
	}

	if (bdevs)
		*bdevs = devs;
	return list_length(devs);
}
