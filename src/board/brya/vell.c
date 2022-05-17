// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/sound/cs35l53.h"

#define AUD_I2C_ADDR1		0x43
#define AUD_I2C_ADDR2		0x42
#define AUD_I2C_ADDR3		0x41
#define AUD_I2C_ADDR4		0x40
#define SDMODE_PIN		GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	config = (struct audio_config){
		.bus = {
			.type			= AUDIO_I2S,
			.i2s.address		= SSP_I2S1_START_ADDRESS,
			.i2s.enable_gpio	= { .pad =  SDMODE_PIN,
						    .active_low = false },
			.i2s.settings		= &cs35l53_settings,
		},
		.amp = {
			.type			= AUDIO_AMP_NONE,
		},
		.codec = {
			.type			= AUDIO_CS35L53,
			.i2c[0].ctrlr		= I2C0,
			.i2c[0].i2c_addr[0] 	= AUD_I2C_ADDR1,
			.i2c[0].i2c_addr[1] 	= AUD_I2C_ADDR2,
			.i2c[1].ctrlr		= I2C7,
			.i2c[1].i2c_addr[0] 	= AUD_I2C_ADDR3,
			.i2c[1].i2c_addr[1] 	= AUD_I2C_ADDR4,
		},
	};

	return &config;
}

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = SA_DEV_CPU_RP0 },

};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static struct tpm_config config = {
		.pci_dev = PCI_DEV(0, 0x15, 1),
	};
	return &config;
}
