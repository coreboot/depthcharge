/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This module allows setting the TPM mode on Cr50.  It needs to be tightly
 * synchronised with the cr50 TPM codebase in src/third_party/tpm2
 */

#include <libpayload.h>

#include "drivers/tpm/tpm.h"
#include "drivers/tpm/tpm_utils.h"

int tpm_internal_mode(struct TpmOps *me, uint8_t mode_val)
{
	struct tpm_vendor_header *h;
	uint8_t *m;
	uint8_t out_mode;
	int ret = -1;
	uint32_t header_code;

	size_t buffer_size = sizeof(*h) + sizeof(*m);
	size_t recv_len = buffer_size;

	/* Command to send to the TPM and its response. */
	h = xzalloc(buffer_size);

	/* Mode sent to the TPM, and TPM's response (overwritten by xmit). */
	m = (uint8_t *)(h + 1);

	cr50_fill_vendor_cmd_header(h, VENDOR_CC_TPM_MODE, sizeof(*m));
	marshal_u8(m, mode_val);

	if (me->xmit(me, (const uint8_t *)h, buffer_size,
		     (uint8_t *)h, &recv_len) ||
	    (recv_len != buffer_size))
		printf("%s: Communications error\n", __func__);
	else if ((header_code = unmarshal_u32(&h->code)))
		printf("%s: Invalid header code: %d\n", __func__, header_code);
	else {
		out_mode = unmarshal_u8(m);
		if (out_mode != mode_val)
			printf("%s: Invalid TPM mode response: %d (expect %d)\n",
			       __func__, out_mode, mode_val);
		else
			/* TPM responded as expected. */
			ret = 0;
	}

	free(h);

	return ret;
}
