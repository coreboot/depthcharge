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
 */

#include <libpayload.h>

#include "config.h"
#include "drivers/tpm/tpm.h"

static TpmOps *tpm_ops;

void tpm_set_ops(TpmOps *ops)
{
	die_if(tpm_ops, "%s: TPM ops already set.\n", __func__);
	tpm_ops = ops;

	if (CONFIG_TPM_DEBUG_EXTENSIONS)
		tpm_ops->report_state = tpm_internal_state;

	tpm_ops->get_mode = tpm_internal_get_mode;
	tpm_ops->set_mode = tpm_internal_set_mode;
}

int tpm_xmit(const uint8_t *sendbuf, size_t send_size,
	     uint8_t *recvbuf, size_t *recv_len)
{
	die_if(!tpm_ops, "%s: No TPM ops set.\n", __func__);
	return tpm_ops->xmit(tpm_ops, sendbuf, send_size, recvbuf, recv_len);
}

char *tpm_report_state(void)
{
	if (!tpm_ops || !tpm_ops->report_state)
		return NULL;

	return tpm_ops->report_state(tpm_ops);
}

int tpm_get_mode(uint8_t *mode_val)
{
	if (!tpm_ops)
		return -1;

	return tpm_ops->get_mode(tpm_ops, mode_val);
}

int tpm_set_mode(uint8_t mode_val)
{
	if (!tpm_ops)
		return -1;

	return tpm_ops->set_mode(tpm_ops, mode_val);
}
