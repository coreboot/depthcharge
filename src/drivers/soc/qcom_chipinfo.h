/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __DRIVERS_SOC_QCOM_CHIPINFO_H__
#define __DRIVERS_SOC_QCOM_CHIPINFO_H__

#include <stdint.h>

#define QCOM_CHIPINFO_MAX_CPU_CLUSTERS		8
#define QCOM_CHIPINFO_MAX_CPUS_PER_CLUSTER	32

struct qcom_chipinfo_cluster_fuse {
	uint32_t shift;
	uint32_t mask;
};

struct qcom_chipinfo_cpu_fuse {
	uintptr_t reg_addr;
	uint32_t num_clusters;
	struct qcom_chipinfo_cluster_fuse
		clusters[QCOM_CHIPINFO_MAX_CPU_CLUSTERS];
};

/*
 * qcom_chipinfo_get_defective_cpu_clusters - read per-cluster defective CPU
 * bitmasks directly from a QFPROM fuse register.
 *
 * @fuse: fuse register and per-cluster bit-field description.
 *
 * Returns a static array of fuse->num_clusters uint32_t values, or NULL if the
 * fuse description is invalid. Each element is a bitmask for one cluster where
 * bit j = 1 means core j in that cluster is defective. The array is overwritten
 * by the next successful call.
 */
const uint32_t *qcom_chipinfo_get_defective_cpu_clusters(
	const struct qcom_chipinfo_cpu_fuse *fuse);

#endif /* __DRIVERS_SOC_QCOM_CHIPINFO_H__ */
