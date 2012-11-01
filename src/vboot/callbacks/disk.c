/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

#include <libpayload.h>
#include <usb/usb.h>
#include <vboot_api.h>

#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/usb.h"

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

	if (disk_flags & VB_DISK_FLAG_FIXED) {
		VbDiskInfo *disk = malloc(sizeof(VbDiskInfo));
		*info_ptr = disk;
		*count += 1;

		setup_vb_disk_info(disk, &sata_drive);
	} else {
		usb_poll();

		VbDiskInfo *disk =
			malloc(sizeof(VbDiskInfo) * num_usb_drives);
		*info_ptr = disk;
		*count += num_usb_drives;

		for (BlockDev *bdev = usb_drives; bdev; bdev = bdev->next)
			setup_vb_disk_info(disk, bdev);
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
