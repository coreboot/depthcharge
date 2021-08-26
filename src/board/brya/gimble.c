// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98390.h"

#define AUD_I2C_ADDR1		0x38
#define AUD_I2C_ADDR2		0x3c
#define SDMODE_PIN		GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (fw_config_probe(FW_CONFIG(AUDIO, MAX98390_ALC5682I_I2S))) {
		config = (struct audio_config){
			.bus = {
				.i2s.address = SSP_I2S2_START_ADDRESS,
				.i2s.enable_gpio = SDMODE_PIN,
				.i2s.settings = &max98390_settings,
			},
			.amp = {
				.type = AUDIO_AMP_NONE,
			},
			.codec = {
				.type = AUDIO_MAX98390,
				.i2c.ctrlr = I2C0,
				.i2c.i2c_addr[0] = AUD_I2C_ADDR1,
				.i2c.i2c_addr[1] = AUD_I2C_ADDR2,
			},
		};
	}

	return &config;
}

/*
 * This map contains the following information about the Type-C ports:
 * USB2 port (1-based)
 * USB3 Type-C port (0-based)
 * EC port (0-based)
 */
static const struct tcss_map typec_map[] = {
	{ .usb2_port = 1, .usb3_port = 0, .ec_port = 0 },
	{ .usb2_port = 2, .usb3_port = 2, .ec_port = 1 },
};

const struct tcss_map *variant_get_tcss_map(size_t *count)
{
	*count = ARRAY_SIZE(typec_map);
	return typec_map;
}
