/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOOT_ANDROID_PVMFW_H_
#define _BOOT_ANDROID_PVMFW_H_

#include <stddef.h>
#include <vb2_api.h>

#define ANDROID_PVMFW_CFG_ALIGN 4096

/**
 * Pvmfw loading and setup status codes.
 */
enum pvmfw_status {
	/* pvmfw has been loaded and setup successfully */
	PVMFW_SUCCESS = 0,

	/* pvmfw buffer is too small to fit the configuration */
	PVMFW_ERR_CFG_BUFFER_TOO_SMALL = -1,

	/* Allocation failed */
	PVMFW_ERR_NO_MEM = -2,

	/* Device tree operation failed */
	PVMFW_ERR_DT_CREATE_FAIL = -3,

	/* Failed to parse boot params, payload too small */
	PVMFW_ERR_CBOR_TOO_SMALL = -4,

	/* Failed to parse boot params, invalid payload known values */
	PVMFW_ERR_CBOR_MISMATCH = -5,
};

/**
 * Setups a pvmfw's configuration in the buffer using the boot parameters
 * from the GSC.
 *
 * The function will update the contents of the buffer with the configuration
 * and update pvmfw_size accordingly to include it.
 *
 * @param kparams	The result of vboot verification.
 * @param pvmfw_size	If successful the final pvmfw size with the
 *			configuration will be updated.
 * @param params	pvmfw boot parameters from secdata_get_pvmfw_params.
 * @param params_size	Size of the pvmfw boot parameters.
 *
 * @return 0 if successful and other value otherwise.
 */
int setup_android_pvmfw(const struct vb2_kernel_params *kparams, size_t *pvmfw_size,
			const void *params, size_t params_size);

#endif /* _BOOT_ANDROID_PVMFW_H_ */
