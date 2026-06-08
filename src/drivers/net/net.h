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

#ifndef __DRIVERS_NET_NET_H__
#define __DRIVERS_NET_NET_H__

#include <commonlib/list.h>
#include <stdbool.h>
#include <stdint.h>

#include "net/uip.h"

typedef enum NetDeviceInitStatus {
	NetDeviceNotInitted = 0,
	NetDeviceInitted,
	NetDeviceFailed,
} NetDeviceInitStatus;

typedef struct NetDevice NetDevice;

typedef struct NetDeviceOps {
	int (*ready)(NetDevice *dev, int *ready);
	int (*recv)(NetDevice *dev, void *buf, uint16_t *len,
		int maxlen);
	int (*send)(NetDevice *dev, void *buf, uint16_t len);
	int (*mdio_read)(NetDevice *dev, uint8_t loc, uint16_t *val);
	int (*mdio_write)(NetDevice *dev, uint8_t loc, uint16_t val);
	const uip_eth_addr *(*get_mac)(NetDevice *dev);
	int (*init)(NetDevice *dev);
} NetDeviceOps;

struct NetDevice {
	struct list_node list_node;
	const NetDeviceOps *ops;
	void *dev_data;
	NetDeviceInitStatus init_status;
	uint16_t recv_window;
};

typedef struct NetPoller {
	struct list_node list_node;
	void (*poll)(struct NetPoller *poller);
} NetPoller;

void net_add_device(NetDevice *dev);
void net_remove_device(NetDevice *dev);
NetDevice *net_get_device(void);

/* Standard return codes of net_poll function */
enum net_poll_status {
	NET_POLL_NO_DEV,
	NET_POLL_RX_ERR,
	NET_POLL_NO_RX,
	NET_POLL_RX,
};

/*
 * Returns:
 * - NET_POLL_RX when any packet was received
 * - NET_POLL_NO_RX when no packet was received
 * - NET_POLL_NO_DEV when there is no net_device
 * - NET_POLL_RX_ERR if net_device recv callback failed
 */
enum net_poll_status net_poll(void);
int net_send(void *buf, uint16_t len);
int net_wait_for_link(bool loop);
const uip_eth_addr *net_get_mac(void);

extern struct list_node net_pollers;

#endif /* __DRIVERS_NET_NET_H__ */
