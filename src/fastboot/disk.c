/*
 * Copyright 2021 Google Inc.
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

#include <stdbool.h>
#include <stdio.h>
#include "base/gpt.h"
#include "base/list.h"
#include "ctype.h"
#include "drivers/storage/blockdev.h"
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"
#include "fastboot/sparse.h"
#include "gpt.h"
#include "gpt_misc.h"
#include <libpayload.h>
#include <string.h>

bool fastboot_disk_init(struct fastboot_disk *disk)
{
	memset(disk, 0, sizeof(*disk));
	ListNode *devs;
	uint32_t count = get_all_bdevs(BLOCKDEV_FIXED, &devs);

	if (count != 1) {
		FB_DEBUG("wrong number of fixed disks (found %d, wanted 1)\n",
			 count);
		return false;
	}

	BlockDev *bdev = NULL;
	list_for_each(bdev, *devs, list_node)
	{
		disk->disk = bdev;
		break;
	}

	if (disk->disk == NULL) {
		FB_DEBUG("No disk found\n");
		return false;
	}

	FB_DEBUG("Using disk '%s'\n", disk->disk->name);

	disk->gpt = alloc_gpt(disk->disk);
	if (disk->gpt == NULL) {
		return false;
	}
	return true;
}

void fastboot_disk_destroy(struct fastboot_disk *disk)
{
	free_gpt(disk->disk, disk->gpt);
}

bool fastboot_disk_foreach_partition(struct fastboot_disk *disk,
				     disk_foreach_callback_t cb, void *ctx)
{
	GptHeader *h = (GptHeader *)disk->gpt->primary_header;
	GptEntry *e;
	Guid empty;
	memset(&empty, 0, sizeof(empty));
	bool stop = false;
	for (int i = 0; !stop && i < h->number_of_entries; i++) {
		e = (GptEntry *)&disk->gpt
			    ->primary_entries[i * h->size_of_entry];
		if (!memcmp(&e->type, &empty, sizeof(empty)))
			continue;
		char *name = utf16le_to_ascii(e->name, 36);
		stop = cb(ctx, i, e, name);
		free(name);
	}
	return stop;
}

struct find_partition_ctx {
	const char *name;
	size_t name_len;
	GptEntry *result;
};

static bool find_partition_callback(void *ctx, int index, GptEntry *e,
				    char *partition_name)
{
	struct find_partition_ctx *fpctx = (struct find_partition_ctx *)ctx;

	if (!strncmp(fpctx->name, partition_name, fpctx->name_len)) {
		fpctx->result = e;
		return true;
	}

	return false;
}
GptEntry *fastboot_find_partition(struct fastboot_disk *disk,
				  const char *partition_name,
				  size_t partition_name_len)
{
	struct find_partition_ctx fp = {.name = partition_name,
					.name_len = partition_name_len,
					.result = NULL};
	fastboot_disk_foreach_partition(disk, find_partition_callback, &fp);
	return fp.result;
}

void fastboot_write(fastboot_session_t *fb, struct fastboot_disk *disk,
		    const char *partition_name, size_t name_len, void *data,
		    size_t data_len)
{
	GptEntry *e = fastboot_find_partition(disk, partition_name, name_len);
	if (!e) {
		fastboot_fail(fb, "Could not find partition");
		return;
	}

	uint64_t space = GptGetEntrySizeLba(e);
	uint64_t data_blocks = div_round_up(data_len, disk->disk->block_size);
	if (data_blocks > space) {
		fastboot_fail(fb, "Image is too big");
		return;
	}

	if (is_sparse_image(data)) {
		FB_DEBUG("Writing sparse image to LBA %llu to %llu\n",
			 e->starting_lba, e->ending_lba);
		if (write_sparse_image(fb, disk, e, data, data_len))
			return;
	} else {
		FB_DEBUG("Writing LBA %llu to %llu, num blocks = %llu, data "
			 "len = %zu, block size = %u\n",
			 e->starting_lba, e->starting_lba + data_blocks,
			 data_blocks, data_len, disk->disk->block_size);
		lba_t blocks_written = disk->disk->ops.write(
			&disk->disk->ops, e->starting_lba, data_blocks, data);
		if (blocks_written != data_blocks) {
			fastboot_fail(fb, "Failed to write");
			return;
		}
	}
	fastboot_succeed(fb);
	return;
}

void fastboot_erase(fastboot_session_t *fb, struct fastboot_disk *disk,
		    const char *partition_name, size_t name_len)
{
	GptEntry *e = fastboot_find_partition(disk, partition_name, name_len);
	if (!e) {
		fastboot_fail(fb, "Could not find partition");
		return;
	}

	lba_t space = GptGetEntrySizeLba(e);
	if ((disk->disk->ops.erase == NULL) ||
	    disk->disk->ops.erase(&disk->disk->ops, e->starting_lba, space)) {
		if (disk->disk->ops.fill_write(&disk->disk->ops,
					       e->starting_lba, space,
					       0xffffffff) != space) {
			fastboot_fail(fb, "Failed to erase");
			return;
		}
	}
	fastboot_succeed(fb);
}
