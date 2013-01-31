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

#ifndef __NETBOOT_BOOTP_H__
#define __NETBOOT_BOOTP_H__

#include <stdint.h>

#include "net/uip.h"

typedef enum BootpOpcode
{
	BootpRequest = 1,
	BootpReply = 2
} BootpOpcode;

typedef enum BootpHardwareType
{
	BootpEthernet = 1
} BootpHardwareType;

static const uint16_t BootpClientPort = 68;
static const uint16_t BootpServerPort = 67;

static const int BootpTimeoutSeconds = 5;

typedef struct __attribute__((packed)) BootpPacket
{
	uint8_t opcode;
	uint8_t hw_type;
	uint8_t hw_length;
	uint8_t hops;
	uint32_t transaction_id;
	uint16_t seconds;
	uint16_t unused;
	uint32_t client_ip;
	uint32_t your_ip;
	uint32_t server_ip;
	uint32_t gateway_ip;
	uint8_t client_hw_addr[16];
	uint8_t server_name[64];
	uint8_t bootfile_name[128];
	uint8_t options[64];
} BootpPacket;

int bootp(uip_ipaddr_t *server_ip, const char **bootfile);

#endif /* __NETBOOT_BOOTP_H__ */
