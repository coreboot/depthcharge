/* Copyright 2013 The ChromiumOS Authors
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
#include "vboot/widevine.h"

#define PCR_KERNEL_VER 10

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

uint32_t secdata_widevine_prepare(struct vb2_context *ctx)
{
	RETURN_ON_FAILURE(prepare_widevine_root_of_trust(ctx));

	RETURN_ON_FAILURE(prepare_widevine_tpm_pubkey());

	return TPM_SUCCESS;
}

uint32_t secdata_extend_kernel_pcr(struct vb2_context *ctx)
{
	uint8_t buffer[VB2_PCR_DIGEST_RECOMMENDED_SIZE] = {};
	uint32_t size = sizeof(buffer);
	vb2_error_t rv;

	rv = vb2api_get_pcr_digest(ctx, KERNEL_VERSION_PCR, buffer, &size);
	if (rv != VB2_SUCCESS)
		return TPM_E_INTERNAL_INCONSISTENCY;

	RETURN_ON_FAILURE(TlclExtend(PCR_KERNEL_VER, buffer, buffer));

	return TPM_SUCCESS;
}

#if CONFIG(ANDROID_PVMFW)
uint32_t secdata_get_pvmfw_params(void **boot_params, size_t *params_size)
{
	void *params = NULL;
	uint32_t ret;
	uint32_t unused_attrs;
	uint32_t space_size;
	char unused_auth_policy[256];
	uint32_t auth_policy_size = sizeof(unused_auth_policy);

	*boot_params = NULL;
	*params_size = 0;

	/* Get the BootParam space size */
	RETURN_ON_FAILURE(TlclGetSpaceInfo(ANDROID_PVMFW_BOOT_PARAMS_NV_INDEX,
					   &unused_attrs, &space_size,
					   &unused_auth_policy,
					   &auth_policy_size));
	if (space_size == 0) {
		printf("Failed to get pvmfw gsc boot params size\n");
		return TPM_E_INVALID_RESPONSE;
	}

	/* Allocate a buffer for BootParam CBOR object */
	params = malloc(space_size);
	if (!params) {
		printf("Failed to allocate buffer for pvmfw params\n");
		return TPM_E_RESPONSE_TOO_LARGE;
	}

	/* Get contents of BootParam object from NV space */
	ret = TlclRead(ANDROID_PVMFW_BOOT_PARAMS_NV_INDEX, params, space_size);
	if (ret != TPM_SUCCESS) {
		printf("TPM: %#x returned by TlclRead"
		       "(ANDROID_PVMFW_BOOT_PARAMS_NV_INDEX)\n", (int)ret);
		goto fail;
	}

	*boot_params = params;
	*params_size = space_size;
	return 0;
fail:
	/*
	 * Make sure that secrets are no longer in memory, if TlclRead
	 * somehow only read partial data.
	 */
	memset(params, 0, space_size);
	free(params);
	return ret;
}
#endif
