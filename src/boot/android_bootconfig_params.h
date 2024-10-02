/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ANDROID_BOOTCONFIG_PARAMS_H_
#define _ANDROID_BOOTCONFIG_PARAMS_H_

#include <vb2_api.h>

/*
 * Append androidboot bootconfig parameters to bootconfig section.
 *
 * @param kparams - pointer to vboot kernel parameter structure that is used as
 *                  a basis to access current and total size of bootconfig section
 * @param bootc_start - pointer to the bootconfig section
 *
 * Return: Return 0 on success, -1 in case of errors
 */
int append_android_bootconfig_params(struct vb2_kernel_params *kparams,
				     void *bootc_start);

/*
 * Fixup boottime in android bootconfig section.
 *
 * @param ramdisk - Ramdisk address which includes the bootconfig section at the end
 * @param ramdisk_size - Total size of the ramdisk
 * @param bootc_off - Offset of the bootconfig section within the ramdisk
 *
 * Return: 0 on success, -1 in case of errors
 */
int fixup_android_boottime(void *ramdisk, size_t ramdisk_size, size_t bootc_off);

#endif /* _ANDROID_BOOTCONFIG_PARAMS_H_ */
