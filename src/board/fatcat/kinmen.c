// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/fw_config.h"
#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/tps6699x/tps6699x.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/sound/intel_audio_setup.h"

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	if (CONFIG(DRIVER_SOUND_RT721_SNDW) &&
			fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC721_SNDW))){
		config = (struct audio_config){
			.bus = {
				.type = AUDIO_SNDW,
				.sndw.link = AUDIO_SNDW_LINK3,
			},
			.codec = {
				.type = AUDIO_RT721,
			},
		};
	} else if (CONFIG(DRIVER_SOUND_RT1320_SNDW) &&
			fw_config_probe(FW_CONFIG(AUDIO, AUDIO_ALC1320_ALC721_SNDW))) {
		config = (struct audio_config) {
			.bus = {
				.type = AUDIO_SNDW,
				.sndw.link = AUDIO_SNDW_LINK3,
			},
			.codec = {
				.type = AUDIO_RT1320,
			},
		};
	} else
		printf("Implement varaint audio config for other FW CONFIG options\n");

	return &config;
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE9,
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW2_02); /* GPP_E02 */
}

/* Override of functions in src/drivers/ec/tps6699x/tps6699x.c */
void board_tps6699x_register(void)
{
	static CrosEcAuxfwChipInfo tps6699x_info = {
		.vid = CONFIG_DRIVER_EC_TPS6699X_VID,
		.pid = 0x506c,
		.new_chip_aux_fw_ops = new_tps6699x_from_chip_info,
	};
	list_insert_after(&tps6699x_info.list_node, &ec_aux_fw_chip_list);
}

void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path)
{
	*image_path = "tps6699x_GOOG0700.bin";
	*hash_path = "tps6699x_GOOG0700.hash";
}
