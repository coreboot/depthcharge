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

#include <libpayload.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "die.h"
#include "drivers/net/net.h"
#include "drivers/power/power.h"
#include "endian.h"
#include "fastboot/fastboot.h"
#include "fastboot/tcp.h"
#include "net/dhcp.h"
#include "net/net.h"
#include "net/uip.h"

/********************** PACKET BOOKKEEPING FUNCTIONS **************************/

// Destroy a packet.
static void fastboot_tcp_packet_destroy(struct fastboot_tcp_packet *p)
{
	free((void *)p->data);
	p->data = NULL;
	free(p);
}

// Take a packet off the transmit queue.
static struct fastboot_tcp_packet *
fastboot_tcp_txq_pop(struct fastboot_tcp_session *tcp)
{
	struct fastboot_tcp_packet *top =
		container_of(tcp->txq_top, struct fastboot_tcp_packet, node);
	struct list_node *n = tcp->txq_top->next;
	list_remove(tcp->txq_top);
	if (n == NULL)
		tcp->txq_bottom = NULL;
	tcp->txq_top = n;
	return top;
}

// Append a packet to the transmit queue.
static void fastboot_tcp_txq_append(struct fastboot_tcp_session *tcp,
				    struct fastboot_tcp_packet *p)
{
	if (tcp->txq_top == NULL) {
		tcp->txq_top = tcp->txq_bottom = &p->node;
	} else {
		list_insert_after(&p->node, tcp->txq_bottom);
		tcp->txq_bottom = &p->node;
	}
}

// Send a single packet from the packet queue if we can.
static void fastboot_tcp_send_packet(struct fastboot_tcp_session *tcp)
{
	// If we already sent a packet, there's no packets left to send, or
	// we're not connected to anyone, return.
	if (tcp->last_packet || !tcp->txq_top ||
	    tcp->state == WAIT_FOR_HANDSHAKE)
		return;

	struct fastboot_tcp_packet *p = fastboot_tcp_txq_pop(tcp);
	tcp->last_packet = p;
	if (p->len > 8) {
		// Skip the 8-byte size field.
		FB_TRACE_IO("[to host] %.*s\n", p->len - 8,
			    ((char *)p->data) + 8);
	} else {
		// Handshake, send the whole thing.
		FB_TRACE_IO("[to host] %.*s\n", p->len, (char *)p->data);
	}
	uip_send(p->data, p->len);
}

// Should we keep handling packets? Returns false if we're currently looking
// at data from a different host to the one we started with.
static bool fastboot_tcp_is_valid_conn(struct fastboot_tcp_session *tcp)
{
	// If no connection is established, then anything is valid.
	if (tcp->state == WAIT_FOR_HANDSHAKE)
		return true;

	// Make sure that the remote IP and port match the session's.
	if (!uip_ipaddr_cmp(&uip_conn->ripaddr, &tcp->ripaddr) ||
	    uip_conn->rport != tcp->rport)
		return false;

	return true;
}

// Send some data. This is used by the protocol-layer code in fastboot.c
static int fastboot_tcp_send(struct FastbootOps *fb, void *data, size_t datalen)
{
	struct fastboot_tcp_session *tcp =
		container_of(fb, struct fastboot_tcp_session, fb_session);
	uint64_t size = htobe64(datalen);
	void *packet_data = xmalloc(sizeof(size) + datalen);

	memcpy(packet_data, &size, sizeof(size));
	memcpy(packet_data + sizeof(size), data, datalen);

	struct fastboot_tcp_packet *p = xmalloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->data = packet_data;
	p->len = datalen + sizeof(size);
	fastboot_tcp_txq_append(tcp, p);

	return 0;
}

/******************** PROTOCOL HANDLING *******************/

/**
 * Enter specified state and perform setup if necessary
 */
static void fastboot_tcp_enter_state(struct fastboot_tcp_session *tcp,
				     enum fastboot_tcp_state new_state)
{
	switch (new_state) {
	case WAIT_FOR_HANDSHAKE:
		if (tcp->last_packet) {
			fastboot_tcp_packet_destroy(tcp->last_packet);
			tcp->last_packet = NULL;
		}
		/* Drain packet queue */
		while (tcp->txq_top != NULL) {
			struct fastboot_tcp_packet *top = fastboot_tcp_txq_pop(tcp);
			fastboot_tcp_packet_destroy(top);
		}
		break;
	case WAIT_FOR_HEADER:
		tcp->state_data.wait_for_header.header_bytes_collected = 0;
		break;
	case WAIT_FOR_COMMAND_SEGMENT:
		tcp->state_data.wait_for_data.segment_offset = 0;
		break;
	default:
		/* No extra setup */
		break;
	}

	tcp->state = new_state;
}

/* Handles the initial FB01 packet */
static const char handshake[] = "FB01";
static const size_t handshake_len = sizeof(handshake) - 1;
static uint64_t fastboot_tcp_handshake(struct fastboot_tcp_session *tcp, char *buf,
				       uint64_t datalen)
{
	if (datalen < handshake_len) {
		FB_DEBUG("Invalid handshake!");
		uip_close();
		return 0;
	}

	if (memcmp(buf, handshake, handshake_len) != 0) {
		FB_DEBUG("Invalid handshake received, closing connection.\n");
		uip_close();
		return 0;
	}

	/* Looks good, reply */
	struct fastboot_tcp_packet *p = xmalloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->data = handshake;
	p->len = handshake_len;
	fastboot_tcp_txq_append(tcp, p);

	/* Take note of the remote end's information */
	tcp->ripaddr = uip_conn->ripaddr;
	tcp->rport = uip_conn->rport;
	fastboot_tcp_enter_state(tcp, WAIT_FOR_HEADER);
	FB_DEBUG("Established fastboot connection with %d.%d.%d.%d\n",
		 uip_ipaddr_to_quad(&tcp->ripaddr));

	return handshake_len;
}

/**
 * Extract header (expected length of the message) from the buffer
 *
 * Returns number of bytes consumed in the last call to obtain amount of data
 * to receive. Result of data_left_to_receive is valid when tcp state is changed
 * to WAIT_FOR_COMMAND or WAIT_FOR_DOWNLOAD.
 *
 * Returns number of bytes consumed from buf.
 */
static uint64_t fastboot_tcp_recv_header(struct fastboot_tcp_session *tcp,
					 const char *buf, uint64_t datalen)
{
	struct fastboot_tcp_header_state_data *const state_data =
		&tcp->state_data.wait_for_header;
	const uint64_t bytes_to_collect =
		MIN(datalen, sizeof(state_data->header) - state_data->header_bytes_collected);

	memcpy(state_data->header + state_data->header_bytes_collected, buf, bytes_to_collect);

	state_data->header_bytes_collected += bytes_to_collect;
	if (state_data->header_bytes_collected == sizeof(state_data->header)) {
		/* Whole header received, move to waiting for data */
		tcp->state_data.wait_for_data.data_left_to_receive =
			be64dec(state_data->header);
		/* Choose next state based on fastboot session state */
		fastboot_tcp_enter_state(tcp, tcp->fb_session.state == DOWNLOAD ?
					      WAIT_FOR_DOWNLOAD : WAIT_FOR_COMMAND);
	} else {
		FB_DEBUG("short packet of %llu bytes, so far received %d\n",
			 datalen, state_data->header_bytes_collected);
	}

	return bytes_to_collect;
}

/**
 * Handle packet if all data are in the buf. Otherwise switch to WAIT_FOR_COMMAND_SEGMENT
 * if packet can be handled in segments. If packet cannot be handled, connection is closed.
 *
 * Returns number of bytes consumed from buf.
 */
static uint64_t fastboot_tcp_recv_command(struct fastboot_tcp_session *tcp, char *buf,
					  uint64_t datalen)
{
	struct fastboot_tcp_data_state_data *const state_data = &tcp->state_data.wait_for_data;

	/* We have the whole packet, handle it */
	if (datalen >= state_data->data_left_to_receive) {
		const uint64_t packet_len = state_data->data_left_to_receive;

		FB_TRACE_IO("[from host] %.*s\n", packet_len, buf);
		fastboot_handle_packet(&tcp->fb_session, buf, packet_len);
		fastboot_tcp_enter_state(tcp, WAIT_FOR_HEADER);

		return packet_len;
	}

	/* Check if we can handle this packet using segment method */
	if (state_data->data_left_to_receive <= FASTBOOT_MSG_MAX) {
		fastboot_tcp_enter_state(tcp, WAIT_FOR_COMMAND_SEGMENT);
		return 0;
	}

	/* We don't have full packet in buffer and it is too big to handle in segments */
	uip_close();
	printf("drop connection because cannot collect segmented packet of %llu size\n",
	       state_data->data_left_to_receive);
	fastboot_reset_session(&tcp->fb_session);

	return 0;
}

/**
 * Collect all segments of the fastboot packet. Handle it when it is ready.
 *
 * Returns number of bytes consumed from buf.
 */
static uint64_t fastboot_tcp_recv_segment(struct fastboot_tcp_session *tcp, char *buf,
					  uint64_t datalen)
{
	struct fastboot_tcp_data_state_data *const state_data = &tcp->state_data.wait_for_data;
	const uint64_t bytes_consumed = MIN(state_data->data_left_to_receive, datalen);

	memcpy(state_data->segment_buf + state_data->segment_offset, buf, bytes_consumed);
	state_data->segment_offset += bytes_consumed;
	state_data->data_left_to_receive -= bytes_consumed;

	if (state_data->data_left_to_receive == 0) {
		/* We received the whole packet, handle it */
		FB_TRACE_IO("[from host] %.*s\n", state_data->segment_offset,
			    state_data->segment_buf);
		fastboot_handle_packet(&tcp->fb_session, state_data->segment_buf,
				       state_data->segment_offset);
		fastboot_tcp_enter_state(tcp, WAIT_FOR_HEADER);
	}

	return bytes_consumed;
}

/**
 * Handle data in fastboot download state.
 *
 * Returns number of bytes consumed from buf.
 */
static uint64_t fastboot_tcp_recv_download(struct fastboot_tcp_session *tcp, char *buf,
					   uint64_t datalen)
{
	struct fastboot_tcp_data_state_data *const state_data = &tcp->state_data.wait_for_data;
	const uint64_t bytes_consumed = MIN(state_data->data_left_to_receive, datalen);

	FB_TRACE_IO("[from host] download %llu/%llu bytes\n", bytes_consumed,
		    state_data->data_left_to_receive);

	state_data->data_left_to_receive -= bytes_consumed;
	fastboot_handle_packet(&tcp->fb_session, buf, bytes_consumed);

	/* Whole packet received, wait for new packet. */
	if (state_data->data_left_to_receive == 0)
		fastboot_tcp_enter_state(tcp, WAIT_FOR_HEADER);

	return bytes_consumed;
}

/* Consume all fastboot packets in buf and handle them */
static void fastboot_tcp_recv(struct fastboot_tcp_session *tcp, char *buf,
			      uint64_t datalen)
{
	uint64_t consumed;

	while (datalen && !uip_closed()) {
		switch (tcp->state) {
		case WAIT_FOR_HANDSHAKE:
			consumed = fastboot_tcp_handshake(tcp, buf, datalen);
			break;
		case WAIT_FOR_HEADER:
			consumed = fastboot_tcp_recv_header(tcp, buf, datalen);
			break;
		case WAIT_FOR_COMMAND:
			consumed = fastboot_tcp_recv_command(tcp, buf, datalen);
			break;
		case WAIT_FOR_COMMAND_SEGMENT:
			consumed = fastboot_tcp_recv_segment(tcp, buf, datalen);
			break;
		case WAIT_FOR_DOWNLOAD:
			consumed = fastboot_tcp_recv_download(tcp, buf, datalen);
			break;
		default:
			/* Should never happen */
			die("invalid tcp session state\n");
		}
		datalen -= consumed;
		buf += consumed;
	}
}

/* Forward declaration of a global tcp_session */
static struct fastboot_tcp_session tcp_session;

// Net callback, called when there's network work to do.
static void fastboot_tcp_net_callback(void)
{
	struct fastboot_tcp_session *tcp = &tcp_session;
	if (uip_udpconnection()) {
		// Ignore UDP connections.
		return;
	}

	// Make sure it's not somebody else's connection.
	if (!fastboot_tcp_is_valid_conn(tcp)) {
		FB_DEBUG("Invalid connection\n");
		uip_close();
		return;
	}

	// Check for TCP state.
	if (uip_rexmit()) {
		uip_send(tcp->last_packet->data, tcp->last_packet->len);
		return;
	}
	// Note that uip_closed() can be true even if there's still data left.
	// We don't bother processing it and give up, though.
	if (uip_closed() || uip_aborted() || uip_timedout()) {
		FB_DEBUG(
			"connection closed: closed %d aborted %d timedout %d\n",
			uip_closed(), uip_aborted(), uip_timedout());
		fastboot_reset_session(&tcp->fb_session);
		return;
	}

	if (uip_acked()) {
		// Last packet was sent successfully.
		fastboot_tcp_packet_destroy(tcp->last_packet);
		tcp->last_packet = NULL;
	}

	// Try and send a packet from the queue, if there is one (and the last
	// packet has been ACKed).
	fastboot_tcp_send_packet(tcp);
	if (!uip_newdata()) {
		// Give up if there's not any new data.
		return;
	}

	fastboot_tcp_recv(tcp, uip_appdata, uip_datalen());

	// Try again to send a packet, in case there wasn't one before
	// but we just enqueued one.
	fastboot_tcp_send_packet(tcp);
}

static void fastboot_tcp_reset(struct FastbootOps *fb)
{
	struct fastboot_tcp_session *tcp =
		container_of(fb, struct fastboot_tcp_session, fb_session);

	fastboot_tcp_enter_state(tcp, WAIT_FOR_HANDSHAKE);
}

static void fastboot_tcp_poll(struct FastbootOps *fb)
{
	net_poll();
}

static void fastboot_tcp_release(struct FastbootOps *fb)
{
	net_set_callback(NULL);
	free(fb->serial);
}

static struct fastboot_tcp_session tcp_session = {
	.fb_session = {
		.poll = fastboot_tcp_poll,
		.send_packet = fastboot_tcp_send,
		.reset = fastboot_tcp_reset,
		.release = fastboot_tcp_release,
		.type = FASTBOOT_TCP_CONN,
	},
};

static void fastboot_tcp_setup_serial_string(struct FastbootOps *fb, uip_ipaddr_t *ip)
{
	const char fmt[] = "IP: %d.%d.%d.%d";
	int str_len = snprintf(NULL, 0, fmt, uip_ipaddr_to_quad(ip));

	/* If sprintf failed, at least setup empty serial */
	if (str_len < 0)
		str_len = 0;
	str_len++;

	fb->serial = xmalloc(str_len);
	snprintf(fb->serial, str_len, fmt, uip_ipaddr_to_quad(ip));
}

struct FastbootOps *fastboot_setup_tcp(void)
{
	net_wait_for_link();

	/* Set up the network stack */
	uip_init();

	uip_ipaddr_t my_ip, next_ip, server_ip;
	const char *dhcp_bootfile;
	while (dhcp_request(&next_ip, &server_ip, &dhcp_bootfile))
		printf("Dhcp failed, retrying.\n");
	uip_gethostaddr(&my_ip);

	fastboot_tcp_setup_serial_string(&tcp_session.fb_session, &my_ip);

	uip_listen(uip_htons(FASTBOOT_PORT));

	net_set_callback(fastboot_tcp_net_callback);

	return &tcp_session.fb_session;
}
