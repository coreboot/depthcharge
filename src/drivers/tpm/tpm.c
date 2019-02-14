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
#include <tss_constants.h>

#include "config.h"
#include "drivers/tpm/tpm.h"
#include "drivers/tpm/tpm_utils.h"

static TpmOps *tpm_ops;

void tpm_set_ops(TpmOps *ops)
{
	die_if(tpm_ops, "%s: TPM ops already set.\n", __func__);
	tpm_ops = ops;

	if (CONFIG_TPM_DEBUG_EXTENSIONS)
		tpm_ops->report_state = tpm_internal_state;
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

int tpm_set_mode(uint8_t mode_val)
{
	struct tpm_vendor_header *h;
	uint8_t *m;
	uint8_t buffer[sizeof(*h) + sizeof(*m)];

	uint32_t header_code;

	size_t recv_len = sizeof(buffer);

	if (!tpm_ops)
		return -1;

	/* Command to send to the TPM and its response. */
	h = (struct tpm_vendor_header *)&buffer[0];

	/* Mode sent to the TPM, and TPM's response (overwritten by xmit). */
	m = (uint8_t *)(h + 1);

	cr50_fill_vendor_cmd_header(h, VENDOR_CC_TPM_MODE, sizeof(*m));
	marshal_u8(m, mode_val);

	if (tpm_ops->xmit(tpm_ops, (const uint8_t *)h, sizeof(buffer),
			  (uint8_t *)h, &recv_len)) {
		printf("%s: IO error\n", __func__);
		return -1;
	}

	header_code = unmarshal_u32(&h->code);
	if (header_code == VENDOR_RC_NO_SUCH_COMMAND) {
		printf("%s: Command not supported\n", __func__);
		return TPM_E_INTERNAL_ERROR;
	}
	if (header_code) {
		printf("%s: Invalid header code: %d\n", __func__,
		       header_code);
		return -1;
	}
	if (recv_len != sizeof(buffer)) {
		printf("%s: Invalid response\n", __func__);
		return -1;
	}
	if (unmarshal_u8(m) != mode_val) {
		printf("%s: Invalid TPM mode response: %d (expect %d)\n",
		       __func__, unmarshal_u8(m), mode_val);
		return -1;
	}
	return TPM_SUCCESS;
}
