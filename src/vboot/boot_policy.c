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

#include "boot/multiboot.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"

/*
 * Boot policy settings. By default, we select CrOS image type and command line
 * as the one filled by the signer. This is important to maintain consistency
 * with current boards.
 */
static const struct boot_policy boot_policy[] = {
#if CONFIG(KERNEL_MULTIBOOT)
	{
		.img_type = KERNEL_IMAGE_MULTIBOOT,
		.cmd_line_loc = CMD_LINE_SIGNER,
	},
#endif
	{
		.img_type = KERNEL_IMAGE_CROS,
		.cmd_line_loc = CMD_LINE_SIGNER,
	},
	{
		.img_type = KERNEL_IMAGE_BOOTIMG,
		.cmd_line_loc = CMD_LINE_BOOTIMG_HDR,
	},
};

static struct {
	size_t count;
	const struct boot_policy *policy;
} curr_policy = {
	.count = ARRAY_SIZE(boot_policy),
	.policy = boot_policy,
};

typedef int (*fill_bootinfo_fnptr)(struct boot_info *bi,
				   struct vb2_kernel_params *kparams,
				   const struct boot_policy *policy);

/************************* CrOS Image Parsing ****************************/

static int fill_info_cros(struct boot_info *bi,
			  struct vb2_kernel_params *kparams,
			  const struct boot_policy *policy)
{
	bi->kernel = kparams->kernel_buffer;
	bi->loader = (uint8_t *)bi->kernel + kparams->bootloader_offset;
	bi->params = (uint8_t *)bi->loader - CrosParamSize;

	if (policy->cmd_line_loc == CMD_LINE_SIGNER)
		bi->cmd_line = (char *)bi->params - CmdLineSize;
	else
		bi->cmd_line = NULL;

	return 0;
}

/*********************** Multiboot Image Parsing *************************/
#if CONFIG(KERNEL_MULTIBOOT)
static int fill_info_multiboot(struct boot_info *bi,
			       struct vb2_kernel_params *kparams,
			       const struct boot_policy *policy)
{
	bi->kparams = kparams;

	if (multiboot_fill_boot_info(bi) < 0)
		return -1;

	if (policy->cmd_line_loc != CMD_LINE_SIGNER)
		bi->cmd_line = NULL;

	return 0;
}
#endif

/************************* Android GKI *******************************/
#define ANDROID_GKI_BOOT_HDR_SIZE 4096

static int gki_setup_ramdisk(struct boot_info *bi,
			     struct vb2_kernel_params *kparams,
			     int fill_cmdline)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	struct boot_img_hdr_v4 *init_hdr;
	uint8_t *init_boot_ramdisk_src;
	uint8_t *init_boot_ramdisk_dst;
	uint32_t vendor_ramdisk_section_offset;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);
	init_hdr = (struct boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
					      kparams->init_boot_offset);

	/*
	 * TODO: For now assuming single ramdisk in vendor_hdr, do not load more
	 * than one ramdisk. This is a WIP solution, need to improve based on
	 * mode.
	 */
	if (vendor_hdr->vendor_ramdisk_table_entry_num > 1) {
		printf("GKI: Loading more than single ramdisk is not supported\n");
		return -1;
	}

	/* init_boot partition should contain only ramdisk, kernel_size should be 0 */
	if (init_hdr->kernel_size != 0) {
		printf("GKI: Kernel size on init_boot partition has to be zero\n");
		return -1;
	}

	/* Calculate address offset of vendor_ramdisk section on vendor_boot partition */
	vendor_ramdisk_section_offset = ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4),
						 vendor_hdr->page_size);

	init_boot_ramdisk_dst = ((uint8_t *)vendor_hdr +
				vendor_ramdisk_section_offset +
				vendor_hdr->vendor_ramdisk_size);

	/* On init_boot there's no kernel, so ramdisk follows the header */
	init_boot_ramdisk_src = (uint8_t *)init_hdr + ANDROID_GKI_BOOT_HDR_SIZE;

	/* Move init_boot ramdisk to directly follow the vendor_boot ramdisk.
	 * This is a requirement from Android system. The cpio/gzip/lz4
	 * compression formats support this type of concatenation. After
	 * the kernel decompresses, it extracts contatenated file into
	 * an initramfs, which results in a file structure that's a generic
	 * ramdisk (from init_boot) overlaid on the vendor ramdisk (from
	 * vendor_boot) file structure. */
	memmove(init_boot_ramdisk_dst, init_boot_ramdisk_src, init_hdr->ramdisk_size);

	/* Update ramdisk addr and size */
	bi->ramdisk_addr = (uint8_t *)vendor_hdr + vendor_ramdisk_section_offset;
	bi->ramdisk_size = vendor_hdr->vendor_ramdisk_size + init_hdr->ramdisk_size;

	if (fill_cmdline)
		bi->cmd_line = (char *)vendor_hdr->cmdline;

	return 0;
}

static int fill_info_gki(struct boot_info *bi,
			 struct vb2_kernel_params *kparams,
			 const struct boot_policy *policy)
{
	if (kparams->kernel_buffer == NULL) {
		printf("Pointer to kernel buffer is not initialized\n");
		return -1;
	}

	/* Kernel starts at the beginning of kernel buffer */
	bi->kernel = kparams->kernel_buffer + ANDROID_GKI_BOOT_HDR_SIZE;

	if (gki_setup_ramdisk(bi, kparams, 1))
		return -1;

	return 0;
}

/*
 * Table describing allowed command line locations and parsing functions for a
 * given image type.
 */
static const struct {
	uint32_t allowed_cmdline;
	fill_bootinfo_fnptr fn;
} img_type_info[] = {
	[KERNEL_IMAGE_CROS] = {CMD_LINE_SIGNER | CMD_LINE_DTB, fill_info_cros},
	[KERNEL_IMAGE_BOOTIMG] = {CMD_LINE_BOOTIMG_HDR,
				  fill_info_gki},
#if CONFIG(KERNEL_MULTIBOOT)
	[KERNEL_IMAGE_MULTIBOOT] = {CMD_LINE_SIGNER, fill_info_multiboot},
#endif
};

/* Returns 0 if img type allows given command line location, else -1 */
static int check_policy(kernel_img_type_t type,
			cmd_line_loc_t cmd_line_loc)
{
	/*
	 * If given location for cmd line is allowed for this image type, return
	 * 0.
	 */
	if ((img_type_info[type].allowed_cmdline & cmd_line_loc)
	    == cmd_line_loc)
		return 0;

	/* Not allowed: return -1 */
	return -1;
}

/*
 * Allows board to set a boot policy based on:
 * a) Image type that the board supports - CrOS / Bootimg
 * b) Location of command line
 *
 * Returns 0 on success, -1 on error. Error can occur if a particular image type
 * does not support provided location for command line.
 */
int set_boot_policy(const struct boot_policy *policy, size_t count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (check_policy(policy[i].img_type,
				 policy[i].cmd_line_loc) == -1) {
			printf("Boot Policy: Invalid type-cmdline combo\n");
			return -1;
		}
	}

	curr_policy.count = count;
	curr_policy.policy = policy;

	printf("Boot policy: Policy set by board\n");

	return 0;
}

/*
 * Depending upon the boot policy set by the board for supported image type,
 * this function selects the appropriate function pointer to fill up the
 * boot_info structure.
 *
 * Returns 0 on success, -1 on error. Error can occur if the image type handler
 * is not able to setup the boot info structure for any reason.
 *
 * This function assumes that the board provides a prioritized list of image
 * types that it wants to boot. In the default case, we let only CrOS images
 * with cmd line filled by signer boot.
 */
int fill_boot_info(struct boot_info *bi, struct vb2_kernel_params *kparams)
{
	int i;
	const struct boot_policy *policy;
	uint32_t type = GET_KERNEL_IMG_TYPE(kparams->flags);

	for (i = 0; i < curr_policy.count; i++) {
		if (type == curr_policy.policy[i].img_type)
			break;
	}

	if (i == curr_policy.count) {
		printf("Boot policy: Invalid image type %x!\n", type);
		return -1;
	}
	policy = &curr_policy.policy[i];

	printf("Boot policy: Match for type %x with cmdline %x\n",
	       policy->img_type, policy->cmd_line_loc);

	return img_type_info[policy->img_type].fn(bi, kparams, policy);
}
