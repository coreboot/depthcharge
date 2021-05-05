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

#include "fastboot/tcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "die.h"
#include "drivers/net/net.h"
#include "drivers/power/power.h"
#include "endian.h"
#include "fastboot/fastboot.h"
#include "net/net.h"
#include "net/uip.h"
#include "netboot/netboot.h"

static struct fastboot_tcp_session tcp_session;

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
	ListNode *n = tcp->txq_top->next;
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

// Reset a session.
static void fastboot_tcp_reset_session(struct fastboot_tcp_session *tcp)
{
	if (tcp->last_packet) {
		fastboot_tcp_packet_destroy(tcp->last_packet);
		tcp->last_packet = NULL;
	}
	// Drain packet queue.
	while (tcp->txq_top != NULL) {
		struct fastboot_tcp_packet *top = fastboot_tcp_txq_pop(tcp);
		fastboot_tcp_packet_destroy(top);
	}

	// Reset everything to its initial state.
	memset(tcp, 0, sizeof(*tcp));
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
void fastboot_send(void *data, uint64_t datalen)
{
	uint64_t size = htobe64(datalen);
	void *packet_data = xmalloc(sizeof(size) + datalen);
	memcpy(packet_data, &size, sizeof(size));
	memcpy(packet_data + sizeof(size), data, datalen);

	struct fastboot_tcp_packet *p = xmalloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->data = packet_data;
	p->len = datalen + sizeof(size);
	fastboot_tcp_txq_append(&tcp_session, p);
}

/******************** PROTOCOL HANDLING *******************/

// Handles the initial FB01 packet.
static const char *handshake = "FB01";
static void fastboot_tcp_handshake(struct fastboot_tcp_session *tcp)
{
	if (uip_datalen() < 4) {
		FB_DEBUG("Invalid handshake!");
		uip_close();
		return;
	}

	if (strncmp(uip_appdata, handshake, 4) != 0) {
		FB_DEBUG("Invalid handshake received, closing connection.\n");
		uip_close();
		return;
	}

	// Looks good, reply.
	struct fastboot_tcp_packet *p = xmalloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->data = handshake;
	p->len = 4;
	fastboot_tcp_txq_append(tcp, p);

	// Take note of the remote end's information.
	tcp->ripaddr = uip_conn->ripaddr;
	tcp->rport = uip_conn->rport;
	tcp->state = WAIT_FOR_PACKET;
	FB_DEBUG("Established fastboot connection with %d.%d.%d.%d\n",
		 uip_ipaddr_to_quad(&tcp->ripaddr));
}

// Handle a packet received while in the WAIT_FOR_PACKET state.
static void fastboot_tcp_recv(struct fastboot_tcp_session *tcp, char *buf,
			      uint64_t datalen)
{
	uint64_t expected_length;
	if (uip_datalen() < sizeof(expected_length)) {
		FB_DEBUG("Fastboot packet was too short!\n");
		uip_close();
		return;
	}
	memcpy(&expected_length, buf, sizeof(expected_length));
	expected_length = be64toh(expected_length);

	uint64_t available = datalen - sizeof(expected_length);
	if (available < expected_length) {
		FB_DEBUG("expected %llu but only got %llu full packet len "
			 "%llu\n",
			 expected_length, available, datalen);
		tcp->data_left_to_receive = expected_length - available;
		tcp->state = PACKET_INCOMPLETE;
		if (tcp->fb_session.state != DOWNLOAD) {
			// TODO(simonshields): this means that the packet we
			// received was smaller than 68 bytes. Do we need to
			// handle this?
			uip_close();
			printf("drop connection because expected=%llu but "
			       "available=%llu and not in download state.\n",
			       expected_length, available);
			return;
		}
	} else if (available > expected_length) {
		// This doesn't seem to happen, but if it does, we need to fail,
		// because we don't handle the extra data and so might be
		// missing something important.
		FB_DEBUG("received extra data! expected=%llu available=%llu\n",
			 expected_length, available);
		uip_close();
		return;
	}

	if (tcp->fb_session.state == DOWNLOAD) {
		FB_TRACE_IO("[from host] %llu/%llu bytes\n", available,
			    tcp->data_left_to_receive);
	} else {
		FB_TRACE_IO("[from host] %.*s\n", (int)available, &buf[8]);
	}
	buf += sizeof(uint64_t);
	fastboot_handle_packet(&tcp->fb_session, buf, available);
}

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
		fastboot_tcp_reset_session(tcp);
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

	switch (tcp->state) {
	case WAIT_FOR_HANDSHAKE:
		fastboot_tcp_handshake(tcp);
		break;
	case WAIT_FOR_PACKET:
		fastboot_tcp_recv(tcp, uip_appdata, uip_datalen());
		break;
	case PACKET_INCOMPLETE:
	default:
		// Should never happen.
		die("invalid tcp session state\n");
	}

	// Try again to send a packet, in case there wasn't one before
	// but we just enqueued one.
	fastboot_tcp_send_packet(tcp);
}

void fastboot_over_tcp(void)
{
	net_wait_for_link();

	// Set up the network stack.
	uip_init();

	uip_ipaddr_t my_ip, next_ip, server_ip;
	const char *dhcp_bootfile;
	while (try_dhcp(&my_ip, &next_ip, &server_ip, &dhcp_bootfile))
		printf("Dhcp failed, retrying.\n");

	memset(&tcp_session, 0, sizeof(tcp_session));
	printf("fastboot starting.\n");
	uip_listen(uip_htons(FASTBOOT_PORT));

	net_set_callback(fastboot_tcp_net_callback);
	while (!fastboot_is_finished(&tcp_session.fb_session))
		net_poll();
	net_set_callback(NULL);
	printf("fastboot done.\n");
	if (tcp_session.fb_session.state == REBOOT)
		cold_reboot();
}
