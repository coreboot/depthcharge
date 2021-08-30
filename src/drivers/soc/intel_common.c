// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 *
 * Common SoC driver for Intel SOC.
 */

#include <arch/io.h>
#include <libpayload.h>
#include <stdint.h>
#include "drivers/soc/common/iomap.h"
#include "drivers/soc/intel_common.h"

#define GPE0_STS_SIZE 4
#define GPE0_STS(off, x) ((off) + ((x) * GPE0_STS_SIZE))

#define PCH_PCIE_CFG_LCAP_PN	0x4f /* Root Port Number */

int soc_get_gpe(int gpe, const SocGpeConfig *cfg)
{
	int bank;
	uint32_t mask, sts;
	uint64_t start;
	int rc = 0;
	const uint64_t timeout_us = 1000;
	uint16_t gpe0_sts_port;

	if (gpe < 0 || gpe > cfg->gpe_max)
		return -1;

	bank = gpe / 32;
	mask = 1 << (gpe % 32);
	gpe0_sts_port = cfg->acpi_base + GPE0_STS(cfg->gpe0_sts_off, bank);

	/* Wait for GPE status to clear */
	start = timer_us(0);
	do {
		if (timer_us(start) > timeout_us)
			break;

		sts = inl(gpe0_sts_port);
		if (sts & mask) {
			outl(mask, gpe0_sts_port);
			rc = 1;
		}
	} while (sts & mask);

	return rc;
}

static int get_original_pn(pcidev_t rp, const SocPcieRpGroup *groups,
			   size_t num_groups)
{
	unsigned int rp_slot = PCI_SLOT(rp);
	unsigned int rp_func = PCI_FUNC(rp);

	unsigned int idx = 1;
	for (size_t i = 0; i < num_groups; i++) {
		const SocPcieRpGroup *group = &groups[i];

		if (rp_slot != group->slot) {
			idx += group->count;
			continue;
		}

		if (rp_func < rp_start_fn(group))
			return -1;

		if (rp_func > rp_end_fn(group)) {
			idx += group->count;
			continue;
		}

		idx += rp_func - rp_start_fn(group);
		return idx;
	}

	return -1;
}

pcidev_t intel_remap_pcie_rp(pcidev_t rp, const SocPcieRpGroup *groups,
			     size_t num_groups)
{
	const int original_pn = get_original_pn(rp, groups, num_groups);
	const unsigned int lcap_pn = pci_read_config8(rp, PCH_PCIE_CFG_LCAP_PN);

	if ((int)lcap_pn == original_pn)
		return rp;

	/* Bummer, this device doesn't match; scan PCI config space to
	 * reconstruct where rp got moved to */
	for (size_t i = 0; i < num_groups; i++) {
		const SocPcieRpGroup *group = &groups[i];
		unsigned int fn;

		for (fn = rp_start_fn(group); fn <= rp_end_fn(group); fn++) {
			const pcidev_t dev = PCI_DEV(0, group->slot, fn);
			const uint16_t did = pci_read_config16(dev,
							       REG_DEVICE_ID);
			if (did == 0xffff) {
				/* If function 0 is disabled, then the whole
				 * slot is disabled, so skip it */
				if (!fn)
					break;
				continue;
			}

			const uint8_t dev_pn =
				pci_read_config8(dev, PCH_PCIE_CFG_LCAP_PN);
			if (dev_pn == original_pn)
				return dev;
		}
	}

	return (pcidev_t)-1;
}
