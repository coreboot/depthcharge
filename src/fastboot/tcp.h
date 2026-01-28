/*
 * Copyright 2021 Google LLC
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

#include <commonlib/list.h>
#include <stdbool.h>

#include "fastboot/fastboot.h"
#include "net/uip.h"

#define FASTBOOT_PORT 5554

enum fastboot_tcp_state {
	/* Waiting for the "FB01" handshake */
	WAIT_FOR_HANDSHAKE = 0,
	/* Waiting for the start of a new fastboot packet */
	WAIT_FOR_HEADER,
	/* Waiting for the fastboot command data after receiving header */
	WAIT_FOR_COMMAND,
	/* Waiting for the fastboot command data if it was split over multiple TCP packets */
	WAIT_FOR_COMMAND_SEGMENT,
	/* Waiting for the fastboot download data after receiving header */
	WAIT_FOR_DOWNLOAD,
};

/* State data structure used in WAIT_FOR_HEADER state */
struct fastboot_tcp_header_state_data {
	/* Amount of bytes already collected */
	uint8_t header_bytes_collected;
	/* Buffer for collecting a header */
	char header[sizeof(uint64_t)];
};

/*
 * State data structure used in WAIT_FOR_COMMAND, WAIT_FOR_COMMAND_SEGMENT and
 * WAIT_FOR_DOWNLOAD states
 */
struct fastboot_tcp_data_state_data {
	/* Amount of data left to receive */
	uint64_t data_left_to_receive;
	/* Amount of bytes already collected in segment_buf in WAIT_FOR_COMMAND_SEGMENT state */
	size_t segment_offset;
	/* The buffer for storing fastboot packet segments in WAIT_FOR_COMMAND_SEGMENT state */
	char segment_buf[FASTBOOT_MSG_MAX];
};

/* Union of all state specific data */
union fastboot_tcp_state_data {
	struct fastboot_tcp_header_state_data wait_for_header;
	struct fastboot_tcp_data_state_data wait_for_data;
};

struct fastboot_tcp_session {
	// Transport-agnostic fastboot session information.
	struct FastbootOps fb_session;
	// Current state of this session.
	enum fastboot_tcp_state state;
	// Data specific for the current state
	union fastboot_tcp_state_data state_data;
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
	struct list_node txq_head;
	struct list_node *txq_last;
};

// These are entries in the packet queue.
struct fastboot_tcp_packet {
	const void *data;
	int len;
	struct list_node node;
};

struct FastbootOps *fastboot_setup_tcp(void);

#endif // __FASTBOOT_TCP_H__
