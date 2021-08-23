/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Common SoC driver for Intel SOC.
 */

#ifndef __DRIVERS_SOC_INTEL_COMMON_H__
#define __DRIVERS_SOC_INTEL_COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <pci.h>
#include <pci/pci.h>

/* Platform specific GPE configuration */
typedef struct SocGpeConfig {
	int gpe_max;
	uint16_t acpi_base;
	uint16_t gpe0_sts_off;
} SocGpeConfig;

int soc_get_gpe(int gpe, const SocGpeConfig *cfg);

typedef struct {
	unsigned int slot;
	unsigned int first_fn;
	unsigned int count;
} SocPcieRpGroup;

static inline unsigned int rp_start_fn(const SocPcieRpGroup *group)
{
	return group->first_fn;
}

static inline unsigned int rp_end_fn(const SocPcieRpGroup *group)
{
	return group->first_fn + group->count - 1;
}

/* Since the FSP disables PCIe RPs that it finds no downstream devices on, if
 * function 0 gets disabled, one of the used RPs will get remapped to function 0
 * in order to preserve PCI compatibility (a multi-function device must always
 * have function 0 present). Therefore, this function will return the correct
 * mapping if it has been remapped, original otherwise.
 *
 * Assumes that `groups` are sorted such that the PCIe RP numbers within and
 * across groups are in increasing order.
 */
pcidev_t intel_remap_pcie_rp(pcidev_t rp, const SocPcieRpGroup *groups,
			     size_t num_groups);

#endif /* __DRIVERS_SOC_INTEL_COMMON_H__ */
