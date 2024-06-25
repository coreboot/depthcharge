/* SPDX-License-Identifier: BSD-3-Clause */

#include <commonlib/list.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "base/device_tree.h"

#define START_BIT 7
#define END_BIT 14

static int install_cpunode_data(struct device_tree_fixup *fixup,
				struct device_tree *tree)
{
	unsigned int *fuse_addr = (unsigned int *)0x78418C;
	unsigned int fuse_value = read32(fuse_addr);
	char cpu_name[8];
	const char *cpu_path[] = { "cpus", cpu_name, NULL };

	for (int i = START_BIT; i <= END_BIT; i++) {
		if ((fuse_value >> i) & 0x1) {
			snprintf(cpu_name, sizeof(cpu_name), "cpu@%u",
				 (i - START_BIT) * 100);
			struct device_tree_node *cpu_node = dt_find_node(
				tree->root, cpu_path, NULL, NULL, 0);
			if (!cpu_node) {
				printf("ERROR: Failed to find node %s\n",
				       cpu_name);
				return 1;
			}
			dt_add_string_prop(cpu_node, "status", "fail");
			printf("CPU%d disabled by fuse setting\n",
			       i - START_BIT);
		}
	}

	return 0;
}

static struct device_tree_fixup cpu_data = {
	.fixup = &install_cpunode_data
};

static int cpunode_init(void)
{
	list_insert_after(&cpu_data.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(cpunode_init);
