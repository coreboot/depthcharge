// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/fw_config.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/sound/intel_audio_setup.h"
#include "drivers/storage/storage_common.h"
#include "drivers/ec/rts5453/rts5453.h"

#define EC_SOC_INT_ODL		GPP_B05

void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path,
				    int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "tps6699x_GOOG0A00.bin";
	*hash_path = "tps6699x_GOOG0A00.hash";
}

void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				    int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "rts5453_GOOG0500.bin";
	*hash_path = "rts5453_GOOG0500.hash";
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (CONFIG(DRIVER_SOUND_HDA) &&
			fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC256M_CG_HDA))) {
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
	} else if (CONFIG(DRIVER_SOUND_RT721_SNDW) &&
			fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC721_SNDW))) {
		config = (struct audio_config) {
			.bus = {
				.type = AUDIO_SNDW,
				.sndw.link = AUDIO_SNDW_LINK0,
			},
			.codec = {
				.type = AUDIO_RT721,
			},
		};
	} else if (CONFIG(DRIVER_SOUND_RT1320_SNDW) &&
			fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC722_ALC1320_SNDW))) {
		config = (struct audio_config) {
			.bus = {
				.type = AUDIO_SNDW,
				.sndw.link = AUDIO_SNDW_LINK2,
			},
			.codec = {
				.type = AUDIO_RT1320,
			},
		};
	} else
		printf("Implement varaint audio config for other FW CONFIG options\n");

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
