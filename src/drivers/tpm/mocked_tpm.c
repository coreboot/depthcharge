/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <tss_constants.h>

#include "drivers/tpm/tpm.h"

void tpm_set_ops(TpmOps *ops)
{
}

int tpm_xmit(const uint8_t *sendbuf, size_t send_size,
	     uint8_t *recvbuf, size_t *recv_len)
{
	return TPM_SUCCESS;
}

char *tpm_report_state(void)
{
	return NULL;
}

int tpm_set_tpm_mode(uint8_t mode_val)
{
	return TPM_SUCCESS;
}

int tpm_get_boot_mode(uint8_t *boot_mode)
{
	return TPM_E_NO_SUCH_COMMAND;
}
