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

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>
#include <stdlib.h>

#include "boot/multiboot.h"
#include "vboot/boot.h"
#include "vboot/boot_info.h"

/************************* CrOS Image Parsing ****************************/

static int fill_info_cros(struct boot_info *bi,
			  struct vb2_kernel_params *kparams)
{
	bi->kernel = kparams->kernel_buffer;
	bi->loader = (uint8_t *)bi->kernel + kparams->bootloader_offset;
	bi->params = (uint8_t *)bi->loader - CrosParamSize;
	bi->cmd_line = (char *)bi->params - CmdLineSize;

	return 0;
}

/*********************** Multiboot Image Parsing *************************/
#if CONFIG(KERNEL_MULTIBOOT)
static int fill_info_multiboot(struct boot_info *bi,
			       struct vb2_kernel_params *kparams)
{
	bi->kparams = kparams;

	if (multiboot_fill_boot_info(bi) < 0)
		return -1;

	return 0;
}
#endif

int fill_boot_info(struct boot_info *bi, struct vb2_kernel_params *kparams)
{
	uint32_t type = GET_KERNEL_IMG_TYPE(kparams->flags);

	if (type == KERNEL_IMAGE_CROS) {
		return fill_info_cros(bi, kparams);
#if CONFIG(KERNEL_MULTIBOOT)
	} else if (type == KERNEL_IMAGE_MULTIBOOT) {
		return fill_info_multiboot(bi, kparams);
#endif
	} else {
		printf("%s: Invalid image type %x!\n", __func__, type);
		return -1;
	}
}
