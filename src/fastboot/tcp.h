/*
 * Copyright (C) 2021 Google Inc.
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

#ifndef __FASTBOOT_TCP_H__
#define __FASTBOOT_TCP_H__

#include <stdbool.h>
#include "base/list.h"
#include "fastboot/fastboot.h"
#include "net/uip.h"

#define FASTBOOT_PORT 5554

enum fastboot_tcp_state {
	// Waiting for the "FB01" handshake.
	WAIT_FOR_HANDSHAKE = 0,
	// Waiting for the start of a new fastboot packet.
	WAIT_FOR_PACKET = 1,
	// Start of a fastboot packet has been received, but it is split over
	// multiple
	// TCP packets.
	PACKET_INCOMPLETE = 2,
};

struct fastboot_tcp_session {
	// Transport-agnostic fastboot session information.
	struct fastboot_session fb_session;
	// Current state of this session.
	enum fastboot_tcp_state state;
	// Details about the remote end of the connection.
	// We use this to make sure we ignore packets that aren't from the
	// machine that initiated our connection.
	uip_ipaddr_t ripaddr;
	uint16_t rport;
	// The last packet we sent, in case we have to retransmit it.
	// NULL once the packet has been acked.
	struct fastboot_tcp_packet *last_packet;

	// The queue of packets that are waiting to be sent.
	// This is necessary because uIP only sends a packet once per callback.
	ListNode *txq_top;
	ListNode *txq_bottom;

	// Amount of data left to receive when in the PACKET_INCOMPLETE state.
	uint64_t data_left_to_receive;
};

// These are entries in the packet queue.
struct fastboot_tcp_packet {
	const void *data;
	int len;
	ListNode node;
};

void fastboot_send(void *data, uint64_t len);
void fastboot_over_tcp(void);

#endif // __FASTBOOT_TCP_H__
