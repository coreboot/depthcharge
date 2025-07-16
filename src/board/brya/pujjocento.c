// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include "board/brya/include/variant.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/storage_common.h"

#define EC_PCH_INT_ODL		GPD2

/* Override of func in src/drivers/ec/rts5453/rts5453.c */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path,
				   int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	switch (ec_pd_id) {
	case 0:
		/* bypass retimer */
		*image_path = "rts5453_GOOG0901.bin";
		*hash_path = "rts5453_GOOG0901.hash";
		break;
	case 1:
		/* TUSB546/1044 */
		*image_path = "rts5453_GOOG0F00.bin";
		*hash_path = "rts5453_GOOG0F00.hash";
		break;
	default:
		printf("Unknown ec_pd_id %d\n", ec_pd_id);
	}
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type = AUDIO_PWM,
			.pwm.pad = GPP_B14,
		}
	};

	return &config;
}

int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_UFS,
		.pci_dev = PCH_DEV_UFS1,
	},
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCH_DEV_I2C0,
	};

	return &config;
}

const int variant_get_ec_int(void)
{
	return EC_PCH_INT_ODL;
}
