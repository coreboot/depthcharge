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

#ifndef __DRIVERS_NET_NET_H__
#define __DRIVERS_NET_NET_H__

#include <stdint.h>

#include "base/list.h"
#include "net/uip.h"

typedef struct NetDevice {
	ListNode list_node;
	int (*ready)(struct NetDevice *dev, int *ready);
	int (*recv)(struct NetDevice *dev, void *buf, uint16_t *len,
		int maxlen);
	int (*send)(struct NetDevice *dev, void *buf, uint16_t len);
	int (*mdio_read)(struct NetDevice *dev, uint8_t loc, uint16_t *val);
	int (*mdio_write)(struct NetDevice *dev, uint8_t loc, uint16_t val);
	const uip_eth_addr *(*get_mac)(struct NetDevice *dev);
	void *dev_data;
} NetDevice;

typedef struct NetPoller {
	ListNode list_node;
	void (*poll)(struct NetPoller *poller);
} NetPoller;

void net_add_device(NetDevice *dev);
void net_remove_device(NetDevice *dev);
NetDevice *net_get_device(void);
void net_poll(void);
int net_send(void *buf, uint16_t len);
void net_wait_for_link(void);
const uip_eth_addr *net_get_mac(void);

extern ListNode net_pollers;

#endif /* __DRIVERS_NET_NET_H__ */
