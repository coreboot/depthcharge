/*
 * Copyright 2014 Google LLC
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

#ifndef __DRIVERS_NET_USB_ETH_H__
#define __DRIVERS_NET_USB_ETH_H__

#include "drivers/net/net.h"

typedef struct UsbEthId {
	uint16_t vendor;
	uint16_t product;
} UsbEthId;

typedef struct UsbEthDriver {
	const UsbEthId *supported_ids;
	int num_supported_ids;
	const struct NetDeviceOps ops;

	/* Called to allocate a new device instance */
	NetDevice *(*alloc)(struct GenericUsbDevice *dev);
	/* Called to free a device instance */
	void (*free)(NetDevice *net_dev);

	struct list_node list_node;
} UsbEthDriver;

extern struct list_node usb_eth_drivers;

/*
 * Empirically chosen USB ETH 100ms poll interval. It gives plenty of time for USB device to
 * response while being short enough to allow to response for other actions that may happen
 * simultaneously (e.g. interactions with UI).
 *
 * It is up to the driver to choose final timeout value, this is only a recommendation.
 */
#define USB_ETH_BULK_POLL_TIMEOUT_US 100000

int usb_eth_read_reg(usbdev_t *dev, uint8_t request, uint16_t value,
			    uint16_t index, uint16_t length, void *data);
int usb_eth_write_reg(usbdev_t *dev, uint8_t request, uint16_t value,
			    uint16_t index, uint16_t length, void *data);
int usb_eth_init_endpoints(usbdev_t *dev, endpoint_t **in, int in_idx,
				  endpoint_t **out, int out_idx);

#endif /* __DRIVERS_NET_USB_ETH_H__ */
