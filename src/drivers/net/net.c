/*
 * Copyright 2013 Google Inc.
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

#include <assert.h>
#include <endian.h>
#include <libpayload.h>

#include "drivers/net/net.h"
#include "net/uip.h"
#include "net/uip_arp.h"

ListNode net_pollers;
static ListNode net_devices;
static NetDevice *net_device;

void net_add_device(NetDevice *dev)
{
	NetDevice *list_dev;

	/* Just in case, see if it has been already added. */
	list_for_each(list_dev, net_devices, list_node)
		if (dev == list_dev) {
			printf("%s: Attemp to include the same device\n",
			       __func__);
			return;
		}

	if (dev) {
		assert(dev->ready);
		assert(dev->recv);
		assert(dev->send);
		assert(dev->get_mac);
	}

	printf("Adding net device\n");
	list_insert_after(&dev->list_node, &net_devices);
}

void net_remove_device(NetDevice *dev)
{
	if (net_device == dev) {
		printf("Removing current net device\n");
		net_device = NULL;
	}

	list_remove(&dev->list_node);
}

NetDevice *net_get_device(void)
{
	return net_device;
}

void net_wait_for_link(void)
{
	printf("Waiting for link\n");

	while (1) {
		NetPoller *net_poller;
		NetDevice *new_device;

		list_for_each(net_poller, net_pollers, list_node)
			net_poller->poll(net_poller);

		list_for_each(new_device, net_devices, list_node) {
			int ready;

			/* the first link up wins */
			new_device->ready(new_device, &ready);
			if (ready) {
				net_device = new_device;
				/*
				 * Just in case, to accommodate some dongles
				 * which need more time than they think
				 */
				mdelay(200);
				printf("done.\n");
				return;
			}
		}
	}
}

void net_poll(void)
{
	if (!net_device) {
		printf("No network device.\n");
		return;
	}

	struct uip_eth_hdr *hdr = (struct uip_eth_hdr *)uip_buf;
	if (net_device->recv(net_device, uip_buf, &uip_len,
			     CONFIG_UIP_BUFSIZE)) {
		printf("Receive failed.\n");
		return;
	}
	if (uip_len) {
		if (hdr->type == htonw(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			if (uip_len > 0) {
				uip_arp_out();
				net_device->send(net_device, uip_buf, uip_len);
			}
		} else if (hdr->type == htonw(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if (uip_len > 0)
				net_device->send(net_device, uip_buf, uip_len);
		}
	}
}

int net_send(void *buf, uint16_t len)
{
	if (!net_device) {
		printf("No network device.\n");
		return 1;
	}
	return net_device->send(net_device, buf, len);
}

const uip_eth_addr *net_get_mac(void)
{
	if (!net_device) {
		printf("No network device.\n");
		return NULL;
	}
	return net_device->get_mac(net_device);
}
