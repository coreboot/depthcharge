// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brox/include/variant.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/storage_common.h"

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
	{ .media = STORAGE_UFS, .pci_dev = PCH_DEV_UFS1 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path)
{
	if (fw_config_probe(FW_CONFIG(RETIMER, RETIMER_BYPASS))) {
		*image_path = "rts5453_retimer_bypass.bin";
		*hash_path = "rts5453_retimer_bypass.hash";
	} else if (fw_config_probe(FW_CONFIG(RETIMER, RETIMER_JHL8040))) {
		*image_path = "rts5453_retimer_jhl8040.bin";
		*hash_path = "rts5453_retimer_jhl8040.hash";
	} else {
		/* Unknown configuration. Don't perform an update. */
		*image_path = NULL;
		*hash_path = NULL;
	}
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
