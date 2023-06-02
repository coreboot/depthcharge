// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/rex/include/variant.h"
#include "drivers/gpio/meteorlake.h"
#include "drivers/soc/meteorlake.h"
#include "drivers/storage/storage_common.h"

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	return &config;
}

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE11 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}
