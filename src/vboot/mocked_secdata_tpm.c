/* Copyright 2012 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking secure data spaces
 * stored in the TPM NVRAM (mock versions).
 */

#include <tss_constants.h>
#include <vb2_api.h>

#include "secdata_tpm.h"

int secdata_kernel_locked = 0;

uint32_t secdata_firmware_write(struct vb2_context *ctx)
{
	ctx->flags &= ~VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	return TPM_SUCCESS;
}

uint32_t secdata_kernel_write(struct vb2_context *ctx)
{
	ctx->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	return TPM_SUCCESS;
}

uint32_t secdata_kernel_lock(struct vb2_context *ctx)
{
	secdata_kernel_locked = 1;
	return TPM_SUCCESS;
}

uint32_t secdata_fwmp_read(struct vb2_context *ctx)
{
	ctx->flags |= VB2_CONTEXT_NO_SECDATA_FWMP;
	return TPM_SUCCESS;
}

uint32_t secdata_widevine_prepare(struct vb2_context *ctx)
{
	return TPM_SUCCESS;
}

uint32_t secdata_extend_kernel_pcr(struct vb2_context *ctx)
{
	return TPM_SUCCESS;
}
