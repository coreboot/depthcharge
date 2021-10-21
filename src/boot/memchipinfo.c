/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <libpayload.h>
#include "base/init_funcs.h"
#include "base/device_tree.h"
#include "base/list.h"

static int install_memchipinfo_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	struct mem_chip_info *mem_chip_ptr =
				(struct mem_chip_info *)lib_sysinfo.mem_chip_base;
	uint8_t *revision = xzalloc(8);
	uint64_t temp_density = 0;
	int i;

	const char *memchip_path[] = { "lpddr3", NULL };
        DeviceTreeNode *memchip_node = dt_find_node(tree->root,
					memchip_path, NULL, NULL, 1);

	/* Add a compatible property. */
	dt_add_string_prop(memchip_node, "compatible", "jedec,lpddr3");

	/* Add properties. */
	for (i=0; i<mem_chip_ptr->num_channels; i++)
		temp_density += mem_chip_ptr->channel[i].density;

	/* convert density from bytes to megabits */
	temp_density /= 1024 * 1024 / 8;
	dt_add_u32_prop(memchip_node, "density", temp_density);
	dt_add_u32_prop(memchip_node, "io_width",
			mem_chip_ptr->channel[0].io_width);
	dt_add_u32_prop(memchip_node, "manufacturer_id",
			mem_chip_ptr->channel[0].manufacturer_id);

	revision[3] = mem_chip_ptr->channel[0].revision_id[0];
	revision[7] = mem_chip_ptr->channel[0].revision_id[1];
	dt_add_bin_prop(memchip_node, "revision_id", revision, 8);

	return 0;
}

static DeviceTreeFixup memchipinfo_data = {
        .fixup = &install_memchipinfo_data
};

static int memchipinfo_init(void)
{
	if (lib_sysinfo.mem_chip_base)
		list_insert_after(&memchipinfo_data.list_node,
				&device_tree_fixups);
	return 0;
}

INIT_FUNC(memchipinfo_init);
