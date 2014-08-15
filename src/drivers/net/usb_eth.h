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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_NET_USB_ETH_H__
#define __DRIVERS_NET_USB_ETH_H__

#include "drivers/net/net.h"

typedef struct UsbEthId {
	uint16_t vendor;
	uint16_t product;
} UsbEthId;

typedef struct UsbEthDevice {
	int (*init)(GenericUsbDevice *dev);
	NetDevice net_dev;
	const UsbEthId *supported_ids;
	int num_supported_ids;

	ListNode list_node;
} UsbEthDevice;

extern ListNode usb_eth_drivers;

int usb_eth_read_reg(usbdev_t *dev, uint8_t request, uint16_t value,
			    uint16_t index, uint16_t length, void *data);
int usb_eth_write_reg(usbdev_t *dev, uint8_t request, uint16_t value,
			    uint16_t index, uint16_t length, void *data);
int usb_eth_init_endpoints(usbdev_t *dev, endpoint_t **in, int in_idx,
				  endpoint_t **out, int out_idx);

#endif /* __DRIVERS_NET_USB_ETH_H__ */
