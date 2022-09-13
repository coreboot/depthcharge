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

#include <libpayload.h>
#include <tss_constants.h>

#include "drivers/tpm/google/tpm.h"
#include "drivers/tpm/tpm.h"

static TpmOps *tpm_ops;

void tpm_set_ops(TpmOps *ops)
{
	die_if(tpm_ops, "%s: TPM ops already set.\n", __func__);
	tpm_ops = ops;
}

int tpm_xmit(const uint8_t *sendbuf, size_t send_size,
	     uint8_t *recvbuf, size_t *recv_len)
{
	die_if(!tpm_ops, "%s: No TPM ops set.\n", __func__);
	return tpm_ops->xmit(tpm_ops, sendbuf, send_size, recvbuf, recv_len);
}

char *tpm_report_state(void)
{
	die_if(!tpm_ops, "%s: No TPM ops set.\n", __func__);

	if (!CONFIG(DRIVER_TPM_GOOGLE)) {
		printf("%s: not supported\n", __func__);
		return NULL;
	}

	return tpm_google_get_tpm_state(tpm_ops);
}

int tpm_set_tpm_mode(uint8_t mode_val)
{
	die_if(!tpm_ops, "%s: No TPM ops set.\n", __func__);

	if (!CONFIG(DRIVER_TPM_GOOGLE)) {
		printf("%s: not supported\n", __func__);
		return -1;
	}

	return tpm_google_set_tpm_mode(tpm_ops, mode_val);
}
