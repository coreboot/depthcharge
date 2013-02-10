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

#include <assert.h>
#include <libpayload.h>
#include <vboot_api.h>

#include "drivers/storage/blockdev.h"

static void setup_vb_disk_info(VbDiskInfo *disk, BlockDev *bdev)
{
	disk->name = bdev->name;
	disk->handle = (VbExDiskHandle_t)bdev;
	disk->bytes_per_lba = bdev->block_size;
	disk->lba_count = bdev->block_count;
	disk->flags = bdev->removable ? VB_DISK_FLAG_REMOVABLE :
					VB_DISK_FLAG_FIXED;
}

VbError_t VbExDiskGetInfo(VbDiskInfo **info_ptr, uint32_t *count,
			  uint32_t disk_flags)
{
	*count = 0;
	ListNode *node, *list;
	static int need_init = 1;

	if (need_init) {
		block_dev_init();
		need_init = 0;
	}

	if (disk_flags & VB_DISK_FLAG_FIXED) {
		list = &fixed_block_devices;
	} else {
		block_dev_refresh();
		list = &removable_block_devices;
	}

	// Count the devices.
	for (node = list->next; node; node = node->next, (*count)++) {}

	// Allocate enough VbDiskInfo structures.
	VbDiskInfo *disk = malloc(sizeof(VbDiskInfo) * *count);
	*info_ptr = disk;

	// Fill them from the BlockDev structures.
	for (node = list->next; node; node = node->next) {
		BlockDev *bdev = container_of(node, BlockDev, list_node);
		setup_vb_disk_info(disk++, bdev);
	}
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskFreeInfo(VbDiskInfo *infos,
			   VbExDiskHandle_t preserve_handle)
{
	free(infos);
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskRead(VbExDiskHandle_t handle, uint64_t lba_start,
		       uint64_t lba_count, void *buffer)
{
	BlockDev *dev = (BlockDev *)handle;
	if (dev->read(dev, lba_start, lba_count, buffer) != lba_count) {
		printf("Read failed.\n");
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskWrite(VbExDiskHandle_t handle, uint64_t lba_start,
			uint64_t lba_count, const void *buffer)
{
	BlockDev *dev = (BlockDev *)handle;
	if (dev->write(dev, lba_start, lba_count, buffer) != lba_count) {
		printf("Write failed.\n");
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}
