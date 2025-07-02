/* SPDX-License-Identifier: BSD-3-Clause */

#include <commonlib/list.h>
#include <libpayload.h>
#include "base/init_funcs.h"
#include "base/device_tree.h"

static int install_pscinode_data(struct device_tree_fixup *fixup,
				struct device_tree *tree)
{
	struct device_tree_node *psci = dt_find_node_by_path(tree, "/psci", NULL, NULL, 0);

	if (!psci)
		printf("PSCI not supported.\n");
	else
		list_remove(&psci->list_node);

	return 0;
}

static struct device_tree_fixup psci_data = {
	.fixup = &install_pscinode_data
};

static int pscinode_init(void)
{
	list_insert_after(&psci_data.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(pscinode_init);
