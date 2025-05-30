// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/fw_config.h"
#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/sound/intel_audio_setup.h"

#define PDC_RTS5452_PROJ_NAME	"GOOG0800"
#define PDC_RTS5453_PROJ_NAME	"GOOG0400"

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (CONFIG(DRIVER_SOUND_HDA) && fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC256_HDA))) {
		config = (struct audio_config){
			.bus = {
				.type = AUDIO_HDA,
			},
			.codec = {
				.type = AUDIO_ALC256,
			},
		};
	} else if (CONFIG(DRIVER_SOUND_RT721_SNDW) &&
			fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC721_SNDW))) {
		config = (struct audio_config) {
			.bus = {
				.type = AUDIO_SNDW,
				.sndw.link = AUDIO_SNDW_LINK3,
			},
			.codec = {
				.type = AUDIO_RT721,
			},
		};
	} else {
		printf("Implement variant audio config for other FW CONFIG options\n");
	}

	return &config;
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE1,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_NVME_GEN4)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE5,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_NVME_GEN4)
	},
	{
		.media = STORAGE_UFS,
		.pci_dev = PCI_DEV_UFS,
		.ufs = {
			.ref_clk_freq = UFS_REFCLKFREQ_38_4
		},
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UFS)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE1,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE5,
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
	{
		.media = STORAGE_UFS,
		.pci_dev = PCI_DEV_UFS,
		.ufs = {
			.ref_clk_freq = UFS_REFCLKFREQ_38_4
		},
		.fw_config = FW_CONFIG(STORAGE, STORAGE_UNKNOWN)
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	if (!strcmp(r->fw_name_str, PDC_RTS5452_PROJ_NAME)) {
		*image_path = "rts5452_retimer_jhl9040.bin";
		*hash_path = "rts5452_retimer_jhl9040.hash";
	} else if (!strcmp(r->fw_name_str, PDC_RTS5453_PROJ_NAME)) {
		*image_path = "rts5453_retimer_jhl9040.bin";
		*hash_path = "rts5453_retimer_jhl9040.hash";
	} else {
		*image_path = NULL;
		*hash_path = NULL;
	}
}
