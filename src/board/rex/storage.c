// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>
#include <stdio.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/rex/include/variant.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/soc/meteorlake.h"
#include "drivers/soc/intel_common.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/storage_common.h"

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
		pcidev_t remapped;

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
		default:
			break;
		}
	}

	return 0;
}

INIT_FUNC(configure_storage);
