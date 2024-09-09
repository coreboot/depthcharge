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
#include <vb2_android_bootimg.h>

#include "boot/android_bootconfig_params.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"
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

/****************************** Android GKI ******************************/

static int gki_setup_bootconfig(struct boot_info *bi, struct vb2_kernel_params *kp)
{
	struct bootconfig_trailer *trailer;
	struct bootconfig bc;
	int ret;

	uintptr_t kernel_buffer_end = (uintptr_t)kp->kernel_buffer + kp->kernel_buffer_size;
	uintptr_t ramdisk_end = (uintptr_t)kp->ramdisk + kp->ramdisk_size;
	bootconfig_init(&bc, (void *)ramdisk_end, kernel_buffer_end - ramdisk_end);

	/*
	 * "bootconfig" is already included in the vendor_boot cmdline, if the
	 * vendor ramdisk contains non-empty bootconfig. Since in our case we're
	 * unconditionally adding some parameters this way, we'll need to make
	 * sure that kernel knows to parse them even if there's nothing
	 * in the vendor ramdisk.
	 */
	if (!kp->bootconfig_size)
		commandline_append("bootconfig");

	/* Append parameters from vendor image to bootconfig */
	ret = bootconfig_append_params(&bc, kp->bootconfig, kp->bootconfig_size);
	if (ret < 0) {
		printf("GKI: Cannot append build time bootconfig\n");
		return -1;
	}

	ret = bootconfig_append_cmdline(&bc, kp->vboot_cmdline_buffer);
	if (ret < 0) {
		printf("GKI: Cannot copy vboot cmdline to bootconfig\n");
		return -1;
	}

	append_android_bootconfig_params(&bc, kp);

	trailer = bootconfig_finalize(&bc, 0);
	if (!trailer) {
		printf("GKI: Cannot finalize bootconfig\n");
		return -1;
	}

	/* Update ramdisk size after adding bootconfig */
	bi->ramdisk_size += trailer->params_size + sizeof(*trailer);

	return 0;
}

static int fill_info_gki(struct boot_info *bi,
			 struct vb2_kernel_params *kparams)
{
	if (kparams->kernel_buffer == NULL) {
		printf("Pointer to kernel buffer is not initialized\n");
		return -1;
	}

	/* gki_setup_bootconfig() expects ramdisk to be part of kernel buffer */
	assert((void *)kparams->ramdisk > kparams->kernel_buffer &&
	       kparams->ramdisk + kparams->ramdisk_size <=
	       (uint8_t *)kparams->kernel_buffer + kparams->kernel_buffer_size);

	/* Kernel starts at the beginning of kernel buffer */
	bi->kernel = kparams->kernel_buffer + BOOT_HEADER_SIZE;
	bi->ramdisk_addr = kparams->ramdisk;
	bi->ramdisk_size = kparams->ramdisk_size;
	bi->cmd_line = kparams->vendor_cmdline_buffer;

	if (CONFIG(BOOTCONFIG)) {
		if (gki_setup_bootconfig(bi, kparams))
			return -1;
	}

	return 0;
}

int fill_boot_info(struct boot_info *bi, struct vb2_kernel_params *kparams)
{
	uint32_t type = GET_KERNEL_IMG_TYPE(kparams->flags);

	if (type == KERNEL_IMAGE_CROS) {
		return fill_info_cros(bi, kparams);
	} else if (type == KERNEL_IMAGE_BOOTIMG) {
		return fill_info_gki(bi, kparams);
#if CONFIG(KERNEL_MULTIBOOT)
	} else if (type == KERNEL_IMAGE_MULTIBOOT) {
		return fill_info_multiboot(bi, kparams);
#endif
	} else {
		printf("%s: Invalid image type %x!\n", __func__, type);
		return -1;
	}
}
