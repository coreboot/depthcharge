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
#include <usb/usb.h>
#include <usb/usbdisk.h>
#include <usb/usbmsc.h>

#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/usb.h"

typedef struct UsbDrive {
	BlockDev dev;
	usbdev_t *udev;
} UsbDrive;

static lba_t dc_usb_read(BlockDevOps *me, lba_t start, lba_t count,
			 void *buffer)
{
	UsbDrive *drive = container_of(me, UsbDrive, dev.ops);
	if (readwrite_blocks_512(drive->udev, start, count,
				 cbw_direction_data_in, buffer))
		return 0;
	else
		return count;
}

static lba_t dc_usb_write(BlockDevOps *me, lba_t start, lba_t count,
			  const void *buffer)
{
	UsbDrive *drive = container_of(me, UsbDrive, dev.ops);
	if (readwrite_blocks_512(drive->udev, start, count,
				 cbw_direction_data_out, (void *)buffer))
		return 0;
	else
		return count;
}

void usbdisk_create(usbdev_t *dev)
{
	usbmsc_inst_t *msc = MSC_INST(dev);
	UsbDrive *drive = xzalloc(sizeof(*drive));
	static const int name_size = 16;
	char *name = xmalloc(name_size);
	snprintf(name, name_size, "USB disk %d", dev->address);
	drive->dev.ops.read = &dc_usb_read;
	drive->dev.ops.write = &dc_usb_write;
	drive->dev.name = name;
	drive->dev.removable = 1;
	drive->dev.block_size = msc->blocksize;
	drive->dev.block_count = msc->numblocks;
	drive->udev = dev;

	msc->data = drive;

	list_insert_after(&drive->dev.list_node, &removable_block_devices);

	printf("Added %s.\n", name);
}

void usbdisk_remove(usbdev_t *dev)
{
	usbmsc_inst_t *msc = MSC_INST(dev);
	UsbDrive *drive = (UsbDrive *)(msc->data);
	assert(drive);

	list_remove(&drive->dev.list_node);
	printf("Removed %s.\n", drive->dev.name);
	free((void *)drive->dev.name);
	free(drive);
}

static int usb_ctrlr_update(BlockDevCtrlrOps *me)
{
	dc_usb_initialize();
	usb_poll();
	return 0;
}

static int usb_ctrlr_register(void)
{
	static BlockDevCtrlr usb =
	{
		.ops = {
			.update = &usb_ctrlr_update
		},
		.need_update = 1
	};

	list_insert_after(&usb.list_node, &removable_block_dev_controllers);
	return 0;
}

INIT_FUNC(usb_ctrlr_register);
