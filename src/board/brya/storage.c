// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/soc/alderlake.h"
#include "drivers/soc/intel_common.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/rtk_mmc.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/sdhci_gli.h"
#include "drivers/storage/ufs.h"
#include "drivers/storage/ufs_intel.h"

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

static void setup_rtkmmc(pcidev_t dev)
{
	if (!CONFIG(DRIVER_STORAGE_SDHCI_PCI))
		return;

	if (CONFIG(DRIVER_STORAGE_MMC_RTK)) {
		RtkMmcHost *rtkmmc = probe_pci_rtk_host(dev, RTKMMC_PLATFORM_REMOVABLE);
		if (rtkmmc) {
			rtkmmc->name = "rtksd";
			list_insert_after(&rtkmmc->mmc_ctrlr.ctrlr.list_node,
				  	&removable_block_dev_controllers);
		}
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

static void setup_ufs(pcidev_t dev)
{
	if (!CONFIG(DRIVER_STORAGE_UFS_INTEL))
		return;

	IntelUfsCtlr *ufs = new_intel_ufs_ctlr(dev);
	if (ufs)
		list_insert_after(&ufs->ufs.bctlr.list_node,
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
		pcidev_t remapped;

		if (config[i].fw_config) {
			if (fw_config_is_provisioned() &&
					!fw_config_probe(config[i].fw_config)) {
				printf("%s not support\n",
					storage_to_string(config[i]));
				continue;
			}
		}

		remapped = remap_pci_dev(dev);

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
		case STORAGE_RTKMMC:
			setup_rtkmmc(remapped);
			break;
		case STORAGE_EMMC:
			setup_emmc(remapped, &config[i].emmc);
			break;
		case STORAGE_UFS:
			setup_ufs(remapped);
			break;
		default:
			break;
		}
	}

	return 0;
}

INIT_FUNC(configure_storage);
