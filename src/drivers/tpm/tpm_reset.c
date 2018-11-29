/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This module allows resetting the Cr50.  It needs to be tightly
 * synchronised with the cr50 TPM codebase in src/third_party/tpm2
 */

#include <libpayload.h>

#include "drivers/tpm/tpm.h"
#include "drivers/tpm/tpm_utils.h"

int tpm_internal_cr50_reset(struct TpmOps *me, uint16_t delay_ms)
{
	struct tpm_vendor_header *h;
	uint16_t *d;
	int ret = TPM_SUCCESS;
	uint32_t header_code;

	size_t buffer_size = sizeof(*h) + sizeof(*d);
	size_t recv_len_expected = sizeof(*h);
	size_t recv_len = recv_len_expected;

	/* Command to send to the TPM. */
	h = xzalloc(buffer_size);

	/* Delay sent to the TPM. */
	d = (uint16_t *)(h + 1);

	cr50_fill_vendor_cmd_header(h, VENDOR_CC_IMMEDIATE_RESET,
				    sizeof(*d));
	marshal_u16(d, delay_ms);

	printf("%s: Asking Cr50 to reset after %d ms\n", __func__, delay_ms);
	if (me->xmit(me, (const uint8_t *)h, buffer_size,
		     (uint8_t *)h, &recv_len)) {
		printf("%s: IO error\n", __func__);
		ret = -1;
	} else if ((header_code = unmarshal_u32(&h->code))) {
		printf("%s: Invalid header code: %d\n", __func__, header_code);
		ret = -1;
	} else if (recv_len != recv_len_expected) {
		printf("%s: Invalid response size\n", __func__);
		ret = -1;
	}

	free(h);
	return ret;
}
