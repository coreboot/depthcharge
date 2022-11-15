// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/sdhci_gli.h"
#include "drivers/storage/storage_common.h"

#define EMMC_CLOCK_MIN		400000
#define EMMC_CLOCK_MAX		200000000

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type = AUDIO_PWM,
			.pwm.pad = GPP_B14,
		},
	};

	return &config;
}

static const struct emmc_config gl9763e_cfg = {
	.platform_flags = SDHCI_PLATFORM_SUPPORTS_HS400ES,
	.clock_min = EMMC_CLOCK_MIN,
	.clock_max = EMMC_CLOCK_MAX,
};

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
	{ .media = STORAGE_SDHCI, .pci_dev = PCH_DEV_PCIE7 },
	{ .media = STORAGE_EMMC, .pci_dev = PCH_DEV_PCIE11, .emmc = gl9763e_cfg},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV(0, 0x15, 1),
	};

	return &config;
}
