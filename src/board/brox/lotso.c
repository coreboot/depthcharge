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
	{ .media = STORAGE_UFS, .pci_dev = PCH_DEV_UFS1 },
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },
	{ .media = STORAGE_RTKMMC, .pci_dev = PCH_DEV_PCIE5 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type = AUDIO_PWM,
			.pwm.pad = GPP_B14,
		},
	};

	return &config;
}

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path,
		const char **hash_path, struct ec_response_pd_chip_info_v2 *r)
{
	*image_path = "rts5453_retimer_bypass.bin";
	*hash_path = "rts5453_retimer_bypass.hash";
}
