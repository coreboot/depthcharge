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

#include "base/timestamp.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/stream.h"

static void setup_vb_disk_info(VbDiskInfo *disk, BlockDev *bdev)
{
	disk->name = bdev->name;
	disk->handle = (VbExDiskHandle_t)bdev;
	disk->bytes_per_lba = bdev->block_size;
	disk->lba_count = bdev->block_count;
	disk->streaming_lba_count = bdev->stream_block_count;
	disk->flags = bdev->removable ? VB_DISK_FLAG_REMOVABLE :
					VB_DISK_FLAG_FIXED;
	if (bdev->external_gpt)
		disk->flags |= VB_DISK_FLAG_EXTERNAL_GPT;
}

VbError_t VbExDiskGetInfo(VbDiskInfo **info_ptr, uint32_t *count,
			  uint32_t disk_flags)
{
	*count = 0;

	blockdev_type_t bd_type;
	ListNode *devs;

	if (disk_flags & VB_DISK_FLAG_FIXED)
		bd_type = BLOCKDEV_FIXED;
	else
		bd_type = BLOCKDEV_REMOVABLE;

	*count = get_all_bdevs(bd_type, &devs);

	// Only log for fixed disks to avoid spamming timestamps in recovery.
	if (disk_flags & VB_DISK_FLAG_FIXED)
		timestamp_add_now(TS_VB_STORAGE_INIT_DONE);

	// Allocate enough VbDiskInfo structures.
	VbDiskInfo *disk = NULL;
	if (*count)
		disk = xzalloc(sizeof(VbDiskInfo) * *count);

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

VbError_t VbExStreamOpen(VbExDiskHandle_t handle, uint64_t lba_start,
			 uint64_t lba_count, VbExStream_t *stream_ptr)
{
	BlockDevOps *ops = &((BlockDev *)handle)->ops;
	*stream_ptr = (VbExStream_t)ops->new_stream(ops, lba_start, lba_count);
	if (*stream_ptr == NULL) {
		printf("Stream open failed.\n");
		return VBERROR_UNKNOWN;
	}
	return VBERROR_SUCCESS;
}

VbError_t VbExStreamRead(VbExStream_t stream, uint32_t bytes, void *buffer)
{
	StreamOps *dev = (StreamOps *)stream;
	int ret = dev->read(dev, bytes, buffer);
	if (ret != bytes) {
		printf("Stream read failed.\n");
		return VBERROR_UNKNOWN;
	}

	// Vboot first reads some headers from the front of the kernel partition
	// and then the whole kernel body in one call. We assume that any read
	// larger than 1MB is the kernel body, and thus the last read.
	if (bytes > MiB)
		timestamp_add_now(TS_VB_READ_KERNEL_DONE);

	return VBERROR_SUCCESS;
}

void VbExStreamClose(VbExStream_t stream)
{
	StreamOps *dev = (StreamOps *)stream;
	dev->close(dev);
}
