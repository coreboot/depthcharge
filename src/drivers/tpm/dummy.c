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

#include <libpayload.h>

#include "drivers/tpm/tpm.h"

int tis_init(void)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

int tis_open(void)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

int tis_close(void)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

int tis_sendrecv(const uint8_t *sendbuf, size_t send_size, uint8_t *recvbuf,
			size_t *recv_len)
{
	printf("%s not implemented.\n", __func__);
	halt();
}
