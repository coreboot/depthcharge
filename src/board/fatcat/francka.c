// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "board/fatcat/include/variant.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/ec/rts5453/rts5453.h"

#define EC_SOC_INT_ODL		GPP_B05

void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path)
{
	*image_path = "tps6699x_GOOG0A00.bin";
	*hash_path = "tps6699x_GOOG0A00.hash";
}

void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = NULL;
	*hash_path = NULL;
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus =
		{
			.type = AUDIO_HDA,
		},
		.codec =
		{
			.type = AUDIO_ALC256,
		},
	};

	return &config;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW0_11); /* GPP_H11 */
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE9 },
	{ .media = STORAGE_RTKMMC, .pci_dev = PCI_DEV_PCIE3 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV_I2C1,
	};

	return &config;
}

const int variant_get_ec_int(void)
{
	return EC_SOC_INT_ODL;
}
