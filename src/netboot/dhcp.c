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
#include "net/uip_udp_packet.h"
#include "netboot/dhcp.h"

typedef enum DhcpOpcode
{
	DhcpRequestOp = 1,
	DhcpReplyOp = 2
} DhcpOpcode;

typedef enum DhcpHardwareType
{
	DhcpEthernet = 1
} DhcpHardwareType;

static const uint16_t DhcpServerPort = 67;
static const uint16_t DhcpClientPort = 68;

// Wait for a response for 3 seconds before resending a request.
static const uint64_t DhcpRespTimeoutUs = 3 * 1000 * 1000;

typedef struct __attribute__((packed)) DhcpPacket
{
	uint8_t opcode;
	uint8_t hw_type;
	uint8_t hw_length;
	uint8_t hops;
	uint32_t transaction_id;
	uint16_t seconds;
	uint16_t flags;
	uint32_t client_ip;
	uint32_t your_ip;
	uint32_t server_ip;
	uint32_t gateway_ip;
	uint8_t client_hw_addr[16];
	uint8_t server_name[64];
	uint8_t bootfile_name[128];
	uint32_t cookie;
	uint8_t options[308];
} DhcpPacket;

static const unsigned int DhcpMinPacketSize = 576;
static const unsigned int DhcpMaxPacketSize =
	sizeof(DhcpPacket) + UIP_IPH_LEN + UIP_UDPH_LEN;

static const uint8_t DhcpCookie[] = { 99, 130, 83, 99 };

typedef enum DhcpTags {
	// "Vendor extensions".
	DhcpTagPadding = 0,
	DhcpTagSubnetMask = 1,
	DhcpTagTimeOffset = 2,
	DhcpTagDefaultRouter = 3,
	DhcpTagTimeServer = 4,
	DhcpTagDnsServer = 6,
	DhcpTagLogServer = 7,
	DhcpTagCookieServer = 8,
	DhcpTagPrintServer = 9,
	DhcpTagImpressServer = 10,
	DhcpTagResourceLocationServer = 11,
	DhcpTagHostName = 12,
	DhcpTagBootFileSize = 13,
	DhcpTagMeritDumpFile = 14,
	DhcpTagDomainName = 15,
	DhcpTagSwapServer = 16,
	DhcpTagRootPath = 17,
	DhcpTagExtensionsPath = 18,

	// IP layer paramters per host.
	DhcpTagIpForwarding = 19,
	DhcpTagNonLocalSourceRouting = 20,
	DhcpTagPolicyFilter = 21,
	DhcpTagMaximumDatagramSize = 22,
	DhcpTagDefaultTimeToLive = 23,
	DhcpTagPathMtuAgingTimeout = 24,
	DhcpTagPathMtuPlateauTable = 25,

	// IP later parameters per interface.
	DhcpTagInterfaceMtu = 26,
	DhcpTagAllSubnetsAreLocal = 27,
	DhcpTagBroadcastAddress = 28,
	DhcpTagPerformMaskDiscovery = 29,
	DhcpTagMaskSupplier = 30,
	DhcpTagPerformRouterDiscovery = 31,
	DhcpTagRouterSolicitationAddress = 32,
	DhcpTagStaticRouteOption = 33,

	// Link layer paramters per interface.
	DhcpTagTrailerEncapsulation = 34,
	DhcpTagArpCacheTimeout = 35,
	DhcpTagEthernetEncapsulation = 36,

	// Tcp parameters.
	DhcpTagTcpDefaultTtl = 37,
	DhcpTagTcpKeepaliveInterval = 38,
	DhcpTagTcpKeepaliveGarbage = 39,

	// Application and service parameters.
	DhcpTagNetworkInformationServiceDomain = 40,
	DhcpTagNetworkInformationServers = 41,
	DhcpTagNetworkTimeProtocolServers = 42,
	DhcpTagVendorSpecificInformation = 43,
	DhcpTagNetbiosOverTcpIpNameServer = 44,
	DhcpTagNetbiosOverTcpIpDatagramDistributionServer = 45,
	DhcpTagNetbiosOverTcpIpNodeType = 46,
	DhcpTagNetbiosOverTcpIpScope = 47,
	DhcpTagXWindowSystemFontServer = 48,
	DhcpTagXWindowSystemDisplayManager = 49,

	// Dhcp extensions.
	DhcpTagRequestedIpAddress = 50,
	DhcpTagIpAddressLeaseTime = 51,
	DhcpTagOptionOverload = 52,
	DhcpTagMessageType = 53,
	DhcpTagServerIdentifier = 54,
	DhcpTagParameterRequestList = 55,
	DhcpTagMessage = 56,
	DhcpTagMaximumDhcpMessageSize = 57,
	DhcpTagRenewalTimeValue = 58,
	DhcpTagRebindTimeValue = 59,
	DhcpTagClassIdentifier = 60,
	DhcpTagClientIdentifier = 61,

	DhcpTagEndOfList = 255
} DhcpTags;

typedef enum DhcpState
{
	DhcpInit,
	DhcpRequesting,
	DhcpBound
} DhcpState;

typedef enum DhcpMessageType
{
	DhcpNoMessageType = 0,
	DhcpDiscover = 1,
	DhcpOffer = 2,
	DhcpRequest = 3,
	DhcpDecline = 4,
	DhcpAck = 5,
	DhcpNak = 6,
	DhcpRelease = 7
} DhcpMessageType;

enum {
	OptionOverloadNone = 0,
	OptionOverloadFile = 1,
	OptionOverloadSname = 2,
	OptionOverloadBoth = 3
};

// Used in dhcp_callback().
static DhcpState dhcp_state = DhcpInit;
static DhcpPacket *dhcp_in;
static int dhcp_in_ready;
static DhcpPacket *dhcp_out;

typedef int (*DhcpOptionFunc)(uint8_t tag, uint8_t length, uint8_t *value,
			      void *data);

static int dhcp_process_options(DhcpPacket *packet, int overload,
				DhcpOptionFunc func, void *data)
{
	uint8_t *options;
	int size;
	switch (overload) {
	case OptionOverloadNone:
		options = packet->options;
		size = sizeof(packet->options);
		break;
	case OptionOverloadFile:
		options = packet->bootfile_name;
		size = sizeof(packet->bootfile_name);
		break;
	case OptionOverloadSname:
		options = packet->server_name;
		size = sizeof(packet->server_name);
		break;
	case OptionOverloadBoth:
		options = packet->server_name;
		size = sizeof(packet->server_name) +
			sizeof(packet->bootfile_name);
		break;
	default:
		return 1;
	}

	int pos = 0;

	int new_overload = 0;

	while (pos < size) {
		uint8_t tag = options[pos++];
		if (pos >= size)
			return 1;
		uint8_t length = options[pos++];
		if (pos >= size)
			return 1;
		uint8_t *value = &options[pos];
		pos += length;
		if (pos >= size)
			return 1;

		// Handle "end of list" and "option overload" options.
		if (tag == DhcpTagEndOfList)
			break;
		if (tag == DhcpTagOptionOverload) {
			if (length < 1)
				return 1;
			new_overload = *value;
			continue;
		}

		// Otherwise, use the provided callback.
		assert(func);
		if (func(tag, length, value, data))
			return 1;
	}

	if (!overload && new_overload)
		return dhcp_process_options(packet, new_overload, func, data);

	return 0;
}

static int dhcp_get_type(uint8_t tag, uint8_t length, uint8_t *value,
			 void *data)
{
	if (tag != DhcpTagMessageType)
		return 0;
	if (length < 1)
		return 1;

	DhcpMessageType *type = (DhcpMessageType *)data;
	*type = (DhcpMessageType)*value;

	return 0;
}

static int dhcp_get_server(uint8_t tag, uint8_t length, uint8_t *value,
			   void *data)
{
	if (tag != DhcpTagServerIdentifier)
		return 0;
	if (length < sizeof(uint32_t))
		return 1;

	memcpy(data, value, sizeof(uint32_t));

	return 0;
}

static int dhcp_apply_options(uint8_t tag, uint8_t length, uint8_t *value,
			      void *data)
{
	switch (tag) {
	case DhcpTagSubnetMask:
		if (length < 4) {
			printf("Truncated subnet mask.\n");
			return 1;
		} else {
			uip_ipaddr_t netmask;
			uip_ipaddr(&netmask, value[0], value[1],
					     value[2], value[3]);
			uip_setnetmask(&netmask);
			return 0;
		}
	case DhcpTagDefaultRouter:
		if (length < 4) {
			printf("Truncated routers.\n");
			return 1;
		} else {
			uip_ipaddr_t router;
			uip_ipaddr(&router, value[0], value[1],
					    value[2], value[3]);
			uip_setdraddr(&router);
			return 0;
		}
	}
	return 0;
}

static void dhcp_callback(void)
{
	// Check that it's the right port. If it isn't, some other connection
	// is open and we got their packet, and that's a bug on our end.
	assert(ntohw(uip_udp_conn->lport) == DhcpClientPort);

	// If there isn't any data, ignore the packet.
	if (!uip_newdata())
		return;

	// If the packet is too big, ignore it.
	if (uip_datalen() > sizeof(DhcpPacket))
		return;

	// Copy the packet out to ensure alignment, etc.
	memset(dhcp_in, 0, sizeof(DhcpPacket));
	memcpy(dhcp_in, uip_appdata, uip_datalen());

	// Check for problems in the reply.
	if (dhcp_in->opcode != DhcpReplyOp ||
		dhcp_in->hw_length != dhcp_out->hw_length ||
		memcmp(dhcp_in->client_hw_addr,
		       dhcp_out->client_hw_addr,
		       dhcp_out->hw_length) ||
		(dhcp_in->transaction_id != dhcp_out->transaction_id))
		return;

	if (memcmp(&dhcp_in->cookie, DhcpCookie, sizeof(DhcpCookie)))
		return;

	DhcpMessageType type = DhcpNoMessageType;
	if (dhcp_process_options(dhcp_in, OptionOverloadNone, &dhcp_get_type,
				 &type))
		return;

	switch (dhcp_state) {
	case DhcpInit:
		if (type != DhcpOffer)
			return;
		break;
	case DhcpRequesting:
		if (type != DhcpAck && type != DhcpNak)
			return;
		break;
	case DhcpBound:
		// We shouldn't get any more packets once we're bound.
		break;
	}

	// Everything checks out. We have a valid reply.
	dhcp_in_ready = 1;
}

static void dhcp_send_packet(struct uip_udp_conn *conn, const char *name,
			     DhcpPacket *out, DhcpPacket *in)
{
	// Send the outbound packet.
	printf("Sending %s... ", name);
	uip_udp_packet_send(conn, out, sizeof(*out));
	printf("done.\n");

	// Prepare for the reply.
	printf("Waiting for reply... ");
	dhcp_in = in;
	dhcp_out = out;
	dhcp_in_ready = 0;

	// Poll network driver until we get a reply. Resend periodically.
	net_set_callback(&dhcp_callback);
	for (;;) {
		uint64_t start = timer_us(0);
		do {
			net_poll();
		} while (!dhcp_in_ready &&
			 timer_us(start) < DhcpRespTimeoutUs);
		if (dhcp_in_ready)
			break;
		// No response, try again.
		uip_udp_packet_send(conn, out, sizeof(*out));
	}
	net_set_callback(NULL);
	printf("done.\n");
}

static void dhcp_prep_packet(DhcpPacket *packet, uint32_t transaction_id)
{
	memset(packet, 0, sizeof(*packet));
	packet->opcode = DhcpRequestOp;
	packet->hw_type = DhcpEthernet;
	packet->hw_length = sizeof(uip_ethaddr);
	packet->hops = 0;
	packet->transaction_id = transaction_id;
	packet->seconds = htonw(100);
	assert(sizeof(uip_ethaddr) < sizeof(packet->client_hw_addr));
	memcpy(packet->client_hw_addr, &uip_ethaddr, sizeof(uip_ethaddr));
	memcpy(&packet->cookie, DhcpCookie, sizeof(DhcpCookie));
}

static void dhcp_add_option(uint8_t **options, uint8_t tag, void *value,
			    uint8_t length, int *remaining)
{
	assert(*remaining >= length + 2);
	(*options)[0] = tag;
	(*options)[1] = length;
	if (length) {
		assert(value);
		memcpy(*options + 2, value, length);
	}
	*remaining -= length + 2;
	*options += length + 2;
}

int dhcp_request(uip_ipaddr_t *next_ip, uip_ipaddr_t *server_ip,
		 const char **bootfile)
{
	DhcpPacket out, in;
	uint8_t byte;
	uint8_t *options;
	int remaining;
	uint8_t requested[] = { DhcpTagSubnetMask, DhcpTagDefaultRouter };
	assert(DhcpMaxPacketSize >= DhcpMinPacketSize);
	uint16_t max_size = htonw(DhcpMaxPacketSize);
	uint8_t client_id[1 + sizeof(uip_ethaddr)];
	client_id[0] = DhcpEthernet;
	memcpy(client_id + 1, &uip_ethaddr, sizeof(uip_ethaddr));

	// Set up the UDP connection.
	uip_ipaddr_t addr;
	uip_ipaddr(&addr, 255,255,255,255);
	struct uip_udp_conn *conn = uip_udp_new(&addr, htonw(DhcpServerPort));
	if (!conn) {
		printf("Failed to set up UDP connection.\n");
		return 1;
	}
	uip_udp_bind(conn, htonw(DhcpClientPort));

	// Send a DHCP discover packet.
	dhcp_prep_packet(&out, rand());
	options = out.options;
	remaining = sizeof(out.options);
	byte = DhcpDiscover;
	dhcp_add_option(&options, DhcpTagMessageType, &byte, sizeof(byte),
			&remaining);
	dhcp_add_option(&options, DhcpTagClientIdentifier, client_id,
			sizeof(client_id), &remaining);
	dhcp_add_option(&options, DhcpTagParameterRequestList, requested,
			sizeof(requested), &remaining);
	dhcp_add_option(&options, DhcpTagMaximumDhcpMessageSize,
			&max_size, sizeof(max_size), &remaining);
	dhcp_add_option(&options, DhcpTagEndOfList, NULL, 0, &remaining);
	dhcp_send_packet(conn, "DHCP discover", &out, &in);

	// Extract the DHCP server id.
	uint32_t server_id;
	if (dhcp_process_options(&in, OptionOverloadNone, &dhcp_get_server,
				 &server_id)) {
		printf("Failed to extract server id.\n");
		return 1;
	}

	// We got an offer. Request it.
	dhcp_state = DhcpRequesting;
	dhcp_prep_packet(&out, rand());
	options = out.options;
	remaining = sizeof(out.options);
	byte = DhcpRequest;
	dhcp_add_option(&options, DhcpTagMessageType, &byte, sizeof(byte),
			&remaining);
	dhcp_add_option(&options, DhcpTagClientIdentifier, client_id,
			sizeof(client_id), &remaining);
	dhcp_add_option(&options, DhcpTagRequestedIpAddress, &in.your_ip,
			sizeof(in.your_ip), &remaining);
	dhcp_add_option(&options, DhcpTagParameterRequestList, requested,
			sizeof(requested), &remaining);
	dhcp_add_option(&options, DhcpTagMaximumDhcpMessageSize,
			&max_size, sizeof(max_size), &remaining);
	dhcp_add_option(&options, DhcpTagServerIdentifier,
			&server_id, sizeof(server_id), &remaining);
	dhcp_add_option(&options, DhcpTagEndOfList, NULL, 0, &remaining);
	dhcp_send_packet(conn, "DHCP request", &out, &in);

	DhcpMessageType type;
	if (dhcp_process_options(&in, OptionOverloadNone, &dhcp_get_type,
				 &type)) {
		printf("Failed to extract message type.\n");
		dhcp_state = DhcpInit;
		return 1;
	}
	if (type == DhcpNak) {
		printf("DHCP request nak-ed by the server.\n");
		dhcp_state = DhcpInit;
		return 1;
	}

	// The server acked, completing the transaction.
	dhcp_state = DhcpBound;
	uip_udp_remove(conn);

	// Apply the settings.
	if (dhcp_process_options(&in, OptionOverloadNone,
				 &dhcp_apply_options, NULL)) {
		dhcp_state = DhcpInit;
		return 1;
	}

	int bootfile_size = sizeof(in.bootfile_name) + 1;
	char *file = xmalloc(bootfile_size);
	file[bootfile_size - 1] = 0;
	memcpy(file, in.bootfile_name, sizeof(in.bootfile_name));
	*bootfile = file;
	uip_ipaddr(next_ip, in.server_ip >> 0, in.server_ip >> 8,
			    in.server_ip >> 16, in.server_ip >> 24);

	uip_ipaddr(server_ip, server_id >> 0, server_id >> 8,
			      server_id >> 16, server_id >> 24);

	uip_ipaddr_t my_ip;
	uip_ipaddr(&my_ip, in.your_ip >> 0, in.your_ip >> 8,
			   in.your_ip >> 16, in.your_ip >> 24);
	uip_sethostaddr(&my_ip);

	return 0;
}

int dhcp_release(uip_ipaddr_t server_ip)
{
	DhcpPacket release;

	// Set up the UDP connection.
	struct uip_udp_conn *conn =
		uip_udp_new(&server_ip, htonw(DhcpServerPort));
	if (!conn) {
		printf("Failed to set up UDP connection.\n");
		return 1;
	}
	uip_udp_bind(conn, htonw(DhcpClientPort));

	// Prepare the DHCP release packet.
	dhcp_prep_packet(&release, rand());
	uip_ipaddr_t my_ip;
	uip_gethostaddr(&my_ip);
	release.client_ip = (uip_ipaddr1(&my_ip) << 0) |
			    (uip_ipaddr2(&my_ip) << 8) |
			    (uip_ipaddr3(&my_ip) << 16) |
			    (uip_ipaddr4(&my_ip) << 24);
	uint8_t *options = release.options;
	int remaining = sizeof(release.options);
	uint8_t byte = DhcpRelease;
	dhcp_add_option(&options, DhcpTagMessageType, &byte, sizeof(byte),
			&remaining);
	dhcp_add_option(&options, DhcpTagEndOfList, NULL, 0, &remaining);

	// Call uip_udp_packet_send directly since we won't get a reply.
	uip_udp_packet_send(conn, &release, sizeof(release));

	dhcp_state = DhcpInit;
	uip_udp_remove(conn);

	return 0;
}
