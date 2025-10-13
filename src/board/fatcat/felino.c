// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "base/fw_config.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/ec/tps6699x/tps6699x.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/sound/intel_audio_setup.h"
#include "drivers/sound/rt1320_sndw.h"
#include "drivers/sound/rt7xx_sndw.h"
#include "drivers/sound/sndw_common.h"
#include "drivers/storage/storage_common.h"

#define EC_SOC_INT_ODL		GPP_E03

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC712_SNDW))) {
		unsigned int link = AUDIO_SNDW_LINK3;
		enum audio_codec_type audio_type = audio_get_type(link);

		if (audio_type == AUDIO_CODEC_NONE)
			return NULL;

		config = (struct audio_config){
			.bus = {
				.type	= AUDIO_SNDW,
				.sndw.link	= link,
			},
			.codec = {
				.type	= audio_type,
			},
		};
	}

	return &config;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE9 },
	{ .media = STORAGE_RTKMMC, .pci_dev = PCI_DEV_PCIE2 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW2_15); /* GPP_F15 */
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

/* Override of func in src/drivers/ec/tps6699x/tps6699x.c */
void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path,
				    int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "tps6699x_GOOG0700.bin";
	*hash_path = "tps6699x_GOOG0700.hash";
}
