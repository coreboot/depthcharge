/*
 * Copyright 2012 Google LLC
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

#ifndef __DRIVERS_BUS_USB_USB_H__
#define __DRIVERS_BUS_USB_USB_H__

#include <commonlib/list.h>
#include <usb/usb.h>

struct GenericUsbDevice;
struct UsbHostController;
typedef struct GenericUsbDevice GenericUsbDevice;
typedef void (UsbHcCallback)(struct UsbHostController *);

typedef struct GenericUsbDriver {
	int (*probe)(GenericUsbDevice *dev);
	void (*remove)(GenericUsbDevice *dev);

	struct list_node list_node;
} GenericUsbDriver;

extern struct list_node generic_usb_drivers;

typedef struct GenericUsbDevice {
	GenericUsbDriver *driver;
	usbdev_t *dev;
	void *dev_data;
} GenericUsbDevice;

typedef struct UsbHostController {
	hc_type type;
	void *bar;
	struct list_node list_node;
	UsbHcCallback *init_callback;
} UsbHostController;

extern struct list_node usb_host_controllers;

typedef void (*UsbCallback)(void *);
typedef struct UsbCallbackData {
	struct list_node list_node;
	UsbCallback callback;
	void *data;
} UsbCallbackData;

UsbHostController *new_usb_hc(hc_type type, uintptr_t bar);
void set_usb_init_callback(UsbHostController *hc, UsbHcCallback *callback);
void dc_usb_initialize(void);

void usb_poll_prepare(void);

void usb_register_init_callback(UsbCallbackData *toRegister);
void usb_register_poll_callback(UsbCallbackData *toRegister);

#endif /* __DRIVERS_BUS_USB_USB_H__ */
