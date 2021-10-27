// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/guybrush/include/variant.h"

/* FW_CONFIG for Storage Type */
#define FW_CONFIG_STORAGE_TYPE (1 << 6)

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config bridge_storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x03)},
		{ .media = STORAGE_SDHCI, .pci_dev = PCI_DEV(0, 0x02, 0x02)},
        };

	static const struct storage_config ssd_storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x04)},
		{ .media = STORAGE_SDHCI, .pci_dev = PCI_DEV(0, 0x02, 0x02)},
	};

	if (!(lib_sysinfo.fw_config & FW_CONFIG_STORAGE_TYPE)) {
		*count = ARRAY_SIZE(bridge_storage_configs);
		return bridge_storage_configs;
	}

	*count = ARRAY_SIZE(ssd_storage_configs);
	return ssd_storage_configs;
}

unsigned int variant_get_cr50_irq_gpio(void)
{
	return (lib_sysinfo.board_id == 1) ? CR50_INT_3 : CR50_INT_85;
}
