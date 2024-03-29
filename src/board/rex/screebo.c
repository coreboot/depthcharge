// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include "base/fw_config.h"
#include "board/rex/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/gpio/meteorlake.h"
#include "drivers/soc/meteorlake.h"
#include "drivers/storage/storage_common.h"

#define EN_SPK_PIN		GPP_B7

static void initialize_i2s_config(struct audio_config *config)
{
	*config = (struct audio_config) {
		.bus = {
			.type			= AUDIO_I2S,
			.i2s.address		= SSP_I2S0_START_ADDRESS,
			.i2s.enable_gpio	= { .pad = EN_SPK_PIN },
			.i2s.settings		= &max98357a_settings,
		},
		.amp = {
			.type			= AUDIO_GPIO_AMP,
			.gpio.enable_gpio	= EN_SPK_PIN,
		},
		.codec = {
			.type			= AUDIO_RT1019,
		},
	};
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	memset(&config, 0, sizeof(config));
	if (fw_config_probe(FW_CONFIG(AUDIO, ALC1019_ALC5682I_I2S)))
		initialize_i2s_config(&config);

	return &config;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE9 },
	{ .media = STORAGE_RTKMMC, .pci_dev = PCI_DEV_PCIE10,
	.fw_config = FW_CONFIG(DB_SD,SD_RTS5227S)},
	{ .media = STORAGE_SDHCI, .pci_dev = PCI_DEV_PCIE10,
	.fw_config = FW_CONFIG(DB_SD, SD_GL9750)},
};
const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}
