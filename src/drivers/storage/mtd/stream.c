/*
 * Copyright 2014 Google Inc.
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

#include "drivers/storage/stream.h"
#include "drivers/storage/mtd/stream.h"
#include "base/container_of.h"

typedef struct {
	StreamOps ops;
	MtdDev *mtd;
	uint64_t offset;
	uint64_t limit;
} MtdStream;

typedef struct {
	StreamCtrlr ops;
	MtdDevCtrlr *mtd_ctrlr;
} MtdStreamCtrlr;

#define stream_debug(...) do { if (0) printf(__VA_ARGS__); } while (0)

/* returns amount written on success */
static uint64_t read_mtd_stream(StreamOps *dev, uint64_t count,
				void *buffer) {
	/* It is assumed that this function will be called in a single-threaded
	 * manner so no synchronization is attempted at this level. */
	stream_debug("read_mtd_stream buffer=%p\n", buffer);

	MtdStream *mtd_stream = container_of(dev, MtdStream, ops);
	MtdDev *mtd = mtd_stream->mtd;
	assert(mtd != NULL);

	/* TODO(dehrenberg): before any release is made:
	 * Implement unaligned reads in a generic way, in a buffer managed by
	 * the general MTD layer. */
	if (count % mtd->writesize != 0) {
		printf("misaligned stream read, count=0x%llx, writesize=0x%x",
		       count, mtd->writesize);
		return -EINVAL;
	}

	uint8_t *cur_buffer = buffer;
	uint64_t remaining = count;

	while (remaining > 0) {
		stream_debug("Iteration 0x%llx 0x%llx 0x%llx %p\n",
			     remaining, mtd_stream->offset, mtd_stream->limit,
			     cur_buffer);
		assert(remaining + mtd_stream->offset <= mtd_stream->limit);
		if (remaining + mtd_stream->offset == mtd_stream->limit) {
			printf(
			       "read out of bounds remaining=0x%llx offset=0x%llx limit=0x%llx",
			       remaining, mtd_stream->offset,
			       mtd_stream->limit);
			return 0;
		}

		/* Skip a bad block */
		if (mtd_stream->offset % mtd->erasesize == 0) {
			if (mtd->block_isbad(mtd, mtd_stream->offset)) {
				printf("skipping bad block at 0x%llx\n",
				       mtd_stream->offset);
				mtd_stream->offset += mtd->erasesize;
				continue;
			}
		}

		/* Read up to the end of the current erase block or to
		 * the end of the user request, whichever comes first. */
		int length = MIN(ALIGN_UP(mtd_stream->offset + 1,
					  mtd->erasesize),
				 mtd_stream->offset + remaining)
				- mtd_stream->offset;
		unsigned int retlen;
		int ret = mtd->read(mtd, mtd_stream->offset, length, &retlen,
				    cur_buffer);
		if (ret < 0 && ret != -EUCLEAN) {
			printf("Read failure!! ret=%d\n", ret);
			return ret;
		}
		if (retlen != length) {
			printf("Read failure!! retlen=%d\n", retlen);
			return count - remaining + retlen;
		}
		mtd_stream->offset += length;
		remaining -= length;
		cur_buffer += length;
	}

	return count;
}

static void close_mtd_stream(StreamOps *me)
{
	free(container_of(me, MtdStream, ops));
}

static StreamOps *open_mtd_stream(StreamCtrlr *me, uint64_t offset,
				  uint64_t size)
{
	printf("Opening a stream offset=%llx size=%llx\n", offset, size);
	MtdStreamCtrlr *ctrlr = container_of(me, MtdStreamCtrlr, ops);

	int ret = ctrlr->mtd_ctrlr->update(ctrlr->mtd_ctrlr);
	if (ret)
		return NULL;
	MtdDev *info = ctrlr->mtd_ctrlr->dev;
	if (offset % info->erasesize != 0
			|| size % info->erasesize != 0) {
		printf(
		       "misaligned partition stream: offset=%llx erasesize=%x size=%llx",
		       offset, info->erasesize, size);
		return NULL;
	}
	if (offset + size > info->size) {
		printf(
		       "stream goes beyond device size: offset=%llx device size=%llx part size=%llx",
		       offset, info->size, size);
		return NULL;
	}

	MtdStream *dev = xzalloc(sizeof(*dev));
	dev->ops.read = read_mtd_stream;
	dev->ops.close = close_mtd_stream;
	dev->mtd = ctrlr->mtd_ctrlr->dev;
	dev->offset = offset;
	dev->limit = offset + size;
	return &dev->ops;
}

static uint64_t size_mtd_stream(StreamCtrlr *me)
{
	MtdStreamCtrlr *ctrlr = container_of(me, MtdStreamCtrlr, ops);

	int ret = ctrlr->mtd_ctrlr->update(ctrlr->mtd_ctrlr);
	if (ret)
		return -1;	/* can't return ret because could be positive */

	MtdDev *info = ctrlr->mtd_ctrlr->dev;
	return info->size;
}

StreamCtrlr *new_mtd_stream(MtdDevCtrlr *mtd)
{
	MtdStreamCtrlr *ctrlr = xzalloc(sizeof(*ctrlr));
	ctrlr->mtd_ctrlr = mtd;
	ctrlr->ops.open = open_mtd_stream;
	ctrlr->ops.size = size_mtd_stream;
	return &ctrlr->ops;
}
