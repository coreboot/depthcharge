/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common tpm utility functions
 */

#include <libpayload.h>

#include "drivers/tpm/tpm_utils.h"

void cr50_fill_vendor_cmd_header(struct tpm_vendor_header *h, u16 subcommand,
				 size_t content_size)
{
	marshal_u16(&h->tag, TPM_ST_NO_SESSIONS);
	marshal_u32(&h->size, sizeof(struct tpm_vendor_header) + content_size);
	marshal_u32(&h->code, TPM_CC_VENDOR_BIT);
	marshal_u16(&h->subcommand_code, subcommand);
}
