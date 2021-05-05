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

/************************* SLOT LOGIC ******************************/

// Returns 0 if the partition is not valid.
char get_slot_for_partition_name(GptEntry *e, char *partition_name)
{
	const Guid cros_kernel_guid = GPT_ENT_TYPE_CHROMEOS_KERNEL;
	if (memcmp(&e->type, &cros_kernel_guid, sizeof(cros_kernel_guid)) != 0)
		return 0;
	int len = strlen(partition_name);
	// expect partition names ending with -X or _x
	if (partition_name[len - 2] != '-' && partition_name[len - 2] != '_') {
		FB_DEBUG("ignoring kernel name '%s' - missing suffix\n",
			 partition_name);
		return 0;
	}

	if (!isalpha(partition_name[len - 1])) {
		FB_DEBUG("ignoring kernel name '%s' - slot is not a letter\n",
			 partition_name);
		return 0;
	}

	// make all slots lowercase.
	char slot = partition_name[len - 1];
	if (slot < 'a')
		slot += ('a' - 'A');
	return slot;
}

struct kpi_ctx {
	int kernel_count;
	// bitmap of present slots.
	// bit 0 = slot a
	// bit 1 = slot b
	// ...
	// bit 26 = slot z
	uint32_t present;
};
static bool check_kernel_partition_info(void *ctx, int index, GptEntry *e,
					char *partition_name)
{
	struct kpi_ctx *info = (struct kpi_ctx *)ctx;
	char slot = get_slot_for_partition_name(e, partition_name);
	if (slot == 0)
		return false;

	FB_DEBUG("kernel name '%s' => slot %c or %d\n", partition_name, slot,
		 slot - 'a');
	uint32_t bit = 1 << (slot - 'a');
	if (info->present & bit) {
		FB_DEBUG("Multiple slots for '%c'!\n", slot);
	}
	info->present |= bit;
	info->kernel_count++;

	return false;
}
int fastboot_get_slot_count(struct fastboot_disk *disk)
{
	struct kpi_ctx result = {
		.kernel_count = 0,
	};
	fastboot_disk_foreach_partition(disk, check_kernel_partition_info,
					&result);
	return result.kernel_count;
}

struct find_slot_ctx {
	GptEntry *target_entry;
	int count;
	int desired_slot;
};
static bool find_slot_callback(void *ctx, int index, GptEntry *e,
			       char *partition_name)
{
	if (get_slot_for_partition_name(e, partition_name) == 0)
		return false;

	struct find_slot_ctx *fs = (struct find_slot_ctx *)ctx;
	fs->count++;
	if (fs->count == fs->desired_slot) {
		fs->target_entry = e;
		return true;
	}

	return false;
}

GptEntry *fastboot_get_kernel_for_slot(struct fastboot_disk *disk, char slot)
{

	if (slot < 'a') {
		return NULL;
	}
	struct find_slot_ctx ctx = {
		.target_entry = NULL,
		.count = 0,
		.desired_slot = 1 + (slot - 'a'),
	};
	if (fastboot_disk_foreach_partition(disk, find_slot_callback, &ctx)) {
		return ctx.target_entry;
	}
	return NULL;
}

static bool disable_all_callback(void *ctx, int index, GptEntry *e,
				 char *partition_name)
{
	if (get_slot_for_partition_name(e, partition_name) == 0)
		return false;

	GptUpdateKernelWithEntry((GptData *)ctx, e, GPT_UPDATE_ENTRY_INVALID);
	return false;
}
void fastboot_slots_disable_all(struct fastboot_disk *disk)
{
	fastboot_disk_foreach_partition(disk, disable_all_callback, disk->gpt);
}
