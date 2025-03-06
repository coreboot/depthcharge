// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "drivers/ec/tps6699x/tps6699x.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "base/fw_config.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"

#define EC_SOC_INT_ODL		GPP_E03

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type			= AUDIO_I2S,
			.i2s.address	= SSP_I2S1_START_ADDRESS,
			.i2s.settings	= &max98357a_settings,
		},
		.amp = {
			.type			= AUDIO_AMP_NONE,
		},
		.codec = {
			.type			= AUDIO_RT5650,
			.i2c[0].ctrlr		= PCI_DEV_I2C3,
			.i2c[0].i2c_addr[0]	= 0x1a,
		},
	};

	return &config;
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
