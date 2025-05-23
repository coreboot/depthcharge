/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ANDROID_BOOTCONFIG_PARAMS_H_
#define _ANDROID_BOOTCONFIG_PARAMS_H_

#include <vb2_api.h>
#include <vboot/boot.h>

#include "bootconfig.h"

#define BOOTCONFIG_BOOTTIME_KEY_STR "androidboot.boottime"
#define BOOTCONFIG_MAX_BOOTTIME_STR "firmware:18446744073709551615,splash:18446744073709551615"

/*
 * Append androidboot bootconfig parameters to bootconfig section.
 *
 * @param bc pointer to the bootconfig structure
 * @param kp pointer to kernel parameters
 *
 * Return: Return 0 on success, -1 in case of errors
 */
int append_android_bootconfig_params(struct bootconfig *bc, struct vb2_kernel_params *kp);

/*
 * Append Android bootconfig boottime.
 *
 * @param bi - pointer to boot_info structure
 */
int append_android_bootconfig_boottime(struct boot_info *bi);

#endif /* _ANDROID_BOOTCONFIG_PARAMS_H_ */
