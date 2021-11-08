// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/sdhci_gli.h"

#define SDMODE_PIN		GPP_A11
#define SDMODE_ENABLE		0
#define EMMC_CLOCK_MIN		400000
#define EMMC_CLOCK_MAX		200000000

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (fw_config_probe(FW_CONFIG(AUDIO, AUDIO_MAX98357_ALC5682I_I2S))) {
		config = (struct audio_config){
			.bus = {
				.type			= AUDIO_I2S,
				.i2s.address		= SSP_I2S2_START_ADDRESS,
				.i2s.enable_gpio = {
							.pad = SDMODE_PIN,
							.active_low = SDMODE_ENABLE
				},
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
	}
	return &config;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
	{ .media = STORAGE_EMMC, .pci_dev = SA_DEV_CPU_RP0, .emmc = {
		.platform_flags = SDHCI_PLATFORM_SUPPORTS_HS400ES,
		.clock_min = EMMC_CLOCK_MIN,
		.clock_max = EMMC_CLOCK_MAX }},
};

static const struct storage_config storage_configs_for_id0[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCH_DEV_PCIE8 },
	{ .media = STORAGE_EMMC, .pci_dev = PCH_DEV_PCIE8, .emmc = {
		.platform_flags = SDHCI_PLATFORM_SUPPORTS_HS400ES,
		.clock_min = EMMC_CLOCK_MIN,
		.clock_max = EMMC_CLOCK_MAX }},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	if (!fw_config_is_provisioned()) {
		*count = ARRAY_SIZE(storage_configs);
		return storage_configs;
	} else {
		const struct storage_config *storage_config_ptr;
		storage_config_ptr = (lib_sysinfo.board_id == 0) ?
					storage_configs_for_id0 : storage_configs;
		if (fw_config_probe(FW_CONFIG(BOOT_NVME_MASK, BOOT_NVME_ENABLED))) {
			*count = 1;
			return storage_config_ptr;
		} else if (fw_config_probe(FW_CONFIG(BOOT_EMMC_MASK, BOOT_EMMC_ENABLED))) {
			*count = 1;
			return storage_config_ptr + 1;
		}
	}

	*count = 0;
	return NULL;
}
