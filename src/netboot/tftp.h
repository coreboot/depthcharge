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

#ifndef __NETBOOT_TFTP_H__
#define __NETBOOT_TFTP_H__

#include "net/uip.h"

typedef enum TftpOpcode
{
	TftpReadReq = 1,
	TftpWriteReq = 2,
	TftpData = 3,
	TftpAck = 4,
	TftpError = 5
} TftpOpcode;

typedef enum TftpErrorCode
{
	TftpUndefined = 0,
	TftpFileNotFound = 1,
	TftpAccessViolation = 2,
	TftpNoSpace = 3,
	TftpIllegalOp = 4,
	TftpUnknownId = 5,
	TftpFileExists = 6,
	TftpNoSuchUser = 7
} TftpErrorCode;

static const uint16_t TftpPort = 69;
static const int TftpMaxBlockSize = 512;

int tftp_read(void *dest, uip_ipaddr_t *server_ip, const char *bootfile,
	uint32_t *size, uint32_t max_size);

#endif /* __NETBOOT_TFTP_H__ */
