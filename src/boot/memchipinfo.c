/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <commonlib/list.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/device_tree.h"

static int lpddr_type_num(enum mem_chip_type type)
{
	switch (type) {
	case MEM_CHIP_LPDDR3:
		return 3;
	case MEM_CHIP_LPDDR4:
	case MEM_CHIP_LPDDR4X:
		return 4;
	default:
		return 0;
	}
}

static DeviceTreeNode *new_channel(DeviceTree *tree,
				   struct mem_chip_entry *entry)
{
	char name[16];
	snprintf(name, sizeof(name), "lpddr-channel%d", entry->channel);

	const char *path[] = { name, NULL };
	DeviceTreeNode *channel = dt_find_node(tree->root, path, NULL, NULL, 1);

	char *compat = xzalloc(24);
	snprintf(compat, 24, "jedec,lpddr%d-channel",
		 lpddr_type_num(entry->type));
	dt_add_string_prop(channel, "compatible", compat);
	dt_add_u32_prop(channel, "io-width", entry->channel_io_width);
	dt_add_u32_prop(channel, "#address-cells", 1);
	dt_add_u32_prop(channel, "#size-cells", 0);

	return channel;
}

static void new_rank(DeviceTreeNode *channel, struct mem_chip_entry *entry)
{
	char name[8];
	snprintf(name, sizeof(name), "rank@%d", entry->rank);

	const char *path[] = { name, NULL };
	DeviceTreeNode *rank = dt_find_node(channel, path, NULL, NULL, 1);

	int lpddrnum = lpddr_type_num(entry->type);
	printf("LPDDR%d chan%d(x%d) rank%d: density %umbits x%d, MF %02x rev %02x%02x\n",
		lpddrnum, entry->channel, entry->channel_io_width, entry->rank,
		entry->density_mbits, entry->io_width, entry->manufacturer_id,
		entry->revision_id[0], entry->revision_id[1]);

	char *compat = xzalloc(32);
	snprintf(compat, 32, "lpddr%d-%02x,%02x%02x$jedec,lpddr%d", lpddrnum,
		 entry->manufacturer_id, entry->revision_id[0],
		 entry->revision_id[1], lpddrnum);
	dt_add_string_prop(rank, "compatible", compat);
	/* We manipulate the string *after* adding it to the DT here, which
	   works because dt_add_string_prop() hooks the raw pointer into the
	   tree instead of making a deep copy. */
	char *dollar = strchr(compat, '$');
	*dollar = '\0';

	dt_add_u32_prop(rank, "reg", entry->rank);
	dt_add_u32_prop(rank, "density", entry->density_mbits);
	dt_add_u32_prop(rank, "io-width", entry->io_width);

	uint8_t *revision = xzalloc(8);
	revision[3] = entry->revision_id[0];
	revision[7] = entry->revision_id[1];
	dt_add_bin_prop(rank, "revision-id", revision, 8);
}

static int install_memchipinfo_data(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	struct mem_chip_info *info = (void *)lib_sysinfo.mem_chip_base;
	DeviceTreeNode *channel[8] = {0};

	if (info->struct_version != MEM_CHIP_STRUCT_VERSION)
		return 1;

	for (int i = 0; i < info->num_entries; i++) {
		if (!lpddr_type_num(info->entries[i].type)) {
			printf("ERROR:%s: Unknown memory type %d\n",
			       __func__, info->entries[i].type);
			continue;
		}

		int c = info->entries[i].channel;
		if (c >= ARRAY_SIZE(channel)) {
			printf("ERROR:%s: Too high channel number %d\n",
			       __func__, c);
			continue;
		}

		if (!channel[c])
			channel[c] = new_channel(tree, &info->entries[i]);

		new_rank(channel[c], &info->entries[i]);
	}

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
