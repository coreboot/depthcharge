/*
 * Copyright 2015 Google LLC
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VBOOT_BOOT_INFO_H__
#define __VBOOT_BOOT_INFO_H__

#include <vb2_api.h>

#include "vboot/boot.h"

/*
 * Flags field in vboot kernel preamble is defined as:
 * [31:2] - Reserved (for future use)
 * [1:0]  - Image type (0x0 = CrOS, 0x1 = Bootimg, 0x2 = Multiboot)
 */
#define KERNEL_IMG_TYPE_MASK	(0x3)
#define KERNEL_IMG_TYPE_SHIFT	(0x0)

#define GET_KERNEL_IMG_TYPE(x)					\
	(((x) >> KERNEL_IMG_TYPE_SHIFT) & KERNEL_IMG_TYPE_MASK)

typedef enum {
	KERNEL_IMAGE_CROS = 0 << KERNEL_IMG_TYPE_SHIFT,
	KERNEL_IMAGE_BOOTIMG = 1 << KERNEL_IMG_TYPE_SHIFT,
	KERNEL_IMAGE_MULTIBOOT = 2 << KERNEL_IMG_TYPE_SHIFT,
} kernel_img_type_t;

enum {
	CmdLineSize = 4096,
	CrosParamSize = 4096,
};

/*
 * Depending upon kernel image type, this function selects the appropriate function
 * to fill up the boot_info structure.
 *
 * Returns 0 on success, -1 on error. Error can occur if the image type handler
 * is not able to setup the boot info structure for any reason.
 */
int fill_boot_info(struct boot_info *bi, struct vb2_kernel_params *kparams);

#endif /* __VBOOT_BOOT_INFO_H__ */
