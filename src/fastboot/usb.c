// SPDX-License-Identifier: GPL-2.0
/* Copyright 2024 Google LLC. */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <usb/usb.h>

#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "fastboot/fastboot.h"
#include "fastboot/usb.h"

/* USB device ID that can be used as fastboot transport */
#define PL27A1_VID 0x067b
#define PL27A1_PID 0x27a7
/* Google VID is used for all ALinks */
#define GOOGLE_VID 0x18d1
#define ALINK_CABLE_PID 0x506d
#define ALINK_DONGLE_PID 0x506e
#define ALINKQ_DONGLE_PID 0x506f

/* Indexes of endpoints used for fastboot transport */
#define FB_EP_IN 0x89
#define FB_EP_OUT 0x08

typedef struct UsbFastbootDevice {
	ListNode list_node;
	struct FastbootOps fb_session;
	endpoint_t *bulk_in;
	endpoint_t *bulk_out;
	usbdev_t *usb_dev;
} UsbFastbootDevice;

static bool is_usb_fastboot_in_use(UsbFastbootDevice *usb_fb_dev)
{
	return usb_fb_dev->bulk_in != NULL && usb_fb_dev->bulk_out != NULL;
}

static int usb_fastboot_send(struct FastbootOps *fb, void *buf, size_t len)
{
	UsbFastbootDevice *usb_fb_dev = container_of(fb, UsbFastbootDevice, fb_session);

	return usb_fb_dev->usb_dev->controller->bulk(usb_fb_dev->bulk_out, len, buf, 0) < 0;
}

static int usb_fastboot_recv(UsbFastbootDevice *usb_fb_dev, void *buf, size_t *len,
			     int maxlen)
{
	int32_t buf_size;

	buf_size = usb_fb_dev->usb_dev->controller->bulk(usb_fb_dev->bulk_in, maxlen, buf, 0);
	if (buf_size < 0) {
		printf("USBFB: Bulk read error %#x\n", buf_size);
		return 1;
	}
	*len = buf_size;

	return 0;
}

static void usb_fastboot_release(struct FastbootOps *fb)
{
	UsbFastbootDevice *usb_fb_dev = container_of(fb, UsbFastbootDevice, fb_session);

	if (usb_fb_dev->usb_dev == NULL) {
		/* Device is no longer present, free the whole structure */
		free(usb_fb_dev);
	} else {
		/* Mark the device as not in use */
		usb_fb_dev->bulk_in = NULL;
		usb_fb_dev->bulk_out = NULL;
	}
}

static void usb_fastboot_poll(struct FastbootOps *fb)
{
	usb_poll();
}

static void usb_fastboot_dev_poll(usbdev_t *dev)
{
	char packet_buffer[FASTBOOT_COMMAND_MAX];
	size_t len;
	GenericUsbDevice *gen_dev = (GenericUsbDevice *)dev->data;
	UsbFastbootDevice *usb_fb_dev = (UsbFastbootDevice *)gen_dev->dev_data;

	/* Process only Alink in use */
	if (!is_usb_fastboot_in_use(usb_fb_dev))
		return;

	if (usb_fastboot_recv(usb_fb_dev, packet_buffer, &len, sizeof(packet_buffer)))
		return;

	/* Fastboot protocol documentation says that zero-length packets should be ignored */
	if (len == 0)
		return;

	fastboot_handle_packet(&usb_fb_dev->fb_session, packet_buffer, len);
}

static int usb_fastboot_init_endpoints(usbdev_t *dev, UsbFastbootDevice *usb_fb_dev)
{
	endpoint_t *in = NULL;
	endpoint_t *out = NULL;

	for (int i = 0; i < dev->num_endp; i++) {
		if (!in && dev->endpoints[i].endpoint == FB_EP_IN &&
		    dev->endpoints[i].type == BULK &&
		    dev->endpoints[i].direction == IN)
			in = &dev->endpoints[i];
		else if (!out && dev->endpoints[i].endpoint == FB_EP_OUT &&
			 dev->endpoints[i].type == BULK &&
			 dev->endpoints[i].direction == OUT)
			out = &dev->endpoints[i];

		if (in && out) {
			usb_fb_dev->bulk_in = in;
			usb_fb_dev->bulk_out = out;
			return 0;
		}
	}

	printf("USBFB: Problem with the endpoints.\n");
	return 1;
}

/* List of all Alink devices probed */
static ListNode alink_devices;

static struct FastbootOps *usb_fastboot_get_dev(void)
{
	UsbFastbootDevice *list_dev;

	/* Find first unused and valid Alink cable */
	list_for_each(list_dev, alink_devices, list_node) {
		if (is_usb_fastboot_in_use(list_dev))
			continue;

		if (!usb_fastboot_init_endpoints(list_dev->usb_dev, list_dev)){
			/* This Alink seems to be OK */
			return &list_dev->fb_session;
		}
	}

	return NULL;
}

static int usb_fastboot_probe(GenericUsbDevice *dev)
{
	UsbFastbootDevice *usb_fb_dev;
	device_descriptor_t *dd = (device_descriptor_t *)dev->dev->descriptor;

	if (!(dd->idVendor == PL27A1_VID && dd->idProduct == PL27A1_PID) &&
	    !(dd->idVendor == GOOGLE_VID && (dd->idProduct == ALINK_CABLE_PID ||
					     dd->idProduct == ALINK_DONGLE_PID ||
					     dd->idProduct == ALINKQ_DONGLE_PID)))
		return 0;

	usb_fb_dev = xzalloc(sizeof(UsbFastbootDevice));

	dev->dev_data = usb_fb_dev;
	usb_fb_dev->usb_dev = dev->dev;
	usb_fb_dev->usb_dev->poll = usb_fastboot_dev_poll;
	usb_fb_dev->fb_session.poll = usb_fastboot_poll;
	usb_fb_dev->fb_session.send_packet = usb_fastboot_send;
	usb_fb_dev->fb_session.release = usb_fastboot_release;

	list_insert_after(&usb_fb_dev->list_node, &alink_devices);

	return 1;
}

static void usb_fastboot_remove(GenericUsbDevice *dev)
{
	UsbFastbootDevice *usb_fb_dev = (UsbFastbootDevice *)dev->dev_data;

	list_remove(&usb_fb_dev->list_node);

	if (is_usb_fastboot_in_use(usb_fb_dev)) {
		/* Disconnect fastboot and wait for FastbootOps.release */
		usb_fb_dev->fb_session.state = FINISHED;
		usb_fb_dev->bulk_in = NULL;
		usb_fb_dev->bulk_out = NULL;
		usb_fb_dev->usb_dev = NULL;
	} else {
		free(usb_fb_dev);
	}
}

static GenericUsbDriver usb_fastboot_driver = {
	.probe = &usb_fastboot_probe,
	.remove = &usb_fastboot_remove,
};

static int usb_fastboot_driver_register(void)
{
	list_insert_after(&usb_fastboot_driver.list_node,
			  &generic_usb_drivers);

	return 0;
}
INIT_FUNC(usb_fastboot_driver_register);

struct FastbootOps *fastboot_setup_usb(void)
{
	struct FastbootOps *fb = NULL;
	int try = 0;

	do {
		/* Set up the usb stack */
		usb_poll();

		fb = usb_fastboot_get_dev();
		try++;
	} while (fb == NULL && try <= 3);

	return fb;
}
