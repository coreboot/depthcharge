/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _BOOT_ANDROID_PVMFW_H_
#define _BOOT_ANDROID_PVMFW_H_

#include <stddef.h>

#define ANDROID_PVMFW_CFG_ALIGN 4096

/**
 * Setups a pvmfw's configuration in the buffer using the boot parameters
 * from the GSC.
 *
 * The function will update the contents of the buffer with the configuration
 * and update pvmfw_size accordingly to include it.
 *
 * @param buffer	Buffer with loaded pvmfw code and a place where
 *			pvmfw configuration will be put by this function.
 * @param buffer_size	Maximum size of the buffer.
 * @param pvmfw_size	Pointer to the size of the loaded pvmfw code in the
 *			buffer during the function call and the updated size
 *			of pvmfw with the configuration after the function
 *			returns.
 * @param params	pvmfw boot parameters from secdata_get_pvmfw_params.
 * @param params_size	Size of the pvmfw boot parameters.
 *
 * @return 0 if successful and other value otherwise.
 */
int setup_android_pvmfw(void *buffer, size_t buffer_size, size_t *pvmfw_size,
			const void *params, size_t params_size);

#endif /* _BOOT_ANDROID_PVMFW_H_ */
