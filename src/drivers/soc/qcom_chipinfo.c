/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2021 Google LLC
 *
 */

#include <arch/barrier.h>
#include <libpayload.h>
#include <stdint.h>
#include "drivers/soc/qcom_chipinfo.h"

static uint32_t chipinfo_cluster_array[QCOM_CHIPINFO_MAX_CPU_CLUSTERS];

const uint32_t *qcom_chipinfo_get_defective_cpu_clusters(
	const struct qcom_chipinfo_cpu_fuse *fuse)
{
	uint32_t fuse_val;
	uint32_t i;

	if (!fuse || !fuse->num_clusters ||
	    fuse->num_clusters > QCOM_CHIPINFO_MAX_CPU_CLUSTERS)
		return NULL;

	printf("ChipInfo: reading CPU fuse @ 0x%08lx\n",
	       (unsigned long)fuse->reg_addr);

	/*
	 * Compiler + CPU memory barrier: ensure the printf output is
	 * committed to the UART before the MMIO read that may fault.
	 */
	dsb();

	fuse_val = read32((void *)fuse->reg_addr);

	for (i = 0; i < fuse->num_clusters; i++) {
		chipinfo_cluster_array[i] =
			(fuse_val >> fuse->clusters[i].shift) &
			fuse->clusters[i].mask;
	}

	return chipinfo_cluster_array;
}
