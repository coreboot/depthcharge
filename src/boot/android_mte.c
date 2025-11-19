// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "boot/android_mte.h"
#include "base/device_tree.h"

/*
 * Pass tag address and tag size to fix_device_tree() which reserves
 * memory for MTE.
 */
typedef struct {
	struct device_tree_fixup fixup;
	void *mte_addr;
	uint64_t mte_size;
} MteDeviceTreeFixup;

static int fix_device_tree(struct device_tree_fixup *dt_fixup, struct device_tree *tree)
{
	static const char * const reserved_mem[] = {"reserved-memory", "mte", NULL};

	struct device_tree_node *node;
	uint32_t addr_cells = 2, size_cells = 1;
	uint64_t mte_addr;
	uint64_t mte_size;

	MteDeviceTreeFixup *mte_fixup = container_of(dt_fixup, MteDeviceTreeFixup, fixup);
	if (!mte_fixup) {
		printf("Failed to get MteDeviceTreeFixup pointer\n");
		return -1;
	}

	mte_addr = (uint64_t)mte_fixup->mte_addr;
	mte_size = mte_fixup->mte_size;

	node = dt_find_node(tree->root, reserved_mem, &addr_cells, &size_cells,
			    /* create */ 1);
	if (!node) {
		printf("Failed to add reserved-memory for MTE\n");
		return -1;
	}

	dt_add_reg_prop(node, &mte_addr, &mte_size, 1, addr_cells, size_cells);
	dt_add_bin_prop(node, "no-map", NULL, 0);

	printf("device_tree added for mte, addr=%p, size=0x%llx\n",
		(void *)mte_addr, mte_size);
	return 0;
}

/*
 * Initialize MTE.
 *
 * Reserve memory in device tree using the hardcoded MTE base address
 * and size from Kconfig.
 */
void mte_initialize(void)
{
	void *mte_addr = (void *)CONFIG_ANDROID_MTE_TAG_ADDR;
	uint64_t mte_size = CONFIG_ANDROID_MTE_TAG_SIZE;

	if (!mte_addr || !mte_size) {
		printf("Error: wrong MTE config, not reserving memory, addr=%p, size=0x%llx\n",
			mte_addr, mte_size);
		return;
	}

	MteDeviceTreeFixup *mte_fixup = xzalloc(sizeof(*mte_fixup));

	/* save addr/size to be used by fix_device_tree later */
	mte_fixup->fixup.fixup = fix_device_tree;
	mte_fixup->mte_addr = mte_addr;
	mte_fixup->mte_size = mte_size;

	printf("mte reserving memory, addr=%p, size=0x%llx\n", mte_addr, mte_size);

	/* setup device tree */
	list_insert_after(&mte_fixup->fixup.list_node, &device_tree_fixups);
}
