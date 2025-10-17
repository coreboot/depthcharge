/*
 * Copyright 2013 Google LLC
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "arch/arm/boot.h"
#include "base/cleanup_funcs.h"
#include "base/device_tree.h"
#include "boot/android_dtboimg.h"
#include "boot/commandline.h"
#include "boot/dt_update.h"
#include "boot/fit.h"
#include "drivers/storage/blockdev.h"
#include "image/symbols.h"
#include "vboot/boot.h"

#define CMD_LINE_SIZE	4096
#define KERNEL_IMG_NAME	"android_kernel"

static void update_cmdline(struct boot_info *bi, struct device_tree *tree)
{
	const char *path[] = {"chosen", NULL};
	struct device_tree_node *node =
		dt_find_node(tree->root, path, NULL, NULL, 0);

	if (node == NULL)
		goto fail;

	const char *str = dt_find_string_prop(node, "bootargs");

	if (str == NULL)
		goto fail;

	static char cmd_line_buf[2 * CMD_LINE_SIZE];
	BlockDev *bdev = (BlockDev *)bi->kparams->disk_handle;

	struct commandline_info info = {
		.devnum = 0,
		.partnum = bi->kparams->partition_number + 1,
		.guid = bi->kparams->partition_guid,
		.external_gpt = bdev->external_gpt,
	};

	if (commandline_subst(str, cmd_line_buf, sizeof(cmd_line_buf), &info))
		goto fail;

	dt_add_string_prop(node, "bootargs", cmd_line_buf);

	return;

fail:
	printf("WARNING! No cmd line passed to kernel\n");
}

static FitImageNode *construct_kernel_fit_image_node(void *kernel, size_t kernel_size)
{
	FitImageNode *image;

	image = xzalloc(sizeof(*image));
	image->name = KERNEL_IMG_NAME;
	image->data = kernel;
	image->size = kernel_size;
	image->compression = CompressionLz4;
	return image;
}

int boot(struct boot_info *bi)
{
	struct device_tree *tree = NULL;
	FitImageNode *kernel = fit_load(bi->kernel, &tree);

	/* If kernel is not FIT format and separate partition exists for DTB/DTBO, then
	   extract the kernel and devicetree from their respective images. */
	if ((!kernel || !tree) && bi->dt) {
		tree = bi->dt;
		kernel = construct_kernel_fit_image_node(bi->kernel, bi->kernel_size);
	}

	if (!kernel || !tree)
		return 1;

	dt_update_chosen(tree, bi->cmd_line);
	dt_update_memory(tree);

	/*
	 * On ARM, there are two different types of images that can be used for
	 * storing the kernel on disk:
	 * 1. default image type (FIT)
	 * 2. fastboot bootimg type
	 * In case of bootimg type kernels, there are several options of passing
	 * in command line parameters for the kernel. It could be present either
	 * in the signed image, or it could be present in the bootimg header or
	 * it could be passed through the DTB.
	 *
	 * If until this point, we still have bi->cmd_line == NULL, it means
	 * that we need to go and check if it is present in the DTB. If yes,
	 * then command line substitution needs to be performed and then we need
	 * to add it back to the DTB.
	 *
	 * This step cannot be performed before because we get pointer to the
	 * device tree after the call to fit_load. Also, fit_load does not make
	 * any changes to command line in dtb if bi->cmd_line is NULL.
	 */
	if (bi->cmd_line == NULL)
		update_cmdline(bi, tree);

	if (bi->ramdisk_addr && bi->ramdisk_size)
		fit_add_ramdisk(tree, bi->ramdisk_addr, bi->ramdisk_size);

	/* Add DT node if pvmfw was loaded to memory */
	if (CONFIG(ANDROID_PVMFW) && bi->pvmfw_addr && bi->pvmfw_size &&
	    fit_add_pvmfw(tree, bi->pvmfw_addr, bi->pvmfw_size) != 0) {
		/*
		 * Failed to add pvmfw node, clear the pvmfw with secrets from
		 * memory.
		 */
		printf("ERROR: Failed to add pvmfw reserved mem node!\n");
		memset(bi->pvmfw_addr, 0, bi->pvmfw_size);
		return 1;
	}

	if (dt_apply_fixups(tree))
		return 1;

	// Allocate a spot for the FDT in memory.
	void *fdt = &_fit_fdt_start;
	uint32_t size = dt_flat_size(tree);
	if (_fit_fdt_start + size > _fit_fdt_end) {
		printf("ERROR: FDT image overflows buffer!\n");
		return 1;
	}

	// Reserve the spot the device tree will go.
	struct device_tree_reserve_map_entry *entry = xzalloc(sizeof(*entry));
	entry->start = (uintptr_t)fdt;
	entry->size = size;
	list_insert_after(&entry->list_node, &tree->reserve_map);

	// Flatten it.
	dt_flatten(tree, fdt);

	run_cleanup_funcs(CleanupOnHandoff);

	return boot_arm_linux(bi, fdt, kernel);
}
