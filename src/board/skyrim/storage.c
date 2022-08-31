// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/skyrim/include/variant.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/storage_common.h"
#include "drivers/storage/rtk_mmc.h"

static void setup_rtkmmc(pcidev_t dev)
{
	if (!CONFIG(DRIVER_STORAGE_MMC_RTK))
		return;

	RtkMmcHost *rtkmmc = probe_pci_rtk_host(dev, RTKMMC_PLATFORM_REMOVABLE);
	if (rtkmmc) {
		rtkmmc->name = "rtksd";
		list_insert_after(&rtkmmc->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
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
	const struct storage_config *config =
		variant_get_storage_configs(&count);
	if (!config || !count)
		return 0;

	for (size_t i = 0; i < count; i++) {
		const pcidev_t dev = config[i].pci_dev;

		switch (config[i].media) {
		case STORAGE_NVME:
			setup_nvme(dev);
			break;
		case STORAGE_RTKMMC:
			setup_rtkmmc(dev);
			break;
		default:
			break;
		}
	}

	return 0;
}

INIT_FUNC(configure_storage);
