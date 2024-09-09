/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ANDROID_BOOTCONFIG_PARAMS_H_
#define _ANDROID_BOOTCONFIG_PARAMS_H_

#include <vb2_api.h>
#include <vboot/boot.h>

#include "bootconfig.h"

/*
 * Append androidboot bootconfig parameters to bootconfig section.
 *
 * @param bc pointer to the bootconfig structure
 * @param kp pointer to kernel parameters
 *
 * Return: Return 0 on success, -1 in case of errors
 */
int append_android_bootconfig_params(struct bootconfig *bc, struct vb2_kernel_params *kp);

#endif /* _ANDROID_BOOTCONFIG_PARAMS_H_ */
