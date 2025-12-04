// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "base/fw_config.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/tas2563.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/rts5453/rts5453.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/sound/intel_audio_setup.h"
#include "drivers/sound/tas2563.h"
#define AUD_I2C_ADDR1	0x4e
#define AUD_I2C_ADDR2	0x4f
#define AUD_I2C_ADDR3	0x4c
#define AUD_I2C_ADDR4	0x4d

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (CONFIG(DRIVER_SOUND_I2S) &&
	    fw_config_probe(FW_CONFIG(AUDIO_AMPLIFIER, AUDIO_AMPLIFIER_TAS2563))) {
		config = (struct audio_config) {
			.bus = {
				.type = AUDIO_I2S,
				.i2s.address = SSP_I2S1_START_ADDRESS,
				.i2s.settings = &tas2563_settings,
			},
			.codec = {
				.type = AUDIO_TAS2563,
				.i2c[0].ctrlr = PCI_DEV_I2C0,
				.i2c[0].i2c_addr[0] = AUD_I2C_ADDR1,
				.i2c[0].i2c_addr[1] = AUD_I2C_ADDR2,
				.i2c[0].i2c_addr[2] = AUD_I2C_ADDR3,
				.i2c[0].i2c_addr[3] = AUD_I2C_ADDR4,
			},
		};
	} else {
		printf("Implement varaint audio config for other FW CONFIG options\n");
	}

	return &config;
}

static const struct storage_config storage_configs[] = {
	{
		.media = STORAGE_NVME,
		.pci_dev = PCI_DEV_PCIE5,
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


