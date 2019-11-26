/*
 * Copyright 2014 Google Inc.
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

#include <assert.h>
#include <endian.h>
#include <libpayload.h>
#include <stdint.h>
#include <usb/usb.h>

#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/net/net.h"
#include "drivers/net/mii.h"
#include "drivers/net/usb_eth.h"

int usb_eth_read_reg(usbdev_t *dev, uint8_t request, uint16_t value,
			    uint16_t index, uint16_t length, void *data)
{
	dev_req_t dev_req;
	dev_req.req_recp = dev_recp;
	dev_req.req_type = vendor_type;
	dev_req.data_dir = device_to_host;
	dev_req.bRequest = request;
	dev_req.wValue = value;
	dev_req.wIndex = index;
	dev_req.wLength = length;

	return (dev->controller->control(dev, IN, sizeof(dev_req), &dev_req,
					 dev_req.wLength,
					 (uint8_t *)data) < 0);
}

int usb_eth_write_reg(usbdev_t *dev, uint8_t request, uint16_t value,
			    uint16_t index, uint16_t length, void *data)
{
	dev_req_t dev_req;
	dev_req.req_recp = dev_recp;
	dev_req.req_type = vendor_type;
	dev_req.data_dir = host_to_device;
	dev_req.bRequest = request;
	dev_req.wValue = value;
	dev_req.wIndex = index;
	dev_req.wLength = length;

	return (dev->controller->control(dev, OUT, sizeof(dev_req), &dev_req,
					 dev_req.wLength,
					 (uint8_t *)data) < 0);
}

int usb_eth_init_endpoints(usbdev_t *dev, endpoint_t **in, int in_idx,
				  endpoint_t **out, int out_idx)
{
	*in = &dev->endpoints[in_idx];
	*out = &dev->endpoints[out_idx];

	if (dev->num_endp < in_idx || dev->num_endp < out_idx ||
	    (*in)->type != BULK || (*out)->type != BULK ||
	    (*in)->direction != IN || (*out)->direction != OUT) {
		printf("Problem with the endpoints.\n");
		return 1;
	}

	return 0;
}

ListNode usb_eth_drivers;

static NetDevice *usb_eth_net_device;

static int usb_eth_probe(GenericUsbDevice *dev)
{
	int i;
	UsbEthDevice *eth_dev;

	device_descriptor_t *dd = (device_descriptor_t *)dev->dev->descriptor;
	list_for_each(eth_dev, usb_eth_drivers, list_node) {
		for (i = 0; i < eth_dev->num_supported_ids; i++) {
			if (dd->idVendor == eth_dev->supported_ids[i].vendor &&
			   dd->idProduct == eth_dev->supported_ids[i].product) {
				usb_eth_net_device = &eth_dev->net_dev;
				usb_eth_net_device->dev_data = dev;
				if (eth_dev->init(dev)) {
					break;
				} else {
					net_add_device(usb_eth_net_device);
					return 1;
				}
			}
		}
	}
	return 0;
}

static void usb_eth_remove(GenericUsbDevice *dev)
{
	net_remove_device(usb_eth_net_device);
}

static void usb_net_poller(struct NetPoller *poller)
{
	usb_poll();
}

static NetPoller net_poller = {
	.poll = usb_net_poller
};

static GenericUsbDriver usb_eth_driver = {
	.probe = &usb_eth_probe,
	.remove = &usb_eth_remove,
};

static int usb_eth_driver_register(void)
{
	list_insert_after(&usb_eth_driver.list_node,
			  &generic_usb_drivers);

	list_insert_after(&net_poller.list_node, &net_pollers);

	return 0;
}

INIT_FUNC(usb_eth_driver_register);
