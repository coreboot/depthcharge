// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/sound/max98396.h"

#define SDMODE_PIN		GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config = {
		.bus = {
			.type = AUDIO_I2S,
			.i2s.address = SSP_I2S1_START_ADDRESS,
			.i2s.settings = &max98396_settings,
			.i2s.enable_gpio = {
				.pad = SDMODE_PIN,
				.active_low = false,
			},
		},
		.amp = {
			.type = AUDIO_AMP_NONE,
			.gpio.enable_gpio = SDMODE_PIN,
		},
		.codec = {
			.type = AUDIO_MAX98396,
			.i2c[0].ctrlr = I2C0,
			.i2c[0].i2c_addr[0] = 0x3c,
			.i2c[0].i2c_addr[1] = 0x3d,
		},
	};

	return &config;
}

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCH_DEV_PCIE8 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	/* TPM is on I2C port 1 */
	static struct tpm_config config = {
		.pci_dev = PCI_DEV(0, 0x15, 1),
	};

	return &config;
}
