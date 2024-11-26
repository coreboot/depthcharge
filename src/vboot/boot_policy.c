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
#include <gpt.h>
#include <libpayload.h>
#include <stdint.h>
#include <stdlib.h>
#include <tss_constants.h>
#include <vb2_android_bootimg.h>

#include "base/gpt.h"
#include "base/string_utils.h"
#include "boot/android_bootconfig_params.h"
#include "boot/android_pvmfw.h"
#include "boot/bootconfig.h"
#include "boot/commandline.h"
#include "boot/multiboot.h"
#include "drivers/storage/blockdev.h"
#include "vboot/boot.h"
#include "vboot/boot_policy.h"
#include "vboot/secdata_tpm.h"

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
				   struct vb2_kernel_params *kparams,
				   const struct boot_policy *policy);

/************************* CrOS Image Parsing ****************************/

static int fill_info_cros(struct boot_info *bi,
			  struct vb2_kernel_params *kparams,
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
			     struct vb2_kernel_params *kparams,
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
#define ANDROID_BDEV_KEY_STR "androidboot.boot_devices"
#define ANDROID_BOOT_PART_UUID_KEY_STR "androidboot.boot_part_uuid"
#define ANDROID_BOOT_A_PART_NUM 13
#define ANDROID_BOOT_B_PART_NUM 14
#define ANDROID_SLOT_SUFFIX_KEY_STR "androidboot.slot_suffix"
#define ANDROID_FORCE_NORMAL_BOOT_KEY_STR "androidboot.force_normal_boot"

/*
 * Fill struct boot_info with pvmfw information and fill pvmfw config.
 */
static void setup_pvmfw(struct boot_info *bi, struct vb2_kernel_params *kparams)
{
	int ret;
	uint32_t status;
	size_t pvmfw_size, params_size;
	void *pvmfw_addr, *params = NULL;
	struct boot_img_hdr_v4 *boot_hdr = kparams->pvmfw_buffer;

	if (!kparams->pvmfw_buffer || kparams->pvmfw_size == 0) {
		/* There is no pvmfw so don't do anything */
		printf("PVMFW was not loaded, ignoring...\n");
		return;
	}

	if (kparams->pvmfw_size < ANDROID_GKI_BOOT_HDR_SIZE) {
		/* If loaded pvmfw is smaller then boot header, then fail */
		printf("PVMFW size is too small\n");
		ret = -1;
		goto fail;
	}

	/* Verify that boot header of pvmfw partition is valid */
	if (memcmp(boot_hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE) ||
	    boot_hdr->header_version < 3 || boot_hdr->ramdisk_size) {
		printf("pvmfw's boot hdr is invalid\n");
		ret = -1;
		goto fail;
	}

	/* Get pvmfw code size */
	pvmfw_size = boot_hdr->kernel_size;

	/* Get pvmfw boot params from GSC */
	status = secdata_get_pvmfw_params(&params, &params_size);
	if (status != TPM_SUCCESS) {
		printf("Failed to get pvmfw gsc boot params data. "
		       "secdata_get_pvmfw_params returned %u\n", status);
		ret = -1;
		goto fail;
	}

	/* pvmfw code starts after the boot header. Discard the boot header */
	pvmfw_addr = kparams->pvmfw_buffer + ANDROID_GKI_BOOT_HDR_SIZE;

	/* Verify that pvmfw start address is aligned */
	if (!IS_ALIGNED((uintptr_t)pvmfw_addr, ANDROID_PVMFW_CFG_ALIGN)) {
		printf("Failed to setup pvmfw at aligned address\n");
		ret = -1;
		goto fail;
	}

	ret = setup_android_pvmfw(pvmfw_addr,
				  kparams->pvmfw_buffer_size - ANDROID_GKI_BOOT_HDR_SIZE,
				  &pvmfw_size, params, params_size);
	if (ret != 0) {
		printf("Failed to setup pvmfw\n");
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
		 *
		 * Use memmap instead of e820, because adding a region to e820
		 * would require modifying lib_sysinfo.memrange, which seems
		 * like a hacky approach and might yield side effects.
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
fail:
	/* TODO(b/380002393): Clear the remains before jumping to kernel */
	if (params) {
		/* Make sure that secrets are no longer in memory */
		memset(params, 0, params_size);
		free(params);
	}

	/* If failed then clear the buffer */
	if (ret != 0)
		memset(kparams->pvmfw_buffer, 0, kparams->pvmfw_buffer_size);
}

/*
 * Update cmdline with proper slot_suffix parameter
 */
static int modify_android_slot_suffix(struct vb2_kernel_params *kparams,
				      uintptr_t bootc_ramdisk_addr,
				      size_t bootc_buffer_size)
{
	char *str_to_insert;
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	uint32_t partition_number = kparams->partition_number;
	int bootconfig_size;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);

	/* Validate partition number according to supported layout at
	 * al-internal/platform/vendor/google_devices/houdini/+/tm-al:layout/disk_layout.json */
	if (partition_number == ANDROID_BOOT_A_PART_NUM) {
		str_to_insert = GPT_ENT_NAME_ANDROID_A_SUFFIX;
	} else if (partition_number == ANDROID_BOOT_B_PART_NUM) {
		str_to_insert = GPT_ENT_NAME_ANDROID_B_SUFFIX;
	} else {
		 /* Exit early if the partition_number is invalid */
		printf("Unsupported partition number to boot GKI: %d\n",
		       partition_number);
		return -1;
	}

	bootconfig_size = append_bootconfig_params(ANDROID_SLOT_SUFFIX_KEY_STR,
						  str_to_insert,
						  (void *)bootc_ramdisk_addr,
						  vendor_hdr->bootconfig_size,
						  bootc_buffer_size);
	if (bootconfig_size < 0) {
		printf("Cannot update boot slot for android GKI!\n");
		return -1;
	} else
		vendor_hdr->bootconfig_size = bootconfig_size;

	return 0;
}

/*
 * Update bootconfig with proper boot_devices parameter
 */
static int modify_android_boot_devices(struct vb2_kernel_params *kparams,
				       uintptr_t bootc_ramdisk_addr,
				       size_t bootc_buffer_size)
{
	char *str_to_insert;
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	BlockDev *bdev = (BlockDev *)kparams->disk_handle;
	int bootconfig_size;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);

	str_to_insert = bdev->removable ?
			CONFIG_ANDROID_BOOT_DEVICES_REMOVABLE :
			CONFIG_ANDROID_BOOT_DEVICES_DISK;

	/* Exit early if platform does not set boot_devices, nothing to do */
	if (!strlen(str_to_insert))
		return 0;

	bootconfig_size = append_bootconfig_params(ANDROID_BDEV_KEY_STR,
						  str_to_insert,
						  (void *)bootc_ramdisk_addr,
						  vendor_hdr->bootconfig_size,
						  bootc_buffer_size);
	if (bootconfig_size < 0) {
		printf("Cannot modify boot device for android GKI!\n");
		return -1;
	} else
		vendor_hdr->bootconfig_size = bootconfig_size;

	return 0;
}

/*
 * Update bootconfig with proper force_normal_boot parameter
 */
static int modify_android_force_normal_boot(struct vb2_kernel_params *kparams,
					    uintptr_t bootc_ramdisk_addr,
					    size_t bootc_buffer_size,
					    bool recovery_boot)
{
	char *str_to_insert;
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	int bootconfig_size;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);

	str_to_insert = recovery_boot ? "0" : "1";

	bootconfig_size = append_bootconfig_params(ANDROID_FORCE_NORMAL_BOOT_KEY_STR,
						  str_to_insert,
						  (void *)bootc_ramdisk_addr,
						  vendor_hdr->bootconfig_size,
						  bootc_buffer_size);
	if (bootconfig_size < 0) {
		printf("Cannot set force normal boot property for Android GKI!\n");
		return -1;
	}

	vendor_hdr->bootconfig_size = bootconfig_size;

	return 0;
}

static bool gki_is_recovery_boot(struct vb2_kernel_params *kparams)
{
	switch (kparams->boot_command) {
	case VB2_BOOT_CMD_NORMAL_BOOT:
		return false;

	case VB2_BOOT_CMD_BOOTLOADER_BOOT:
		/*
		 * TODO(b/358088653): We should enter fastboot mode and clear
		 * BCB command in misc partition. For now ignore that and boot
		 * to recovery where fastbootd should be available.
		 */
		return true;

	case VB2_BOOT_CMD_RECOVERY_BOOT:
		return true;

	default:
		printf("Unknown boot command, assume recovery boot is required\n");
		return true;
	}
}

static int add_android_boot_part_uuid(struct vb2_kernel_params *kparams,
				      uintptr_t bootc_ramdisk_addr,
				      size_t bootc_buffer_size)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	int bootconfig_size;
	char guid_str[GUID_STRLEN];

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);

	guid_to_string(kparams->partition_guid, guid_str, ARRAY_SIZE(guid_str));
	bootconfig_size = append_bootconfig_params(ANDROID_BOOT_PART_UUID_KEY_STR,
						  guid_str,
						  (void *)bootc_ramdisk_addr,
						  vendor_hdr->bootconfig_size,
						  bootc_buffer_size);
	if (bootconfig_size < 0) {
		printf("Cannot modify boot device for android GKI!\n");
		return -1;
	}

	vendor_hdr->bootconfig_size = bootconfig_size;

	return 0;
}

static bool gki_ramdisk_fragment_needed(struct vendor_ramdisk_table_entry_v4 *fragment,
					bool recovery_boot)
{
	/* Ignore all other properties except ramdisk type */
	switch (fragment->ramdisk_type) {
	case VENDOR_RAMDISK_TYPE_PLATFORM:
	case VENDOR_RAMDISK_TYPE_DLKM:
		return true;

	case VENDOR_RAMDISK_TYPE_RECOVERY:
		return recovery_boot;

	default:
		printf("Unknown ramdisk type 0x%x\n", fragment->ramdisk_type);

		return false;
	}
}

static int gki_setup_ramdisk(struct boot_info *bi,
			     struct vb2_kernel_params *kparams,
			     int fill_cmdline)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	struct boot_img_hdr_v4 *init_hdr;
	uint8_t *init_boot_ramdisk_src;
	uint8_t *vendor_ramdisk;
	uint8_t *vendor_ramdisk_end;
	uint32_t vendor_ramdisk_table_section_offset;
	uint32_t vendor_ramdisk_section_offset;
	uint32_t bootconfig_section_offset;
	uintptr_t bootc_ramdisk_addr;
	bool recovery_boot;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
						       kparams->vendor_boot_offset);
	init_hdr = (struct boot_img_hdr_v4 *)((uintptr_t)kparams->kernel_buffer +
					      kparams->init_boot_offset);

	if (init_hdr->kernel_size != 0) {
		printf("GKI: Kernel size on init_boot partition has to be zero\n");
		return -1;
	}

	/* Calculate address offset of vendor_ramdisk section on vendor_boot partition */
	vendor_ramdisk_section_offset = ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4),
						 vendor_hdr->page_size);
	vendor_ramdisk_table_section_offset = vendor_ramdisk_section_offset +
		ALIGN_UP(vendor_hdr->vendor_ramdisk_size, vendor_hdr->page_size) +
		ALIGN_UP(vendor_hdr->dtb_size, vendor_hdr->page_size);

	/* Check if vendor ramdisk table is correct */
	if (vendor_hdr->vendor_ramdisk_table_size <
	    vendor_hdr->vendor_ramdisk_table_entry_num *
	    vendor_hdr->vendor_ramdisk_table_entry_size) {
		printf("GKI: Too small vendor ramdisk table\n");
		return -1;
	}

	recovery_boot = gki_is_recovery_boot(kparams);

	vendor_ramdisk = (uint8_t *)vendor_hdr + vendor_ramdisk_section_offset;
	vendor_ramdisk_end = vendor_ramdisk;

	/* Go through all ramdisk fragments and keep only the required ones */
	for (uintptr_t i = 0,
	     fragment_ptr = (uintptr_t)vendor_hdr + vendor_ramdisk_table_section_offset;
	     i < vendor_hdr->vendor_ramdisk_table_entry_num;
	     fragment_ptr += vendor_hdr->vendor_ramdisk_table_entry_size, i++) {

		struct vendor_ramdisk_table_entry_v4 *fragment;
		uint8_t *fragment_src;

		fragment = (struct vendor_ramdisk_table_entry_v4 *)fragment_ptr;
		if (!gki_ramdisk_fragment_needed(fragment, recovery_boot))
			/* Fragment not needed, skip it */
			continue;

		fragment_src = vendor_ramdisk + fragment->ramdisk_offset;
		if (vendor_ramdisk_end != fragment_src)
			/*
			 * A fragment was skipped before, we need to move current one
			 * at the correct place.
			 */
			memmove(vendor_ramdisk_end, fragment_src, fragment->ramdisk_size);

		/* Update location of the end of vendor ramdisk */
		vendor_ramdisk_end += fragment->ramdisk_size;
	}

	if (CONFIG(BOOTCONFIG)) {
		/* Calculate offset of bootconfig section */
		bootconfig_section_offset = vendor_ramdisk_table_section_offset +
			ALIGN_UP(vendor_hdr->vendor_ramdisk_table_size,
				 vendor_hdr->page_size);

		/* Put bootconfig right after ramdisks */
		bootc_ramdisk_addr = (uintptr_t)vendor_ramdisk_end + init_hdr->ramdisk_size;

		if ((bootc_ramdisk_addr + vendor_hdr->bootconfig_size) >=
		    ((uintptr_t)kparams->kernel_buffer + kparams->kernel_buffer_size)) {
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
		    kparams->kernel_cmdline_buffer,
		    (void *)bootc_ramdisk_addr,
		    vendor_hdr->bootconfig_size);
		if (vendor_hdr->bootconfig_size < 0) {
			printf("GKI: Cannot copy avb cmdline to bootconfig\n");
			return -1;
		}

		if (append_android_bootconfig_params(kparams, (void *)bootc_ramdisk_addr) < 0)
			/*
			 * On error, just log a message and continue with the rest of the
			 * bootflow. The  idea is to get as many run-time bootconfig params
			 * filled up as possible without halting the bootflow and let the OS
			 * decide rather than FW blocking the boot to OS.
			 */
			printf("GKI: Cannot append all android bootconfig params\n");

		/* Update boot device */
		if (modify_android_boot_devices(kparams,
						(uintptr_t)bootc_ramdisk_addr,
						(size_t)(kparams->kernel_buffer +
						kparams->kernel_buffer_size -
						bootc_ramdisk_addr)))
			return -1;


		/* Update slot suffix */
		if (modify_android_slot_suffix(kparams,
					       bootc_ramdisk_addr,
					       (size_t)(kparams->kernel_buffer +
					       kparams->kernel_buffer_size -
					       bootc_ramdisk_addr)) < 0)
			return -1;

		/* Select boot mode */
		if (modify_android_force_normal_boot(kparams,
						     bootc_ramdisk_addr,
						     (size_t)(kparams->kernel_buffer +
							      kparams->kernel_buffer_size -
							      bootc_ramdisk_addr),
						     recovery_boot) < 0)
			return -1;

		if (add_android_boot_part_uuid(kparams,
					       bootc_ramdisk_addr,
					       (size_t)(kparams->kernel_buffer +
							kparams->kernel_buffer_size -
							bootc_ramdisk_addr)) < 0)
			return -1;

		/* Update bootconfig offset within ramdisk */
		bi->ramdisk_bootconfig_offset = bootc_ramdisk_addr - (uintptr_t)vendor_ramdisk;
	}

	/* On init_boot there's no kernel, so ramdisk follows the header */
	init_boot_ramdisk_src = (uint8_t *)init_hdr + ANDROID_GKI_BOOT_HDR_SIZE;

	/* Move init_boot ramdisk to directly follow the vendor_boot ramdisk.
	 * This is a requirement from Android system. The cpio/gzip/lz4
	 * compression formats support this type of concatenation. After
	 * the kernel decompresses, it extracts contatenated file into
	 * an initramfs, which results in a file structure that's a generic
	 * ramdisk (from init_boot) overlaid on the vendor ramdisk (from
	 * vendor_boot) file structure. */
	memmove(vendor_ramdisk_end, init_boot_ramdisk_src, init_hdr->ramdisk_size);

	/* Update ramdisk addr and size */
	bi->ramdisk_addr = vendor_ramdisk;
	bi->ramdisk_size = (uintptr_t)(vendor_ramdisk_end - vendor_ramdisk) +
			   init_hdr->ramdisk_size + vendor_hdr->bootconfig_size;

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

	if (CONFIG(ANDROID_PVMFW))
		setup_pvmfw(bi, kparams);

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
