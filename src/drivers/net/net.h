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

#ifndef __DRIVERS_NET_NET_H__
#define __DRIVERS_NET_NET_H__

#include <stdint.h>

#include "net/uip.h"

typedef struct NetDevice {
	int (*ready)(struct NetDevice *dev, int *ready);
	int (*recv)(struct NetDevice *dev, void *buf, uint16_t *len,
		int maxlen);
	int (*send)(struct NetDevice *dev, void *buf, uint16_t len);
	const uip_eth_addr *(*get_mac)(struct NetDevice *dev);
	void *dev_data;
} NetDevice;

void net_set_device(NetDevice *dev);
NetDevice *net_get_device(void);
void net_poll(void);


int net_ready(int *ready);
int net_send(void *buf, uint16_t len);
const uip_eth_addr *net_get_mac(void);

#endif /* __DRIVERS_NET_NET_H__ */
