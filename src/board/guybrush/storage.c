// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/guybrush/include/variant.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/bayhub.h"

static void setup_sdhci(pcidev_t pci_dev)
{
	if (!CONFIG(DRIVER_STORAGE_SDHCI_PCI))
		return;

	SdhciHost *sd = probe_pci_sdhci_host(pci_dev, SDHCI_PLATFORM_REMOVABLE);

	if (sd) {
		sd->name = "SD";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		printf("Failed to find SD card reader\n");
	}

}

static void setup_nvme(pcidev_t dev)
{
	if (!CONFIG(DRIVER_STORAGE_NVME))
		return;

	/* PCIE O2 Bridge for NVMe */
	NvmeCtrlr *nvme = NULL;
	nvme = new_nvme_ctrlr(dev);

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

		switch (config[i].media) {
		case STORAGE_NVME:
			setup_nvme(dev);
			break;
		case STORAGE_SDHCI:
			setup_sdhci(dev);
			break;
		default:
			break;
		}
	}

	return 0;
}

INIT_FUNC(configure_storage);
