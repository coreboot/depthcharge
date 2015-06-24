/*
 * Copyright 2013 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "arch/arm/boot.h"
#include "base/device_tree.h"
#include "boot/commandline.h"
#include "boot/fit.h"
#include "config.h"
#include "vboot/boot.h"

#define CMD_LINE_SIZE	4096

static void update_cmdline(struct boot_info *bi, DeviceTree *tree)
{
	const char *path[] = {"chosen", NULL};
	DeviceTreeNode *node = dt_find_node(tree->root, path, NULL, NULL, 0);

	if (node == NULL)
		goto fail;

	const char *str = dt_find_string_prop(node, "bootargs");

	if (str == NULL)
		goto fail;

	static char cmd_line_buf[2 * CMD_LINE_SIZE];
	struct commandline_info info;
	memset(&info, 0, sizeof(info));

	if (commandline_subst(str, cmd_line_buf, sizeof(cmd_line_buf), &info))
		goto fail;

	printf("Adding cmdline : %s\n", cmd_line_buf);

	dt_add_string_prop(node, "bootargs", cmd_line_buf);

	return;

fail:
	printf("WARNING! No cmd line passed to kernel\n");
}

int boot(struct boot_info *bi)
{
	DeviceTree *tree;
	void *kernel;
	uint32_t kernel_size;

	if (fit_load(bi->kernel, bi->cmd_line, &kernel, &kernel_size, &tree))
		return 1;

	if (!tree) {
		printf("A device tree is required to boot on ARM.\n");
		return 1;
	}

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

	if (dt_apply_fixups(tree))
		return 1;

	// Allocate a spot for the FDT in memory.
	void *fdt = (void *)(uintptr_t)CONFIG_KERNEL_FIT_FDT_ADDR;
	uint32_t size = dt_flat_size(tree);

	// Reserve the spot the device tree will go.
	DeviceTreeReserveMapEntry *entry = xzalloc(sizeof(*entry));
	entry->start = (uintptr_t)fdt;
	entry->size = size;
	list_insert_after(&entry->list_node, &tree->reserve_map);

	// Flatten it.
	dt_flatten(tree, fdt);

	return boot_arm_linux(CONFIG_ARCH_ARM_MACHINE_TYPE, fdt, kernel,
			      kernel_size);
}
