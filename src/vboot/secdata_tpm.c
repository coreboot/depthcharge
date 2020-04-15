/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking secure data spaces
 * stored in the TPM NVRAM.
 */

#include <stdio.h>
#include <tlcl.h>
#include <tss_constants.h>
#include <vb2_api.h>

#include "vboot/secdata_tpm.h"
#include "vboot/util/misc.h"

#define RETURN_ON_FAILURE(tpm_command) do { \
		uint32_t result_; \
		if ((result_ = (tpm_command)) != TPM_SUCCESS) { \
			printf("TPM: %#x returned by " #tpm_command \
			       "\n", (int)result_); \
			return result_; \
		} \
	} while (0)

/* Keeps track of whether the kernel space has already been locked or not. */
int secdata_kernel_locked = 0;

/**
 * Issue a TPM_Clear and reenable/reactivate the TPM.
 */
static uint32_t secdata_clear_and_reenable(void)
{
	printf("TPM: secdata_clear_and_reenable\n");
	RETURN_ON_FAILURE(TlclForceClear());
	RETURN_ON_FAILURE(TlclSetEnable());
	RETURN_ON_FAILURE(TlclSetDeactivated(0));

	return TPM_SUCCESS;
}

/**
 * Like TlclWrite(), but checks for write errors due to hitting the 64-write
 * limit and clears the TPM when that happens.  This can only happen when the
 * TPM is unowned, so it is OK to clear it (and we really have no choice).
 * This is not expected to happen frequently, but it could happen.
 */
static uint32_t secdata_safe_write(uint32_t index, const void *data,
				   uint32_t length)
{
	uint32_t result = TlclWrite(index, data, length);
	if (result == TPM_E_MAXNVWRITES) {
		RETURN_ON_FAILURE(secdata_clear_and_reenable());
		return TlclWrite(index, data, length);
	} else {
		return result;
	}
}

/* Functions to read and write firmware and kernel spaces. */

uint32_t secdata_firmware_write(struct vb2_context *ctx)
{
	if (!(ctx->flags & VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED)) {
		printf("TPM: secdata_firmware unchanged\n");
		return TPM_SUCCESS;
	}

	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
		printf("Error: secdata_firmware modified "
			  "in non-recovery mode?\n");
		return TPM_E_AREA_LOCKED;
	}

	PRINT_BYTES("TPM: write secdata_firmware", &ctx->secdata_firmware);
	RETURN_ON_FAILURE(secdata_safe_write(FIRMWARE_NV_INDEX,
					     ctx->secdata_firmware,
					     VB2_SECDATA_FIRMWARE_SIZE));

	ctx->flags &= ~VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	return TPM_SUCCESS;
}

uint32_t secdata_kernel_write(struct vb2_context *ctx)
{
	if (!(ctx->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED)) {
		printf("TPM: secdata_kernel unchanged\n");
		return TPM_SUCCESS;
	}

	/* Learn the expected size. */
	uint8_t size = VB2_SECDATA_KERNEL_MIN_SIZE;
	vb2api_secdata_kernel_check(ctx, &size);

	PRINT_N_BYTES("TPM: write secdata_kernel", &ctx->secdata_kernel, size);
	RETURN_ON_FAILURE(secdata_safe_write(KERNEL_NV_INDEX,
					     ctx->secdata_kernel, size));

	ctx->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	return TPM_SUCCESS;
}

uint32_t secdata_kernel_lock(struct vb2_context *ctx)
{
	/* Skip if already locked */
	if (secdata_kernel_locked) {
		printf("TPM: secdata_kernel already locked; skipping\n");
		return TPM_SUCCESS;
	}

	RETURN_ON_FAILURE(TlclLockPhysicalPresence());

	printf("TPM: secdata_kernel locked\n");
	secdata_kernel_locked = 1;
	return TPM_SUCCESS;
}

uint32_t secdata_fwmp_read(struct vb2_context *ctx)
{
	uint8_t size = VB2_SECDATA_FWMP_MIN_SIZE;
	uint32_t r;

	/* Try to read entire 1.0 struct */
	r = TlclRead(FWMP_NV_INDEX, ctx->secdata_fwmp, size);
	if (TPM_E_BADINDEX == r) {
		/* Missing space is not an error; tell vboot */
		printf("TPM: no secdata_fwmp space\n");
		ctx->flags |= VB2_CONTEXT_NO_SECDATA_FWMP;
		return TPM_SUCCESS;
	} else if (TPM_SUCCESS != r) {
		printf("TPM: read secdata_fwmp returned %#x\n", r);
		return r;
	}

	/* Re-read more data if necessary */
	if (vb2api_secdata_fwmp_check(ctx, &size) ==
	    VB2_ERROR_SECDATA_FWMP_INCOMPLETE)
		RETURN_ON_FAILURE(TlclRead(FWMP_NV_INDEX, ctx->secdata_fwmp,
					   size));

	return TPM_SUCCESS;
}
