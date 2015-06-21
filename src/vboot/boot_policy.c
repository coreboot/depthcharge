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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "vboot/boot.h"
#include "vboot/boot_policy.h"

/*
 * Boot policy settings. By default, we select CrOS image type and command line
 * as the one filled by the signer. This is important to maintain consistency
 * with current boards.
 */
static const struct boot_policy boot_policy = {
	.img_type = KERNEL_IMAGE_CROS,
	.cmd_line_loc = CMD_LINE_SIGNER,
};

static struct {
	size_t count;
	const struct boot_policy *policy;
} curr_policy = {
	.count = 1,
	.policy = &boot_policy,
};

typedef int (*fill_bootinfo_fnptr)(struct boot_info *bi,
				   VbSelectAndLoadKernelParams *kparams);

/************************* CrOS Image Parsing ****************************/

static int fill_info_cros(struct boot_info *bi,
			  VbSelectAndLoadKernelParams *kparams)
{
	bi->kernel = kparams->kernel_buffer;
	bi->loader = (uint8_t *)bi->kernel +
		(kparams->bootloader_address - 0x100000);
	bi->params = (uint8_t *)bi->loader - CrosParamSize;

	if (boot_policy.cmd_line_loc == CMD_LINE_SIGNER)
		bi->cmd_line = (char *)bi->params - CmdLineSize;
	else
		bi->cmd_line = NULL;

	return 0;
}

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
			     VbSelectAndLoadKernelParams *kparams)
{
	struct bootimg_hdr *hdr = kparams->kernel_buffer;
	uintptr_t kernel;
	uintptr_t ramdisk_addr;
	uint32_t kernel_size;

	bi->kernel = bootimg_get_kernel_ptr(hdr, kparams->kernel_buffer_size);

	if (bi->kernel == NULL)
		return -1;

	kernel = (uintptr_t)bi->kernel;

	if (boot_policy.cmd_line_loc == CMD_LINE_BOOTIMG_HDR)
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

	uint32_t type = GET_KERNEL_IMG_TYPE(kparams->flags);

	for (i = 0; i < curr_policy.count; i++) {
		if (type == curr_policy.policy[i].img_type)
			break;
	}

	if (i == curr_policy.count) {
		printf("Boot policy: Invalid image type %x!\n", type);
		return -1;
	}

	printf("Boot policy: Match for type %x\n", type);

	return img_type_info[type].fn(bi, kparams);
}
