/*
 * Copyright 2012 Google Inc.
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

#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>

#include "drivers/tpm/tpm.h"

VbError_t VbExTpmInit(void)
{
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmClose(void)
{
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmOpen(void)
{
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmSendReceive(const uint8_t *request, uint32_t request_length,
			     uint8_t *response, uint32_t *response_length)
{
	size_t len = *response_length;
	if (tpm_xmit(request, request_length, response, &len))
		return VBERROR_UNKNOWN;
	/* check 64->32bit overflow and (re)check response buffer overflow */
	if (len > *response_length)
		return VBERROR_UNKNOWN;
	*response_length = len;
	return VBERROR_SUCCESS;
}

static int tpm_mode_vb2ex_translate(int ret) {
	switch (ret) {
	case TPM_SUCCESS:
		return VB2_SUCCESS;
	case TPM_E_NO_SUCH_COMMAND:
		return VB2_ERROR_EX_TPM_NO_SUCH_COMMAND;
	default:
		return VB2_ERROR_UNKNOWN;
	}
}

int vb2ex_tpm_get_mode(enum vb2_tpm_mode *mode_val)
{
	/* Cast safely to uint8_t, since we know enum vb2_tpm_mode values
	 * correspond directly to TPM mode values. */
	return tpm_mode_vb2ex_translate(tpm_get_mode((uint8_t *)mode_val));
}

int vb2ex_tpm_set_mode(enum vb2_tpm_mode mode_val)
{
	/* Cast safely to uint8_t, since we know enum vb2_tpm_mode values
	 * correspond directly to TPM mode values. */
	return tpm_mode_vb2ex_translate(tpm_set_mode((uint8_t)mode_val));
}
