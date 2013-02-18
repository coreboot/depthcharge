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
#include "net/net.h"
#include "net/uip.h"
#include "net/uip_arp.h"
#include "net/uip-udp-packet.h"
#include "netboot/bootp.h"

static BootpPacket *bootp_reply;
static int bootp_reply_ready;
static BootpPacket *bootp_request;

static void bootp_callback(void)
{
	// Check that it's the right port. If it isn't, some other connection
	// is open and we got their packet, and that's a bug on our end.
	assert(ntohw(uip_udp_conn->lport) == BootpClientPort);

	// If there isn't any or enough data, ignore the packet.
	if (!uip_newdata())
		return;
	if (uip_datalen() < sizeof(BootpPacket))
		return;

	// Copy the packet out to ensure alignment, etc.
	memcpy(bootp_reply, uip_appdata, sizeof(BootpPacket));

	// Check for problems in the reply.
	if (bootp_reply->opcode != BootpReply ||
		bootp_reply->hw_length != bootp_request->hw_length ||
		memcmp(bootp_reply->client_hw_addr,
		       bootp_request->client_hw_addr,
		       bootp_request->hw_length) ||
		(bootp_reply->transaction_id !=
		 bootp_request->transaction_id))
		return;

	// Everything checks out. We have a valid reply.
	bootp_reply_ready = 1;
}

int bootp(uip_ipaddr_t *server_ip, const char **bootfile)
{
	BootpPacket request, reply;

	// Prepare the bootp request packet.
	memset(&request, 0, sizeof(request));
	request.opcode = BootpRequest;
	request.hw_type = BootpEthernet;
	request.hw_length = sizeof(uip_ethaddr);
	request.hops = 0;
	request.transaction_id = rand();
	request.seconds = htonw(100);
	assert(sizeof(uip_ethaddr) < sizeof(request.client_hw_addr));
	memcpy(request.client_hw_addr, &uip_ethaddr, sizeof(uip_ethaddr));

	// Set up the UDP connection.
	uip_ipaddr_t addr;
	uip_ipaddr(&addr, 255,255,255,255);
	struct uip_udp_conn *conn = uip_udp_new(&addr, htonw(BootpServerPort));
	if (!conn) {
		printf("Failed to set up UDP connection.\n");
		return -1;
	}
	uip_udp_bind(conn, htonw(BootpClientPort));

	// Send the request.
	printf("Sending bootp request... ");
	uip_udp_packet_send(conn, &request, sizeof(request));
	printf("done.\n");

	// Prepare for the reply.
	printf("Waiting for reply... ");
	bootp_reply = &reply;
	bootp_request = &request;
	bootp_reply_ready = 0;

	// Poll network driver until we get a reply or time out.
	net_set_callback(&bootp_callback);
	uint32_t timeout = BootpTimeoutSeconds * 1000;
	while (!bootp_reply_ready && timeout--) {
		mdelay(1);
		net_poll();
	}
	uip_udp_remove(conn);
	net_set_callback(NULL);

	// See what happened.
	if (!bootp_reply_ready) {
		printf("timeout!\n");
		return -1;
	} else {
		printf("done.\n");
	}

	// Get the stuff we wanted out of the reply.
	int bootfile_size = sizeof(reply.bootfile_name) + 1;
	char *file = malloc(bootfile_size);
	if (!file) {
		printf("Failed to allocate %d bytes for bootfile name.\n",
			bootfile_size);
		return -1;
	}
	file[bootfile_size - 1] = 0;
	memcpy(file, reply.bootfile_name, sizeof(reply.bootfile_name));
	*bootfile = file;
	uip_ipaddr(server_ip, reply.server_ip >> 0, reply.server_ip >> 8,
			      reply.server_ip >> 16, reply.server_ip >> 24);

	// Apply the new network info.
	uip_ipaddr_t my_ip;
	uip_ipaddr(&my_ip, reply.your_ip >> 0, reply.your_ip >> 8,
			   reply.your_ip >> 16, reply.your_ip >> 24);
	uip_sethostaddr(&my_ip);

	return 0;
}
