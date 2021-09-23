// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/brya/include/variant.h"
#include "drivers/soc/alderlake.h"
#include "drivers/soc/intel_common.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/sdhci_gli.h"

static void setup_emmc(pcidev_t dev, const struct emmc_config *config)
{
	if (!CONFIG(DRIVER_STORAGE_SDHCI_PCI))
		return;

	if (CONFIG(DRIVER_STORAGE_GENESYSLOGIC)) {
		/* GL9763E */
		SdhciHost *emmc;

		emmc = new_gl9763e_sdhci_host(dev,
					config->platform_flags,
					config->clock_min, config->clock_max);
		if (emmc) {
			emmc->name = "eMMC";
			list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
					  &fixed_block_dev_controllers);
		} else {
			printf("Failed to find eMMC card reader\n");
		}
	}
}

static void setup_sdhci(pcidev_t dev)
{
	if (!CONFIG(DRIVER_STORAGE_SDHCI_PCI))
		return;

	SdhciHost *sd = probe_pci_sdhci_host(dev, SDHCI_PLATFORM_REMOVABLE);
	if (sd) {
		sd->name = "sd";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				  &removable_block_dev_controllers);
	}
}

static void setup_nvme(pcidev_t dev)
{
	if (!CONFIG(DRIVER_STORAGE_NVME))
		return;

	NvmeCtrlr *nvme = new_nvme_ctrlr(dev);
	if (nvme)
		list_insert_after(&nvme->ctrlr.list_node,
				  &fixed_block_dev_controllers);
}

static int configure_storage(void)
{
	size_t count;
	const struct storage_config *config = variant_get_storage_configs(&count);
	if (!config || !count)
		return 0;

	for (size_t i = 0; i < count; i++) {
		const pcidev_t dev = config[i].pci_dev;
		const SocPcieRpGroup *group;
		size_t group_count;
		pcidev_t remapped;

		group = soc_get_rp_group(dev, &group_count);
		if (group)
			remapped = intel_remap_pcie_rp(dev, group, group_count);
		else
			remapped = dev;

		if (remapped == (pcidev_t)-1) {
			printf("%s: Failed to remap %2X:%X\n",
			       __func__, PCI_SLOT(dev), PCI_FUNC(dev));
			continue;
		}

		switch (config[i].media) {
		case STORAGE_NVME:
			setup_nvme(remapped);
			break;
		case STORAGE_SDHCI:
			setup_sdhci(remapped);
			break;
		case STORAGE_EMMC:
			setup_emmc(remapped, &config[i].emmc);
			break;
		default:
			break;
		}
	}

	return 0;
}

INIT_FUNC(configure_storage);
