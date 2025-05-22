// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/sound/intel_audio_setup.h"

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	config = (struct audio_config){
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
		.pci_dev = PCI_DEV_PCIE9,
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}
