/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2026 Google LLC
 *
 * Disable defective Canim CPU nodes and remove their PSCI power domains.
 * The fixup runs only when soc_id identifies the SoC as Canim.
 */

#include <commonlib/list.h>
#include <libpayload.h>

#include "base/device_tree.h"
#include "base/init_funcs.h"
#include "drivers/soc/qcom_chipinfo.h"
#include "drivers/soc/x1p42100.h"

static const struct qcom_chipinfo_cpu_fuse canim_cpu_fuse = {
	.reg_addr = CANIM_QFPROM_RAW_FEAT_CFG_ROW7_LSB,
	.num_clusters = CANIM_NUM_CPU_CLUSTERS,
	.clusters = {
		[0] = {
			.shift = CANIM_CORE0_CPU_DISABLE_SHIFT,
			.mask = CANIM_CORE0_CPU_DISABLE_MASK,
		},
		[1] = {
			.shift = CANIM_CORE1_CPU_DISABLE_SHIFT,
			.mask = CANIM_CORE1_CPU_DISABLE_MASK,
		},
	},
};

static uint32_t remove_cpu_power_domains(struct device_tree *tree,
					 struct device_tree_node *cpu_node)
{
	const char *psci_path[] = { "psci", NULL };
	const void *pd_data = NULL;
	const uint32_t *pd_cells;
	struct device_tree_node *psci_node;
	size_t pd_size = 0;
	size_t pd_count;
	size_t i = 0;
	uint32_t removed = 0;

	dt_find_bin_prop(cpu_node, "power-domains", &pd_data, &pd_size);
	if (!pd_data)
		return 0;

	if (!pd_size || pd_size % sizeof(uint32_t)) {
		printf("DTB fixup: invalid power-domains on %s\n",
		       cpu_node->name);
		return 0;
	}

	psci_node = dt_find_node(tree->root, psci_path, NULL, NULL, 0);
	if (!psci_node) {
		printf("DTB fixup: /psci not found\n");
		return 0;
	}

	pd_cells = pd_data;
	pd_count = pd_size / sizeof(uint32_t);

	while (i < pd_count) {
		const void *cells_data = NULL;
		struct device_tree_node *pd_node;
		struct device_tree_node *child;
		size_t cells_size = 0;
		uint32_t phandle = be32toh(pd_cells[i]);
		uint32_t num_cells;
		bool is_psci_child = false;

		pd_node = dt_find_node_by_phandle(tree->root, phandle);
		if (!pd_node) {
			printf("DTB fixup: power-domain %#x for %s not found\n",
			       phandle, cpu_node->name);
			break;
		}

		dt_find_bin_prop(pd_node, "#power-domain-cells",
				 &cells_data, &cells_size);
		if (!cells_data || cells_size != sizeof(uint32_t)) {
			printf("DTB fixup: invalid #power-domain-cells on %s\n",
			       pd_node->name);
			break;
		}

		num_cells = be32toh(*(const uint32_t *)cells_data);
		if (num_cells > pd_count - i - 1) {
			printf("DTB fixup: truncated power-domains on %s\n",
			       cpu_node->name);
			break;
		}

		list_for_each(child, psci_node->children, list_node) {
			if (child == pd_node) {
				is_psci_child = true;
				break;
			}
		}

		if (is_psci_child) {
			list_remove(&pd_node->list_node);
			removed++;
		}

		i += 1 + num_cells;
	}

	return removed;
}

static int bluey_fixup_defective_cores(struct device_tree_fixup *fixup,
				       struct device_tree *tree)
{
	const char *cpu_map_path[] = { "cpus", "cpu-map", NULL };
	const uint32_t *cluster_array;
	struct device_tree_node *cpu_map;
	struct device_tree_node *cluster_node;
	struct device_tree_node *core_node;
	uint32_t num_clusters;
	uint32_t cluster_idx = 0;
	uint32_t disabled_count = 0;
	uint32_t removed_pd_count = 0;
	uint32_t dtb_cluster_count = 0;

	(void)fixup;

	if (lib_sysinfo.soc_id != CANIM_SOC_ID)
		return 0;

	num_clusters = canim_cpu_fuse.num_clusters;
	cluster_array =
		qcom_chipinfo_get_defective_cpu_clusters(&canim_cpu_fuse);
	if (!cluster_array) {
		printf("DTB fixup: Canim CPU fuse data unavailable\n");
		return 0;
	}

	cpu_map = dt_find_node(tree->root, cpu_map_path, NULL, NULL, 0);
	if (!cpu_map) {
		printf("DTB fixup: /cpus/cpu-map not found\n");
		return 0;
	}

	list_for_each(cluster_node, cpu_map->children, list_node)
		dtb_cluster_count++;
	if (dtb_cluster_count != num_clusters) {
		printf("DTB fixup: cluster count mismatch (DTB=%u, fuse=%u)\n",
		       dtb_cluster_count, num_clusters);
		return 0;
	}

	list_for_each(cluster_node, cpu_map->children, list_node) {
		uint32_t disabled_mask = cluster_array[cluster_idx];
		uint32_t core_idx = 0;

		list_for_each(core_node, cluster_node->children, list_node) {
			const void *phandle_data = NULL;
			struct device_tree_node *cpu_node;
			size_t phandle_size = 0;
			uint32_t cpu_phandle;

			if (core_idx >= QCOM_CHIPINFO_MAX_CPUS_PER_CLUSTER ||
			    !(disabled_mask & BIT(core_idx))) {
				core_idx++;
				continue;
			}

			dt_find_bin_prop(core_node, "cpu",
					 &phandle_data, &phandle_size);
			if (!phandle_data || phandle_size != sizeof(uint32_t)) {
				printf("DTB fixup: missing CPU phandle for %s/%s\n",
				       cluster_node->name, core_node->name);
				core_idx++;
				continue;
			}

			cpu_phandle = be32toh(*(const uint32_t *)phandle_data);
			cpu_node = dt_find_node_by_phandle(tree->root, cpu_phandle);
			if (!cpu_node) {
				printf("DTB fixup: CPU phandle %#x not found\n",
				       cpu_phandle);
				core_idx++;
				continue;
			}

			dt_add_string_prop(cpu_node, "status", "fail");
			disabled_count++;
			removed_pd_count += remove_cpu_power_domains(tree, cpu_node);
			core_idx++;
		}

		cluster_idx++;
	}

	printf("DTB fixup: disabled %u CPU(s), removed %u PSCI domain(s)\n",
	       disabled_count, removed_pd_count);
	return 0;
}

static struct device_tree_fixup bluey_defective_cores_fixup = {
	.fixup = &bluey_fixup_defective_cores
};

static int bluey_cpumap_fixup_init(void)
{
	list_insert_after(&bluey_defective_cores_fixup.list_node,
			  &device_tree_fixups);
	return 0;
}

INIT_FUNC(bluey_cpumap_fixup_init);
