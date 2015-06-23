/*
 * Copyright 2015 Google Inc.
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
#include <config.h>
#include <libpayload.h>

#include "fastboot/udc.h"
#include "image/symbols.h"

static device_descriptor_t device_descriptor = {
	.bLength = sizeof(device_descriptor_t),
	.bDescriptorType = 1,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = CONFIG_FASTBOOT_USBVID,
	.idProduct = CONFIG_FASTBOOT_USBDID,
};

/* Length of OUT transfer. Because these transfers
 * are named from the host's point of view, OUT
 * is "receive" for us.
 */
static int out_length;

static void fastboot_packet(struct usbdev_ctrl *this, int ep, int in_dir,
	void *data, int len)
{
	if (ep != 1) return; /* none of ours */

	if (in_dir == 0) {
		// tell usb_gadget_recv() that the transfer is done
		out_length = len;
	}
}

/* configuration descriptor's wTotalLength and bConfigurationValue are
 * filled in by driver
 */
static struct usbdev_configuration fastboot_config = {
	.descriptor = {
		.bLength = sizeof(configuration_descriptor_t),
		.bDescriptorType = 2,
		.bNumInterfaces = 1,
		.iConfiguration = 0,
		.bmAttributes = 0xc0, /* self powered */
		.bMaxPower = 0, /* 0*2mA draw */
	},
	.interfaces = {{
		.descriptor = {
			.bLength = sizeof(interface_descriptor_t),
			.bDescriptorType = 4,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 0,
			.bNumEndpoints = 2,
			.bInterfaceClass = 0xff,
			.bInterfaceSubClass = 0x42,
			.bInterfaceProtocol = 0x03,
			.iInterface = 0,
		},
		.eps = (endpoint_descriptor_t[]){{
			.bLength = sizeof(endpoint_descriptor_t),
			.bDescriptorType = 5,
			.bEndpointAddress = 1, // 1-OUT
			.bmAttributes = 2, // Bulk
			.wMaxPacketSize = 512,
			.bInterval = 9,
		},
		{
			.bLength = sizeof(endpoint_descriptor_t),
			.bDescriptorType = 5,
			.bEndpointAddress = 0x81, // 1-IN
			.bmAttributes = 2, // Bulk
			.wMaxPacketSize = 512,
			.bInterval = 9,
		}},
		.init = NULL,
		.handle_packet = fastboot_packet,
		.handle_setup = NULL,
	},
	},
};

static struct usbdev_ctrl *udc = NULL;

/* UDC string table operations */
#define MAX_STR_COUNT		256
static const char *str_table[MAX_STR_COUNT];

/* Index starts from 1 because 0 has special meaning in USB string table. */
#define STRING_TABLE_EMPTY	1
static uint8_t str_table_index = STRING_TABLE_EMPTY;

static uint8_t udc_add_string(const char *str)
{
	assert(str_table_index < MAX_STR_COUNT);

	uint8_t ret = str_table_index;

	str_table[str_table_index] = str;
	str_table_index++;

	return ret;
}

static inline uint8_t udc_string_table_count(void)
{
	return str_table_index;
}

static inline const char **udc_string_table(void)
{
	return str_table;
}

static inline void udc_string_table_reset(void)
{
	str_table_index = STRING_TABLE_EMPTY;
}

int usb_gadget_init(void)
{
	if (lib_sysinfo.serialno)
		device_descriptor.iSerialNumber =
			udc_add_string(lib_sysinfo.serialno);

	fastboot_chipset_init(&udc, &device_descriptor);
	if (udc == NULL)
		return 0;

	uint8_t count = udc_string_table_count();

	if (count != STRING_TABLE_EMPTY)
		udc->add_strings(LANG_EN_US, count, udc_string_table());

	udc->add_gadget(udc, &fastboot_config);
	return 1;
}

size_t usb_gadget_send(const char *msg, size_t size)
{
	void *tmp = udc->alloc_data(size);
	memcpy(tmp, msg, size);
	udc->enqueue_packet(udc, 1, 1, tmp, size, 0, 1);
	return size;
}

static void usb_gadget_force_shutdown(void)
{
	udc_string_table_reset();
	udc->force_shutdown(udc);
}

size_t usb_gadget_recv(void *pkt, size_t size)
{
	/* max 64 packets at once */
	const uint32_t blocksize = 64 * 512;
	size_t total = 0;
	size_t remaining = size;
	if (size > blocksize)
		size = blocksize;

	void *tmp = udc->alloc_data(size);

	/* wait until the device is ready */
	while (!udc->initialized)
		udc->poll(udc);

	while (remaining != 0) {
		out_length = 0;
		if (size > remaining)
			size = remaining;

		udc->enqueue_packet(udc, 1, 0, tmp, size, 0, 0);
		while ((out_length == 0) && udc->initialized)
			udc->poll(udc);

		/* If lost connection, re-initialize gadget mode. */
		if (!udc->initialized) {
			usb_gadget_force_shutdown();
			usb_gadget_init();
			return 0;
		}

		memcpy(pkt, tmp, out_length);

		total += out_length;
		remaining -= out_length;
		pkt += out_length;

		/* short transfer should only happen at the end */
		if (out_length < size)
			break;
	}
	udc->free_data(tmp);
	return total;
}

void usb_gadget_stop(void)
{
	udc_string_table_reset();
	udc->shutdown(udc);
}
