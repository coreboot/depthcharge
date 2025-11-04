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

static FitImageNode *construct_kernel_fit_image_node(void *kernel, size_t kernel_size)
{
	FitImageNode *image;

	image = xzalloc(sizeof(*image));
	image->name = KERNEL_IMG_NAME;
	image->data = kernel;
	image->size = kernel_size;
	/* TODO(b/457429470): Switch to LZ4 compression once it is supported */
	image->compression = CompressionNone;
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

	/* Add DT node if pKVM DRNG seed was prepared to memory */
	if (CONFIG(ANDROID_PKVM_DRNG_SEED) && bi->pvmfw_addr && bi->pvmfw_buffer_size &&
	    dt_add_pkvm_drng(tree, bi) != 0) {
		/* Failed to add pKVM DRNG seed node */
		printf("ERROR: Failed to add pKVM DRNG seed reserved mem node!\n");
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
