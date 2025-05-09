/*
 * Copyright 2021 Google LLC
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

#include <commonlib/list.h>
#include <libpayload.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vb2_gpt.h>

#include "base/gpt.h"
#include "ctype.h"
#include "drivers/storage/blockdev.h"
#include "fastboot/disk.h"
#include "fastboot/fastboot.h"

bool fastboot_disk_init(struct fastboot_disk *disk)
{
	memset(disk, 0, sizeof(*disk));
	struct list_node *devs;
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

void fastboot_write(struct FastbootOps *fb, struct fastboot_disk *disk,
		    const char *partition_name, const uint64_t blocks_offset,
		    void *data, size_t data_len)
{
	switch (gpt_write_partition(disk->disk, disk->gpt, partition_name, blocks_offset,
				    data, data_len)) {
	case GPT_IO_SUCCESS:
		fastboot_succeed(fb);
		break;
	case GPT_IO_SIZE_NOT_ALIGNED:
		fastboot_fail(fb, "Buffer size %lu not aligned to block size of %u", data_len,
			      fb->disk->block_size);
		break;
	case GPT_IO_NO_PARTITION:
		fastboot_fail(fb, "Could not find partition \"%s\"", partition_name);
		break;
	case GPT_IO_OUT_OF_RANGE:
		fastboot_fail(fb, "Image is too big");
		break;
	case GPT_IO_TRANSFER_ERROR:
		fastboot_fail(fb, "Failed to write");
		break;
	case GPT_IO_SPARSE_TOO_SMALL:
		fastboot_fail(fb, "Sparse image ended abruptly");
		break;
	case GPT_IO_SPARSE_WRONG_HEADER_SIZE:
		fastboot_fail(fb, "Wrong size of sparse header/chunk header");
		break;
	case GPT_IO_SPARSE_BLOCK_SIZE_NOT_ALIGNED:
		fastboot_fail(fb, "Sparse block size not aligned to block size of %u",
			      disk->disk->block_size);
		break;
	case GPT_IO_SPARSE_WRONG_CHUNK_SIZE:
		fastboot_fail(fb, "Sparse chunk size is wrong");
		break;
	case GPT_IO_SPARSE_WRONG_CHUNK_TYPE:
		fastboot_fail(fb, "Unrecognised sparse chunk type");
		break;
	default:
		fastboot_fail(fb, "Unknown error while writing");
	}
}

void fastboot_erase(struct FastbootOps *fb, struct fastboot_disk *disk,
		    const char *partition_name)
{
	switch (gpt_erase_partition(disk->disk, disk->gpt, partition_name)) {
	case GPT_IO_SUCCESS:
		fastboot_succeed(fb);
		break;
	case GPT_IO_NO_PARTITION:
		fastboot_fail(fb, "Could not find partition \"%s\"\n", partition_name);
		break;
	case GPT_IO_TRANSFER_ERROR:
		fastboot_fail(fb, "Failed to erase");
		break;
	default:
		fastboot_fail(fb, "Unknown error while writing");
	}
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
	gpt_foreach_partition(disk->gpt, check_kernel_partition_info, &result);
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
	if (gpt_foreach_partition(disk->gpt, find_slot_callback, &ctx))
		return ctx.target_entry;

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
	gpt_foreach_partition(disk->gpt, disable_all_callback, disk->gpt);
}
