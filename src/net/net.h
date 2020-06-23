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

#ifndef __NET_NET_H__
#define __NET_NET_H__

/* Common maximum supported jumbo frame size according to Wikipedia. */
#define ETHERNET_MAX_FRAME_SIZE 9216

typedef void (*NetCallback)(void);

void net_set_callback(NetCallback func);
NetCallback net_get_callback(void);

void net_call_callback(void);

#endif /* __NET_NET_H__ */
