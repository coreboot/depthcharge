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

#include "drivers/bus/usb/usb.h"

void dc_usb_initialize(void)
{
	static int need_init = 1;

	if (need_init) {
		usb_initialize();
		need_init = 0;
	}
}

void usb_generic_create(usbdev_t *dev)
{
	device_descriptor_t *dd = (device_descriptor_t *)dev->descriptor;
	printf("USB device added with ID %04x:%04x\n",
		dd->idVendor, dd->idProduct);
}

void usb_generic_remove(usbdev_t *dev)
{
	device_descriptor_t *dd = (device_descriptor_t *)dev->descriptor;
	printf("USB device removed with ID %04x:%04x\n",
		dd->idVendor, dd->idProduct);
}
