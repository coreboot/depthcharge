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

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	/* TODO update logic for retimerless firmware update */
	*image_path = NULL;
	*hash_path = NULL;
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config) {
		.bus = {
			.type = AUDIO_SNDW,
			.sndw.link = AUDIO_SNDW_LINK3,
		},
		.codec = {
			.type = AUDIO_RT721,
		},
	};

	return &config;
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE1,
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW1_17); /* GPP_B17 */
}
