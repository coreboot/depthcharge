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

#include "base/cleanup_funcs.h"
#include "drivers/bus/usb/usb.h"

ListNode generic_usb_drivers;
ListNode usb_host_controllers;

void usb_generic_create(usbdev_t *dev)
{
	// Allocate a structure to keep track of this device.
	GenericUsbDevice *gdev = xmalloc(sizeof(*gdev));
	gdev->dev = dev;

	// Check if any generic USB driver wants to claim it.
	GenericUsbDriver *driver;
	list_for_each(driver, generic_usb_drivers, list_node) {
		gdev->driver = driver;
		// If so, attach our info and return.
		if (driver->probe && driver->probe(gdev)) {
			dev->data = gdev;
			return;
		}
	}

	// Nobody wanted it so free resources and return.
	free(gdev);
}

void usb_generic_remove(usbdev_t *dev)
{
	// If this device was never claimed, ignore it.
	if (!dev->data)
		return;

	// If the driver for this device has a "remove" function, call it.
	GenericUsbDevice *gdev = dev->data;
	assert(gdev->driver);
	if (gdev->driver->remove)
		gdev->driver->remove(gdev);

	// Free our bookeeping data structure and return.
	free(gdev);
	dev->data = NULL;
}

UsbHostController *new_usb_hc(hc_type type, uintptr_t bar)
{
	UsbHostController *hc = xzalloc(sizeof(*hc));
	hc->type = type;
	hc->bar = (void *)bar;
	return hc;
}

void set_usb_init_callback(UsbHostController *hc, UsbHcCallback *callback)
{
	hc->init_callback = callback;
}

static int dc_usb_shutdown(struct CleanupFunc *cleanup, CleanupType type)
{
	printf("Shutting down all USB controllers.\n");
	usb_exit();
	return 0;
}

void dc_usb_initialize(void)
{
	static const char * hc_types[] = {[UHCI] = "UHCI", [OHCI] = "OHCI",
					  [EHCI] = "EHCI", [XHCI] = "XHCI"};
	static int need_init = 1;
	static CleanupFunc cleanup = {
		&dc_usb_shutdown,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	if (need_init) {
		usb_initialize();
		need_init = 0;
		list_insert_after(&cleanup.list_node, &cleanup_funcs);

		UsbHostController *hc;
		list_for_each(hc, usb_host_controllers, list_node) {
			printf("Initializing %s USB controller at %p.\n",
					hc_types[hc->type], hc->bar);
			usb_add_mmio_hc(hc->type, hc->bar);
			if (hc->init_callback)
				hc->init_callback(hc);
		}
	}
}
