/*
 * Copyright 2013 Google LLC
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
 */

#ifndef __DRIVERS_TPM_TPM_H__
#define __DRIVERS_TPM_TPM_H__

#include <stddef.h>
#include <stdint.h>

enum {
	TpmAccessValid = (1 << 7),
	TpmAccessActiveLocality = (1 << 5),
	TpmAccessRequestPending = (1 << 2),
	TpmAccessRequestUse = (1 << 1),
	TpmAccessEstablishment = (1 << 0),
};

enum {
	TpmStsFamilyShift = 26,
	TpmStsFamilyMask = (0x3 << TpmStsFamilyShift),
	TpmStsFamilyTpm2 = (1 << TpmStsFamilyShift),
	TpmStsResetEstablismentBit = (1 << 25),
	TpmStsCommandCancel = (1 << 24),
	TpmStsBurstCountShift = 8,
	TpmStsBurstCountMask = (0xFFFF << TpmStsBurstCountShift),
	TpmStsValid = (1 << 7),
	TpmStsCommandReady = (1 << 6),
	TpmStsGo = (1 << 5),
	TpmStsDataAvail = (1 << 4),
	TpmStsDataExpect = (1 << 3),
	TpmStsSelfTestDone = (1 << 2),
	TpmStsResponseRetry = (1 << 1),
};

enum {
	TpmCmdCountOffset = 2,
	TpmCmdOrdinalOffset = 6,
	TpmMaxBufSize = 1260
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

/*
 * tpm_report_state()
 *
 * On platforms where this feature is supported this function returns a buffer
 * containing a string representing internal TPM state. If the feature is not
 * supported, the function returns NULL.
 *
 * The caller of this function is expected to free the string buffer.
 */
char *tpm_report_state(void);

/*
 * tpm_set_tpm_mode()
 *
 * On platforms where this feature is supported, sets the TPM mode to mode_val.
 * Otherwise, return -1.
 *
 */
int tpm_set_tpm_mode(uint8_t mode_val);

#endif /* __DRIVERS_TPM_TPM_H__ */
