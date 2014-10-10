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

	// Figure out which devices/controllers to operate on.
	ListNode *ctrlrs, *devs;
	if (disk_flags & VB_DISK_FLAG_FIXED) {
		devs = &fixed_block_devices;
		ctrlrs = &fixed_block_dev_controllers;
	} else {
		devs = &removable_block_devices;
		ctrlrs = &removable_block_dev_controllers;
	}

	// Update any controllers that need it.
	BlockDevCtrlr *ctrlr;
	list_for_each(ctrlr, *ctrlrs, list_node) {
		if (ctrlr->ops.update && ctrlr->need_update &&
		    ctrlr->ops.update(&ctrlr->ops)) {
			printf("Updating a storage controller failed. "
			       "Skipping.\n");
		}
	}

	// Count the devices.
	for (ListNode *node = devs->next; node; node = node->next, (*count)++)
		;

	// Allocate enough VbDiskInfo structures.
	VbDiskInfo *disk = NULL;
	if (*count)
		disk = xmalloc(sizeof(VbDiskInfo) * *count);

	*info_ptr = disk;

	// Fill them from the BlockDev structures.
	BlockDev *bdev;
	list_for_each(bdev, *devs, list_node)
		setup_vb_disk_info(disk++, bdev);

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
	BlockDevOps *ops = &((BlockDev *)handle)->ops;
	if (ops->read(ops, lba_start, lba_count, buffer) != lba_count) {
		printf("Read failed.\n");
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

VbError_t VbExDiskWrite(VbExDiskHandle_t handle, uint64_t lba_start,
			uint64_t lba_count, const void *buffer)
{
	BlockDevOps *ops = &((BlockDev *)handle)->ops;
	if (ops->write(ops, lba_start, lba_count, buffer) != lba_count) {
		printf("Write failed.\n");
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

/*
 * Simple implementation of new streaming APIs.  This turns them into calls to
 * the sector-based disk read/write functions above.  This will be replaced
 * imminently with fancier streaming.  In the meantime, this will allow the
 * vboot_reference change which uses the streaming APIs to commit.
 */

/* The stub implementation assumes 512-byte disk sectors */
#define LBA_BYTES 512

/* Internal struct to simulate a stream for sector-based disks */
struct disk_stream {
	/* Disk handle */
	VbExDiskHandle_t handle;

	/* Next sector to read */
	uint64_t sector;

	/* Number of sectors left in partition */
	uint64_t sectors_left;
};

VbError_t VbExStreamOpen(VbExDiskHandle_t handle, uint64_t lba_start,
			 uint64_t lba_count, VbExStream_t *stream)
{
	struct disk_stream *s;

	if (!handle) {
		*stream = NULL;
		return VBERROR_UNKNOWN;
	}

	s = VbExMalloc(sizeof(*s));
	s->handle = handle;
	s->sector = lba_start;
	s->sectors_left = lba_count;

	*stream = (void *)s;

	return VBERROR_SUCCESS;
}

VbError_t VbExStreamRead(VbExStream_t stream, uint32_t bytes, void *buffer)
{
	struct disk_stream *s = (struct disk_stream *)stream;
	uint64_t sectors;
	VbError_t rv;

	if (!s)
		return VBERROR_UNKNOWN;

	/* For now, require reads to be a multiple of the LBA size */
	if (bytes % LBA_BYTES)
		return VBERROR_UNKNOWN;

	/* Fail on overflow */
	sectors = bytes / LBA_BYTES;
	if (sectors > s->sectors_left)
		return VBERROR_UNKNOWN;

	rv = VbExDiskRead(s->handle, s->sector, sectors, buffer);
	if (rv != VBERROR_SUCCESS)
		return rv;

	s->sector += sectors;
	s->sectors_left -= sectors;

	return VBERROR_SUCCESS;
}

void VbExStreamClose(VbExStream_t stream)
{
	struct disk_stream *s = (struct disk_stream *)stream;

	/* Allow freeing a null pointer */
	if (!s)
		return;

	VbExFree(s);
}
