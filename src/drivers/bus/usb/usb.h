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

#ifndef __DRIVERS_BUS_USB_USB_H__
#define __DRIVERS_BUS_USB_USB_H__

#include <usb/usb.h>

#include "base/list.h"

struct GenericUsbDevice;
struct UsbHostController;
typedef struct GenericUsbDevice GenericUsbDevice;
typedef void (UsbHcCallback)(struct UsbHostController *);

typedef struct GenericUsbDriver {
	int (*probe)(GenericUsbDevice *dev);
	void (*remove)(GenericUsbDevice *dev);

	ListNode list_node;
} GenericUsbDriver;

extern ListNode generic_usb_drivers;

typedef struct GenericUsbDevice {
	GenericUsbDriver *driver;
	usbdev_t *dev;
	void *dev_data;
} GenericUsbDevice;

typedef struct UsbHostController {
	hc_type type;
	void *bar;
	ListNode list_node;
	UsbHcCallback *init_callback;
} UsbHostController;

extern ListNode usb_host_controllers;

UsbHostController *new_usb_hc(hc_type type, uintptr_t bar);
void set_usb_init_callback(UsbHostController *hc, UsbHcCallback *callback);
void dc_usb_initialize(void);

#endif /* __DRIVERS_BUS_USB_USB_H__ */
