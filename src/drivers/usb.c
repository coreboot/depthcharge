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

#include <assert.h>
#include <libpayload.h>
#include <usb/usbdisk.h>
#include <usb/usbmsc.h>

#include "drivers/blockdev.h"
#include "drivers/usb.h"

BlockDev *usb_drives;
int num_usb_drives;

static lba_t dc_usb_read(BlockDev *dev, lba_t start, lba_t count, void *buffer)
{
	usbdev_t *udev = (usbdev_t *)(dev->dev_data);
	if (readwrite_blocks_512(udev, start, count, cbw_direction_data_in,
				 buffer))
		return 0;
	else
		return count;
}

static lba_t dc_usb_write(BlockDev *dev, lba_t start, lba_t count,
			  const void *buffer)
{
	usbdev_t *udev = (usbdev_t *)(dev->dev_data);
	if (readwrite_blocks_512(udev, start, count, cbw_direction_data_out,
				 (void *)buffer))
		return 0;
	else
		return count;
}

void usbdisk_create(usbdev_t *dev)
{
	BlockDev *bdev = (BlockDev *)malloc(sizeof(BlockDev));
	usbmsc_inst_t *msc = MSC_INST(dev);

	if (!bdev) {
		printf("Failed to allocate USB block device!\n");
		return;
	}

	bdev->name = "USB disk";
	bdev->removable = 1;
	bdev->block_size = msc->blocksize;
	bdev->block_count = msc->numblocks;
	bdev->read = &dc_usb_read;
	bdev->write = &dc_usb_write;
	bdev->dev_data = dev;

	bdev->next = usb_drives;
	usb_drives = bdev;
	num_usb_drives++;

	printf("Added usb disk %d.\n", dev->address);
}

void usbdisk_remove(usbdev_t *dev)
{
	BlockDev **bdev_ptr = &usb_drives;
	BlockDev *bdev = *bdev_ptr;

	// If we know this device is a USB disk, it must have a device address.
	assert(dev->address);

	while (bdev) {
		usbdev_t *udev = (usbdev_t *)bdev->dev_data;

		if (udev->address == dev->address) {
			bdev_ptr = &bdev->next;
			free(bdev);
			assert(num_usb_drives);
			num_usb_drives--;

			printf("Removed usb disk %d.\n", dev->address);
			return;
		}

		bdev_ptr = &bdev->next;
		bdev = *bdev_ptr;
	}

	printf("Failed to find usb disk %d.\n", dev->address);
}
