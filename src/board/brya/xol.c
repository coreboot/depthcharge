// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/storage_common.h"

#define SDMODE_PIN		GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	memset(&config, 0, sizeof(config));

	config = (struct audio_config){
		.bus = {
			.type			= AUDIO_I2S,
			.i2s.address		= SSP_I2S1_START_ADDRESS,
			.i2s.enable_gpio	= { .pad = SDMODE_PIN },
			.i2s.settings		= &max98357a_settings,
		},
		.amp = {
			.type			= AUDIO_GPIO_AMP,
			.gpio.enable_gpio	= SDMODE_PIN,
		},
		.codec = {
			.type			= AUDIO_MAX98357,
		},
	};

	return &config;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
        { .media = STORAGE_UFS, .pci_dev = PCH_DEV_UFS1 },
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
