// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/fw_config.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"

/* TODO: Implement override for `variant_probe_audio_config` */
static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE5,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_NVME_GEN4)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE1,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_NVME_GEN4)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE9,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_NVME_GEN5)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE5,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE1,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE9,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
        *count = ARRAY_SIZE(storage_configs);
        return storage_configs;
}
