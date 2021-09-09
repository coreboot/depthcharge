// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	return &config;
}

const struct tcss_map typec_map[] = {
	{ .usb2_port = 1, .usb3_port = 0, .ec_port = 0 },
	{ .usb2_port = 3, .usb3_port = 2, .ec_port = 1 },
};

const struct tcss_map *variant_get_tcss_map(size_t *count)
{
	*count = ARRAY_SIZE(typec_map);
	return typec_map;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}
