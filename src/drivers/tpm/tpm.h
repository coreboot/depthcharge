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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __DRIVERS_TPM_TPM_H__
#define __DRIVERS_TPM_TPM_H__

#include <stddef.h>
#include <stdint.h>

enum {
	TpmCmdCountOffset = 2,
	TpmCmdOrdinalOffset = 6
};

typedef struct TpmOps
{
	int (*xmit)(struct TpmOps *me, const uint8_t *sendbuf, size_t send_size,
		    uint8_t *recvbuf, size_t *recv_len);
} TpmOps;

void tpm_set_ops(TpmOps *ops);

/*
 * tpm_xmit()
 *
 * Send the requested data to the TPM and then try to get its response
 *
 * @sendbuf - buffer of the data to send
 * @send_size size of the data to send
 * @recvbuf - memory to save the response to
 * @recv_len - pointer to the size of the response buffer, set to the number
 *             of received bytes on completion
 *
 * Returns 0 on success or -1 on failure.
 */
int tpm_xmit(const uint8_t *sendbuf, size_t send_size,
	     uint8_t *recvbuf, size_t *recv_len);

#endif /* __DRIVERS_TPM_TPM_H__ */
