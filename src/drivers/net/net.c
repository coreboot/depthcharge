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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <endian.h>
#include <libpayload.h>

#include "drivers/net/net.h"
#include "net/uip.h"
#include "net/uip_arp.h"

static NetDevice *net_device;

void net_set_device(NetDevice *dev)
{
	if (dev) {
		assert(dev->ready);
		assert(dev->recv);
		assert(dev->send);
		assert(dev->get_mac);
	}

	net_device = dev;
}

NetDevice *net_get_device(void)
{
	return net_device;
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

int net_ready(int *ready)
{
	if (!net_device) {
		printf("No network device.\n");
		return 1;
	}
	return net_device->ready(net_device, ready);
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
