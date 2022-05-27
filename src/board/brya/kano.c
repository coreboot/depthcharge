// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/sound/max98373.h"

#define AUD_I2C_ADDR		0x32
#define SDMODE_PIN		GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type			= AUDIO_I2S,
			.i2s.address		= SSP_I2S1_START_ADDRESS,
			.i2s.enable_gpio	= { .pad =  SDMODE_PIN,
						    .active_low = false },
			.i2s.settings		= &max98373_settings,
		},
		.amp = {
			.type			= AUDIO_AMP_NONE,
			.gpio.enable_gpio	= SDMODE_PIN,
		},
		.codec = {
			.type			= AUDIO_MAX98373,
			.i2c[0].ctrlr		= I2C0,
			.i2c[0].i2c_addr[0]	= AUD_I2C_ADDR,
		},
	};

	return &config;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
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
