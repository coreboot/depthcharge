// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "base/fw_config.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/ec/tps6699x/tps6699x.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/sound/rt1320_sndw.h"
#include "drivers/sound/rt7xx_sndw.h"
#include "drivers/sound/sndw_common.h"
#include "drivers/storage/storage_common.h"

#define EC_SOC_INT_ODL		GPP_E03

enum variant_audio_config {
	AUDIO_UNKNOWN,
	AUDIO_ALC712,
	AUDIO_ALC1320,
};

/*
 * variant_compare_codec_id - Helper to compare a codec info ID with a reference ID.
 * @info: Pointer to the sndw_codec_info containing the ID to compare.
 * @ref_id: Pointer to the reference sndw_codec_id to compare against.
 *
 * This function iterates through the ID fields and returns true if they all match,
 * false otherwise.
 */
static bool variant_compare_codec_id(const sndw_codec_info *info, const sndw_codec_id *ref_id)
{
	for (int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if (ref_id->id[i] != info->codecid.id[i])
			return false;
	}
	return true;
}

static bool variant_audio_config_alc712(const sndw_codec_info *info)
{
	return variant_compare_codec_id(info, &rt712_id);
}

static bool variant_audio_config_alc1320(const sndw_codec_info *info)
{
	return variant_compare_codec_id(info, &rt1320_id);
}

static enum variant_audio_config variant_get_audio_config(unsigned int link)
{
	Soundwire *bus = new_soundwire(link);
	sndw_codec_info info;

	if (sndw_probe_and_enable_codec(bus, &info)) {
		printf("Failed to enable codec.\n");
		return AUDIO_UNKNOWN;
	}

	if (variant_audio_config_alc1320(&info))
		return AUDIO_ALC1320;
	else if (variant_audio_config_alc712(&info))
		return AUDIO_ALC712;
	else
		return AUDIO_UNKNOWN;
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC712_SNDW))) {
		unsigned int link = AUDIO_SNDW_LINK3;
		enum variant_audio_config audio_variant = variant_get_audio_config(link);

		if (audio_variant == AUDIO_UNKNOWN)
			return NULL;

		config = (struct audio_config){
			.bus = {
				.type	= AUDIO_SNDW,
				.sndw.link	= link,
			},
			.codec = {
				.type	= (audio_variant == AUDIO_ALC712) ? AUDIO_RT712 : AUDIO_RT1320,
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
void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path)
{
	*image_path = "tps6699x_GOOG0700.bin";
	*hash_path = "tps6699x_GOOG0700.hash";
}
