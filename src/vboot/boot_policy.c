/*
 * Copyright 2015 Google Inc.
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

#include "base/string_utils.h"
#include "boot/bootconfig.h"
#include "boot/multiboot.h"
#include "vboot/android_image_hdr.h"
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
		.img_type = KERNEL_IMAGE_ANDROID_GKI,
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
				   VbSelectAndLoadKernelParams *kparams,
				   const struct boot_policy *policy);

/************************* CrOS Image Parsing ****************************/

static int fill_info_cros(struct boot_info *bi,
			  VbSelectAndLoadKernelParams *kparams,
			  const struct boot_policy *policy)
{
	bi->kernel = kparams->kernel_buffer;
	bi->loader = (uint8_t *)bi->kernel +
		(kparams->bootloader_address - 0x100000);
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
			       VbSelectAndLoadKernelParams *kparams,
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

/************************* Bootimg Parsing *******************************/
#define BOOTIMG_MAGIC			"ANDROID!"
#define BOOTIMG_MAGIC_SIZE		8
#define BOOTIMG_NAME_SIZE		16
#define BOOTIMG_ARGS_SIZE		512
#define BOOTIMG_EXTRA_ARGS_SIZE	1024
#define BOOTIMG_ID_SIZE		8

struct bootimg_hdr {
	uint8_t magic[BOOTIMG_MAGIC_SIZE];
	uint32_t kernel_size;
	uint32_t kernel_addr;
	uint32_t ramdisk_size;
	uint32_t ramdisk_addr;
	uint32_t second_size;
	uint32_t second_addr;
	uint32_t tags_addr;
	uint32_t page_size;
	uint32_t unused[2];
	uint8_t name[BOOTIMG_NAME_SIZE];
	uint8_t cmdline[BOOTIMG_ARGS_SIZE];
	uint32_t id[BOOTIMG_ID_SIZE];
	uint8_t extra_cmdline[BOOTIMG_EXTRA_ARGS_SIZE];
};

void *bootimg_get_kernel_ptr(void *img, size_t image_size)
{
	struct bootimg_hdr *hdr = img;
	void *kernel;

	if (image_size < sizeof(struct bootimg_hdr)) {
		printf("Bootimg: Header size error!\n");
		return NULL;
	}

	if (memcmp(hdr->magic, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SIZE)) {
		printf("Bootimg: BOOTIMG_MAGIC mismatch!\n");
		return NULL;
	}

	if (image_size <= hdr->page_size) {
		printf("Bootimg: Header page size error!\n");
		return NULL;
	}

	kernel = (void *)((uintptr_t)hdr + hdr->page_size);

	return kernel;
}

static int fill_info_bootimg(struct boot_info *bi,
			     VbSelectAndLoadKernelParams *kparams,
			     const struct boot_policy *policy)
{
	struct bootimg_hdr *hdr = kparams->kernel_buffer;
	uintptr_t kernel;
	uintptr_t ramdisk_addr;
	uint32_t kernel_size;

	bi->kernel = bootimg_get_kernel_ptr(hdr, kparams->kernel_buffer_size);

	if (bi->kernel == NULL)
		return -1;

	kernel = (uintptr_t)bi->kernel;

	if (policy->cmd_line_loc == CMD_LINE_BOOTIMG_HDR)
		bi->cmd_line = (char *)hdr->cmdline;
	else
		bi->cmd_line = NULL;

	/*
	 * kernel_size should be aligned up to page_size since ramdisk always
	 * starts in a new page. The align up operation is performed on
	 * kernel_size rather than (kernel + hdr->kernel_size) because hdr might
	 * not be on an aligned boundary itself and thus aligning (kernel +
	 * hdr->kernel_size) can erroneously make ramdisk_addr point to an
	 * address beyond the ramdisk start.
	 * Aligning kernel_size to hdr->page_size gives us the size of kernel in
	 * multiple of pages.
	 */
	kernel_size = ALIGN_UP(hdr->kernel_size, hdr->page_size);

	if (kernel_size < kparams->kernel_buffer_size) {
		ramdisk_addr = kernel + kernel_size;

		bi->ramdisk_size = hdr->ramdisk_size;
		bi->ramdisk_addr = (void *)ramdisk_addr;
		printf("Bootimg: Ramdisk is present\n");
	} else {
		bi->ramdisk_size = 0;
		bi->ramdisk_addr = NULL;
		printf("Bootimg: No ramdisk present\n");
	}

	return 0;
}

/************************* Android GKI *******************************/
#define ANDROID_GKI_BOOT_HDR_SIZE 4096

static int gki_setup_ramdisk(struct boot_info *bi,
			     VbSelectAndLoadKernelParams *kparams,
			     int fill_cmdline)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	struct boot_img_hdr_v4 *init_hdr;
	uint8_t *init_boot_ramdisk_src;
	uint8_t *init_boot_ramdisk_dst;
	uint32_t vendor_ramdisk_section_offset;
	uint32_t bootconfig_section_offset;
	uintptr_t bootc_ramdisk_addr;

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

	if (init_hdr->kernel_size != 0) {
		printf("GKI: Kernel size on init_boot partition has to be zero\n");
		return -1;
	}

	/* Calculate address offset of vendor_ramdisk section on vendor_boot partition */
	vendor_ramdisk_section_offset = ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4),
						 vendor_hdr->page_size);

	if (CONFIG(BOOTCONFIG) && vendor_hdr->bootconfig_size != 0) {
		/* Calculate offset of bootconfig section */
		bootconfig_section_offset = vendor_ramdisk_section_offset +
			ALIGN_UP(vendor_hdr->vendor_ramdisk_size,
				 vendor_hdr->page_size) +
			ALIGN_UP(vendor_hdr->dtb_size,
				 vendor_hdr->page_size) +
			ALIGN_UP(vendor_hdr->vendor_ramdisk_table_size,
				 vendor_hdr->page_size);

		bootc_ramdisk_addr = (uintptr_t)vendor_hdr +
				     vendor_ramdisk_section_offset +
				     vendor_hdr->vendor_ramdisk_size +
				     init_hdr->ramdisk_size;

		if ((bootc_ramdisk_addr + vendor_hdr->bootconfig_size) >=
		    ((uintptr_t)kparams->kernel_buffer + kparams->vboot_cmdline_offset)) {
			printf("GKI: Not enough space for bootconfig\n");
			return -1;
		}
		/* Generate valid (that is including trailer) bootconfig section
		 * at the end of a ramdisk. Keep track of its size which is
		 * necessary in case of updating it later on.
		 */
		vendor_hdr->bootconfig_size =
			parse_build_time_bootconfig((void *)bootc_ramdisk_addr,
				(uint8_t *)vendor_hdr + bootconfig_section_offset,
				vendor_hdr->bootconfig_size);
		if (vendor_hdr->bootconfig_size < 0) {
			printf("GKI: Cannot parse build time bootconfig\n");
			return -1;
		}

		vendor_hdr->bootconfig_size = bootconfig_append_cmdline(
		    (char *)kparams->kernel_buffer +
			kparams->vboot_cmdline_offset,
		    (void *)bootc_ramdisk_addr,
		    vendor_hdr->bootconfig_size);
		if (vendor_hdr->bootconfig_size < 0) {
			printf("GKI: Cannot copy avb cmdline to bootconfig\n");
			return -1;
		}
	}

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
	bi->ramdisk_size = vendor_hdr->vendor_ramdisk_size + init_hdr->ramdisk_size +
			   vendor_hdr->bootconfig_size;

	if (fill_cmdline)
		bi->cmd_line = (char *)vendor_hdr->cmdline;

	return 0;
}

static int fill_info_gki(struct boot_info *bi,
			 VbSelectAndLoadKernelParams *kparams,
			 const struct boot_policy *policy)
{
	if (kparams->kernel_buffer == NULL) {
		printf("Pointer to kernel buffer is not initialized\n");
		return -1;
	}

	/* Kernel starts at the beginning of kernel buffer */
	bi->kernel = kparams->kernel_buffer;

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
	[KERNEL_IMAGE_BOOTIMG] = {CMD_LINE_SIGNER | CMD_LINE_BOOTIMG_HDR |
				  CMD_LINE_DTB, fill_info_bootimg},
	[KERNEL_IMAGE_ANDROID_GKI] = {CMD_LINE_BOOTIMG_HDR,
				      fill_info_gki},
#if CONFIG(KERNEL_MULTIBOOT)
	[KERNEL_IMAGE_MULTIBOOT] = {CMD_LINE_SIGNER, fill_info_multiboot},
#endif
};

/* Returns 0 if img type allows given command line location, else -1 */
static int sanity_check_policy(kernel_img_type_t type,
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
		if (sanity_check_policy(policy[i].img_type,
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
int fill_boot_info(struct boot_info *bi, VbSelectAndLoadKernelParams *kparams)
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
