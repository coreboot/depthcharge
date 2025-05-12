/*
 * Copyright 2015 Google LLC
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

#include <assert.h>
#include <libpayload.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "base/gpt.h"
#include "base/sparse.h"
#include "ctype.h"

GptData *alloc_gpt(BlockDev *bdev)
{
	assert(bdev);

	GptData *gpt = xzalloc(sizeof(*gpt));

	gpt->sector_bytes = bdev->block_size;
	gpt->streaming_drive_sectors = bdev->block_count;
	gpt->gpt_drive_sectors = bdev->block_count;

	if (AllocAndReadGptData(bdev, gpt) != 0) {
		free(gpt);
		return NULL;
	}

	if (GptInit(gpt) != GPT_SUCCESS) {
		free(gpt);
		gpt = NULL;
	}

	return gpt;
}

int free_gpt(BlockDev *bdev, GptData *gpt)
{
	int ret;
	assert(bdev && gpt);

	ret = WriteAndFreeGptData(bdev, gpt);
	free(gpt);

	return ret;
}

char *gpt_get_entry_name(GptEntry *e)
{
	if (IsUnusedEntry(e))
		return NULL;

	return utf16le_to_ascii(e->name, ARRAY_SIZE(e->name));
}

bool gpt_foreach_partition(GptData *gpt, gpt_foreach_callback_t cb, void *ctx)
{
	GptHeader *h = (GptHeader *)gpt->primary_header;
	GptEntry *e;
	bool stop = false;
	for (int i = 0; !stop && i < h->number_of_entries; i++) {
		e = (GptEntry *)&gpt->primary_entries[i * h->size_of_entry];
		char *name = gpt_get_entry_name(e);
		if (name == NULL)
			continue;
		stop = cb(ctx, i, e, name);
		free(name);
	}
	return stop;
}

struct find_partition_ctx {
	const char *name;
	GptEntry *result;
};

static bool find_partition_callback(void *ctx, int index, GptEntry *e,
				    char *partition_name)
{
	struct find_partition_ctx *fpctx = (struct find_partition_ctx *)ctx;

	if (!strcmp(fpctx->name, partition_name)) {
		fpctx->result = e;
		return true;
	}

	return false;
}
GptEntry *gpt_find_partition(GptData *gpt, const char *partition_name)
{
	struct find_partition_ctx fp = {.name = partition_name,
					.result = NULL};
	gpt_foreach_partition(gpt, find_partition_callback, &fp);
	return fp.result;
}

int gpt_get_number_of_partitions(GptData *gpt)
{
	GptHeader *h = (GptHeader *)gpt->primary_header;

	return h->number_of_entries;
}

GptEntry *gpt_get_partition(GptData *gpt, unsigned int index)
{
	GptHeader *h = (GptHeader *)gpt->primary_header;

	if (index >= h->number_of_entries)
		return NULL;

	return (GptEntry *)&gpt->primary_entries[index * h->size_of_entry];
}

enum gpt_io_ret gpt_read_partition(BlockDev *disk, GptData *gpt, const char *partition_name,
				   const uint64_t blocks_offset, void *data, size_t data_len)
{
	if (data_len % disk->block_size)
		return GPT_IO_SIZE_NOT_ALIGNED;

	GptEntry *e = gpt_find_partition(gpt, partition_name);
	if (!e)
		return GPT_IO_NO_PARTITION;

	const uint64_t space = GptGetEntrySizeLba(e);
	const uint64_t data_blocks = data_len / disk->block_size;
	if (blocks_offset > space || data_blocks > space - blocks_offset)
		return GPT_IO_OUT_OF_RANGE;

	printf("Reading LBA %llu to %llu, num blocks = %llu, data len = %zu, block size = %u\n",
	       e->starting_lba + blocks_offset, e->starting_lba + data_blocks + blocks_offset,
	       data_blocks, data_len, disk->block_size);
	const lba_t blocks_read = disk->ops.read(&disk->ops,
						 e->starting_lba + blocks_offset,
						 data_blocks, data);
	if (blocks_read != data_blocks)
		return GPT_IO_TRANSFER_ERROR;

	return GPT_IO_SUCCESS;
}

enum gpt_io_ret gpt_write_partition(BlockDev *disk, GptData *gpt, const char *partition_name,
				    const uint64_t blocks_offset, void *data, size_t data_len)
{
	GptEntry *e = gpt_find_partition(gpt, partition_name);
	if (!e)
		return GPT_IO_NO_PARTITION;

	uint64_t space = GptGetEntrySizeLba(e);
	const uint64_t start_block = e->starting_lba + blocks_offset;
	if (space < blocks_offset)
		return GPT_IO_OUT_OF_RANGE;
	space -= blocks_offset;

	if (is_sparse_image(data)) {
		printf("Writing sparse image to LBA %llu to %llu\n",
		       start_block, start_block + space);
		return write_sparse_image(disk, start_block, space, data, data_len);
	}

	if (data_len % disk->block_size)
		return GPT_IO_SIZE_NOT_ALIGNED;

	const uint64_t data_blocks = data_len / disk->block_size;
	if (data_blocks > space)
		return GPT_IO_OUT_OF_RANGE;

	printf("Writing LBA %llu to %llu, num blocks = %llu, data "
		 "len = %zu, block size = %u\n",
		 start_block, start_block + data_blocks,
		 data_blocks, data_len, disk->block_size);
	const lba_t blocks_written = disk->ops.write(&disk->ops, start_block,
						     data_blocks, data);
	if (blocks_written != data_blocks)
		return GPT_IO_TRANSFER_ERROR;

	return GPT_IO_SUCCESS;
}

enum gpt_io_ret gpt_erase_partition(BlockDev *disk, GptData *gpt,
		    const char *partition_name)
{
	GptEntry *e = gpt_find_partition(gpt, partition_name);
	if (!e)
		return GPT_IO_NO_PARTITION;

	lba_t space = GptGetEntrySizeLba(e);
	if ((disk->ops.erase == NULL) ||
	    disk->ops.erase(&disk->ops, e->starting_lba, space)) {
		if (blockdev_fill_write(&disk->ops, e->starting_lba, space,
					0xffffffff) != space)
			return GPT_IO_TRANSFER_ERROR;
	}

	return GPT_IO_SUCCESS;
}
