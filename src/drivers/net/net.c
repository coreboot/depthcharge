/*
 * Copyright 2013 Google LLC
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

struct list_node net_pollers;
static struct list_node net_devices;
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

static int net_init_device(NetDevice *new_device)
{
	int err;

	if (!new_device->init)
		return 0;

	if (new_device->init_status == NetDeviceNotInitted) {
		err = new_device->init(new_device);
		new_device->init_status = err ?
			NetDeviceFailed : NetDeviceInitted;
	}

	if (new_device->init_status == NetDeviceInitted)
		return 0;

	return -1;
}

int net_wait_for_link(bool loop)
{
	if (loop)
		printf("Waiting for link\n");

	do {
		NetPoller *net_poller;
		NetDevice *new_device;
		int err;

		list_for_each(net_poller, net_pollers, list_node)
			net_poller->poll(net_poller);

		list_for_each(new_device, net_devices, list_node) {
			int ready;

			err = net_init_device(new_device);
			if (err)
				continue;

			/* the first link up wins */
			new_device->ready(new_device, &ready);
			if (ready) {
				net_device = new_device;
				/* Set TCP receive window supported by the device */
				if (net_device->recv_window != 0)
					uip_recv_window = net_device->recv_window;
				else
					uip_recv_window = CONFIG_UIP_RECEIVE_WINDOW;
				/*
				 * Just in case, to accommodate some dongles
				 * which need more time than they think
				 */
				mdelay(200);
				printf("done.\n");
				return 0;
			}
		}
	} while (loop);

	return -1;
}

/*
 * It isn't well documented in uIP, but example-mainloop-with-arp is using 500ms interval for
 * uip_periodic() calls. Documentation of uip_connect() also suggest that 0.5s is typical
 * interval.
 */
#define UIP_PERIODIC_INTERVAL_US (500 * USECS_PER_MSEC)
/* According to uip_arp_timer it should be called once every 10 seconds */
#define UIP_ARP_INTERVAL_US (10 * USECS_PER_SEC)

void net_poll(void)
{
	int ret;
	static uint32_t periodic_timer_us = 0;
	static uint32_t arp_timer_us = 0;

	if (!net_device) {
		printf("No network device.\n");
		return;
	}

	struct uip_eth_hdr *hdr = (struct uip_eth_hdr *)uip_buf;
	ret = net_device->recv(net_device, uip_buf, &uip_len, CONFIG_UIP_BUFSIZE);
	if (ret)
		printf("Receive failed. (%d)\n", ret);
	if (!ret && uip_len) {
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
	} else if (timer_us(periodic_timer_us) > UIP_PERIODIC_INTERVAL_US) {
		periodic_timer_us = timer_us(0);
		for (int i = 0; i < CONFIG_UIP_CONNS; i++) {
			uip_periodic(i);
			if (uip_len > 0) {
				uip_arp_out();
				net_device->send(net_device, uip_buf, uip_len);
			}
		}
		if (timer_us(arp_timer_us) > UIP_ARP_INTERVAL_US) {
			arp_timer_us = timer_us(0);
			uip_arp_timer();
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
