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

#include "drivers/storage/blockdev.h"

#include <assert.h>
#include <libpayload.h>
#include <stdio.h>

ListNode fixed_block_devices;
ListNode removable_block_devices;

ListNode fixed_block_dev_controllers;
ListNode removable_block_dev_controllers;

typedef struct {
	StreamOps stream;
	BlockDev *blockdev;
	lba_t current_sector;
	lba_t end_sector;
} SimpleStream;

uint64_t simple_stream_read(StreamOps *me, uint64_t count, void *buffer)
{
	SimpleStream *stream = container_of(me, SimpleStream, stream);
	unsigned block_size = stream->blockdev->block_size;

	/* TODO(dehrenberg): implement buffering so that unaligned reads are
	 * possible because alignment constraints are obscure */
	if (count & (block_size - 1)) {
		printf("read_stream_simple(%lld) not LBA multiple\n", count);
		return 0;
	}

	uint64_t sectors = count / block_size;
	if (sectors > stream->end_sector - stream->current_sector) {
		printf("read_stream_simple past the end, "
		       "end_sector=%lld, current_sector=%lld, sectors=%lld\n",
		       stream->end_sector, stream->current_sector, sectors);
		return 0;
	}

	int ret = stream->blockdev->ops.read(&stream->blockdev->ops,
					     stream->current_sector, sectors,
					     buffer);
	if (ret != sectors)
		return ret;

	stream->current_sector += sectors;
	return count;
}

static void simple_stream_close(StreamOps *me)
{
	SimpleStream *stream = container_of(me, SimpleStream, stream);
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
	/* Check that block size is a power of 2 */
	assert((blockdev->block_size & (blockdev->block_size - 1)) == 0);
	return &stream->stream;
}
