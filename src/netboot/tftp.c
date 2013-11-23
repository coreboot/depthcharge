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
#include <stdint.h>

#include "drivers/net/net.h"
#include "net/net.h"
#include "net/uip.h"
#include "net/uip_udp_packet.h"
#include "netboot/tftp.h"

typedef enum TftpStatus
{
	TftpPending = 0,
	TftpSuccess = 1,
	TftpFailure = 2
} TftpStatus;

static TftpStatus tftp_status;

static uint8_t *tftp_dest;
static int tftp_got_response;
static int tftp_blocknum;
static uint32_t tftp_total_size;
static uint32_t tftp_max_size;

typedef struct TftpAckPacket
{
	uint16_t opcode;
	uint16_t block;
} TftpAckPacket;

static void tftp_print_error_pkt(void)
{
	if (uip_datalen() >= 4) {
		uint16_t error;
		memcpy(&error, (uint8_t *)uip_appdata + 2,
			sizeof(error));
		error = ntohw(error);
		printf("Error code %d", error);
		switch (error) {
		case TftpUndefined:
			printf(" (Undefined)\n");
			break;
		case TftpFileNotFound:
			printf(" (File not found)\n");
			break;
		case TftpAccessViolation:
			printf(" (Access violation)\n");
			break;
		case TftpNoSpace:
			printf(" (Not enough space)\n");
			break;
		case TftpIllegalOp:
			printf(" (Illegal operation)\n");
			break;
		case TftpUnknownId:
			printf(" (Unknown transfer ID)\n");
			break;
		case TftpFileExists:
			printf(" (File already exists)\n");
			break;
		case TftpNoSuchUser:
			printf(" (No such user)\n");
			break;
		default:
			printf("\n");
		}
	}
	if (uip_datalen() > 4) {
		// Copy out the error message so we can null terminate it.
		int message_len = uip_datalen() - 4 + 1;
		char *message = xmalloc(message_len);

		message[message_len - 1] = 0;
		memcpy(message, uip_appdata, message_len - 1);
		printf("Error message: %s\n", message);
		free(message);
	}
}

static void tftp_callback(void)
{
	// If there isn't at least an opcode, ignore the packet.
	if (!uip_newdata())
		return;
	if (uip_datalen() < 2)
		return;

	if (!uip_udp_conn->rport) {
		int srcport_offset = offsetof(struct uip_udpip_hdr, srcport);
		memcpy(&uip_udp_conn->rport,
			&uip_buf[CONFIG_UIP_LLH_LEN + srcport_offset],
			sizeof(uip_udp_conn->rport));
	}

	// Extract the opcode.
	uint16_t opcode;
	memcpy(&opcode, uip_appdata, sizeof(opcode));
	opcode = ntohw(opcode);

	// If there was an error, report it and stop the transfer.
	if (opcode == TftpError) {
		tftp_status = TftpFailure;
		printf(" error!\n");
		tftp_print_error_pkt();
		return;
	}

	// We should only get data packets. Those are at least 4 bytes long.
	if (opcode != TftpData || uip_datalen() < 4)
		return;

	// Get the block number.
	uint16_t blocknum;
	memcpy(&blocknum, (uint8_t *)uip_appdata + 2, sizeof(blocknum));
	blocknum = ntohw(blocknum);

	// Ignore blocks which are duplicated or out of order.
	if (blocknum != tftp_blocknum)
		return;

	void *new_data = (uint8_t *)uip_appdata + 4;
	int new_data_len = uip_datalen() - 4;

	// If the block is too big, reject it.
	if (new_data_len > TftpMaxBlockSize)
		return;

	// If we're out of space give up.
	if (new_data_len > tftp_max_size - tftp_total_size) {
		tftp_status = TftpFailure;
		printf("TFTP transfer too large.\n");
		return;
	}

	// If there's any data, copy it in.
	if (new_data_len) {
		memcpy(tftp_dest, new_data, new_data_len);
		tftp_dest += new_data_len;
	}
	tftp_total_size += new_data_len;

	// If this block was less than the maximum size, the transfer is done.
	if (new_data_len < TftpMaxBlockSize) {
		tftp_status = TftpSuccess;
		return;
	}

	// Prepare an ack.
	TftpAckPacket ack = {
		htonw(TftpAck),
		htonw(tftp_blocknum)
	};
	memcpy(uip_appdata, &ack, sizeof(ack));
	uip_udp_send(sizeof(ack));

	// Move on to the next block.
	tftp_blocknum++;

	if (!(tftp_blocknum % 10)) {
		// Give some feedback that something is happening.
		printf("#");
	}
	tftp_got_response = 1;
}

int tftp_read(void *dest, uip_ipaddr_t *server_ip, const char *bootfile,
	uint32_t *size, uint32_t max_size)
{
	// Build the read request packet.
	uint16_t opcode = htonw(TftpReadReq);
	int opcode_len = sizeof(opcode);

	int name_len = strlen(bootfile) + 1;

	const char mode[] = "Octet";
	int mode_len = sizeof(mode);

	int read_req_len = opcode_len + name_len + mode_len;
	uint8_t *read_req = xmalloc(read_req_len);

	memcpy(read_req, &opcode, opcode_len);
	memcpy(read_req + opcode_len, bootfile, name_len);
	memcpy(read_req + opcode_len + name_len, mode, mode_len);

	// Set up the UDP connection.
	struct uip_udp_conn *conn = uip_udp_new(server_ip, htonw(TftpPort));
	if (!conn) {
		printf("Failed to set up UDP connection.\n");
		free(read_req);
		return -1;
	}

	// Send the request.
	printf("Sending tftp read request... ");
	uip_udp_packet_send(conn, read_req, read_req_len);
	conn->rport = 0;
	printf("done.\n");

	// Prepare for the transfer.
	printf("Waiting for the transfer... ");
	tftp_status = TftpPending;
	tftp_dest = dest;
	tftp_blocknum = 1;
	tftp_total_size = 0;
	tftp_max_size = max_size;

	// Poll the network driver until the transaction is done.

	net_set_callback(&tftp_callback);
	while (tftp_status == TftpPending) {
		tftp_got_response = 0;
		net_poll();
		if (tftp_got_response)
			continue;

		// No response. Resend our last packet and try again.
		if (tftp_blocknum == 1) {
			// Resend the read request.
			conn->rport = htonw(TftpPort);
			uip_udp_packet_send(conn, read_req, read_req_len);
			conn->rport = 0;
		} else {
			// Resend the last ack.
			TftpAckPacket ack = {
				htonw(TftpAck),
				htonw(tftp_blocknum - 1)
			};
			uip_udp_packet_send(conn, &ack, sizeof(ack));
		}
	}
	uip_udp_remove(conn);
	free(read_req);
	net_set_callback(NULL);

	// See what happened.
	if (tftp_status == TftpFailure) {
		// The error was printed when it was received.
		return -1;
	} else {
		if (size)
			*size = tftp_total_size;
		printf(" done.\n");
		return 0;
	}
}
