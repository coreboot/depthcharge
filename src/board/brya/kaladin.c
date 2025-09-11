// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/storage_common.h"

#define EMMC_CLOCK_MIN		400000
#define EMMC_CLOCK_MAX		200000000

#define EC_PCH_INT_ODL		GPD2

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
					int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = NULL;
	*hash_path = NULL;
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type = AUDIO_HDA,
		},
		.codec = {
			.type = AUDIO_ALC256,
		},
	};

	return &config;
}

int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_EMMC,
		.pci_dev = PCH_DEV_EMMC,
		.emmc = {
			.platform_flags = SDHCI_PLATFORM_SUPPORTS_HS400ES,
			.clock_min = EMMC_CLOCK_MIN,
			.clock_max = EMMC_CLOCK_MAX
			},
		.fw_config = FW_CONFIG(STORAGE, STORAGE_EMMC)
	},
	{
		.media = STORAGE_UFS,
		.pci_dev = PCH_DEV_UFS1,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UFS)
	},
	{ 	.media = STORAGE_NVME,
		.pci_dev = PCH_DEV_PCIE10,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_NVME)
	},
	{
		.media = STORAGE_EMMC,
		.pci_dev = PCH_DEV_EMMC,
		.emmc = {
				.platform_flags = SDHCI_PLATFORM_SUPPORTS_HS400ES,
				.clock_min = EMMC_CLOCK_MIN,
				.clock_max = EMMC_CLOCK_MAX
		},
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
	{
		.media = STORAGE_UFS,
		.pci_dev = PCH_DEV_UFS1,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
	{ 	.media = STORAGE_NVME,
		.pci_dev = PCH_DEV_PCIE10,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCH_DEV_I2C0,
	};

	return &config;
}

const int variant_get_ec_int(void)
{
	return EC_PCH_INT_ODL;
}
