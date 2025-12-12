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
#include <lp_vboot.h>
#include <stdint.h>
#include <stdlib.h>
#include <tss_constants.h>
#include <vb2_android_bootimg.h>
#include <vboot_api.h>

#include "base/android_misc.h"
#include "base/gpt.h"
#include "base/timestamp.h"
#include "boot/android_bootconfig_params.h"
#include "boot/android_dtboimg.h"
#include "boot/android_pvmfw.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"
#include "boot/multiboot.h"
#include "vboot/boot.h"
#include "vboot/boot_info.h"
#include "vboot/secdata_tpm.h"
#include "vboot/stages.h"

/************************* CrOS Image Parsing ****************************/

static int fill_info_cros(struct boot_info *bi,
			  struct vb2_kernel_params *kparams)
{
	bi->kernel = kparams->kernel_buffer;
	bi->loader = (uint8_t *)bi->kernel + kparams->bootloader_offset;
	bi->params = (uint8_t *)bi->loader - CrosParamSize;
	bi->cmd_line = (char *)bi->params - CmdLineSize;
	bi->kernel_size = (void *)bi->cmd_line - bi->kernel;

	commandline_append("cros_secure");

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

static int gki_setup_bootconfig(struct boot_info *bi, struct vb2_kernel_params *kp,
				char *fb_cmd)
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

	ret = bootconfig_append_cmdline(&bc, kp->bootconfig_cmdline_buffer);
	if (ret < 0) {
		printf("GKI: Cannot copy vboot cmdline to bootconfig\n");
		return -1;
	}

	if (fb_cmd) {
		ret = bootconfig_append_params(&bc, fb_cmd, strlen(fb_cmd));
		if (ret < 0) {
			printf("GKI: Cannot append fastboot bootconfig\n");
			return -1;
		}
	}

	append_android_bootconfig_params(&bc, kp);

	trailer = bootconfig_finalize(&bc,
			sizeof(BOOTCONFIG_BOOTTIME_KEY_STR "=" BOOTCONFIG_MAX_BOOTTIME_STR) +
			sizeof(BOOTCONFIG_DELIMITER));
	if (!trailer) {
		printf("GKI: Cannot finalize bootconfig\n");
		return -1;
	}

	/* Update ramdisk size after adding bootconfig */
	bi->ramdisk_size += trailer->params_size + sizeof(*trailer);

	return 0;
}

/*
 * Fill struct boot_info with pvmfw information and fill pvmfw config.
 */
static int setup_pvmfw(struct boot_info *bi, struct vb2_kernel_params *kparams)
{
	int ret;
	uint32_t status;
	size_t pvmfw_size = kparams->pvmfw_out_size, params_size;
	void *pvmfw_addr = kparams->pvmfw_buffer, *params = NULL;

	timestamp_add_now(TS_PVMFW_SETUP_START);

	if (!pvmfw_addr || pvmfw_size == 0) {
		/* There is no pvmfw so fail and don't do anything */
		printf("pvmfw was not loaded\n");
		return -1;
	}

	/* Get pvmfw boot params from GSC */
	status = secdata_get_pvmfw_params(&params, &params_size);
	if (status != TPM_SUCCESS) {
		printf("Failed to get pvmfw gsc boot params data. "
		       "secdata_get_pvmfw_params returned %u\n", status);
		ret = -1;
		goto fail;
	}

	timestamp_add_now(TS_PVMFW_GSC_NVRAM_DONE);

	/* Verify that pvmfw start address is aligned */
	if (!IS_ALIGNED((uintptr_t)pvmfw_addr, ANDROID_PVMFW_CFG_ALIGN)) {
		printf("Failed to setup pvmfw at aligned address\n");
		ret = -1;
		goto fail;
	}

	ret = setup_android_pvmfw(pvmfw_addr,
				  kparams->pvmfw_buffer_size,
				  &pvmfw_size, params, params_size);
	if (ret != 0) {
		printf("Failed to setup pvmfw configuration\n");
		goto fail;
	}

	/* Add pvmfw to kernel cmdline on x86 */
	if (CONFIG(ARCH_X86)) {
		char str_to_insert[100];
		uint64_t addr = (uintptr_t)pvmfw_addr;
		uint64_t size = (uint64_t)pvmfw_size;

		/*
		 * Set pvmfw to point kernel to where to find pvmfw payload and
		 * memmap to protected the memory to make sure that kernel will
		 * not overwrite the pvmfw with its data.
		 */
		if (snprintf(str_to_insert, sizeof(str_to_insert),
			     "memmap=0x%" PRIx64 "$0x%" PRIx64
			     " pvmfw=0x%" PRIx64 "@0x%" PRIx64,
			     size, addr, size, addr) < 0) {
			printf("Failed to snprintf a pvmfw command line\n");
			ret = -1;
			goto fail;
		}

		commandline_append(str_to_insert);
	}

	bi->pvmfw_addr = pvmfw_addr;
	bi->pvmfw_size = pvmfw_size;
	bi->pvmfw_buffer_size = kparams->pvmfw_buffer_size;
fail:
	if (params) {
		/* Make sure that secrets are no longer in memory */
		memset(params, 0, params_size);
		free(params);
	}

	/* If failed then clear the buffer */
	if (ret != 0)
		memset(kparams->pvmfw_buffer, 0, kparams->pvmfw_buffer_size);

	timestamp_add_now(TS_PVMFW_SETUP_DONE);

	return ret;
}

static int fill_info_gki(struct boot_info *bi,
			 struct vb2_kernel_params *kparams)
{
	struct android_misc_oem_cmdline fastboot_cmd;
	char *fastboot_bootconfig = NULL;

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
	bi->cmd_line = kparams->real_cmdline_ptr;

	if ((vb2api_gbb_get_flags(vboot_get_context()) & VB2_GBB_FLAG_FORCE_UNLOCK_FASTBOOT) ||
	    strstr(kparams->bootconfig_cmdline_buffer,
		   "androidboot.verifiedbootstate=orange")) {
		BlockDev *bdev = (BlockDev *)kparams->disk_handle;
		GptData *gpt = alloc_gpt(bdev);
		if (gpt != NULL && !android_misc_oem_cmdline_read(bdev, gpt, &fastboot_cmd)) {
			fastboot_bootconfig = android_misc_get_oem_cmd(&fastboot_cmd, true);
			commandline_append(android_misc_get_oem_cmd(&fastboot_cmd, false));
		}

		if (gpt != NULL)
			free_gpt(bdev, gpt);
	}

	/* Parse devicetree before setting up bootconfig. This will allow to populate
	   androidboot.dtbo_idx bootconfig parameter for use by FirmwareDtboVerification. */
	struct boot_img_hdr_v4 *boot_hdr = (struct boot_img_hdr_v4 *)kparams->kernel_buffer;

	bi->kernel_size = boot_hdr->kernel_size;
	if (CONFIG(ARCH_ARM))
		bi->dt = android_parse_dtbs(kparams->dtbo, kparams->dtbo_size);

	if (CONFIG(BOOTCONFIG)) {
		if (gki_setup_bootconfig(bi, kparams, fastboot_bootconfig))
			return -1;
	}

	if (CONFIG(ANDROID_PVMFW)) {
		if (setup_pvmfw(bi, kparams))
			printf("Failed to setup pvmfw\n");
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
