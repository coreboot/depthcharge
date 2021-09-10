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

#define AUD_I2C_ADDR1	0x3a
#define AUD_I2C_ADDR2	0x3b
#define AUD_I2C_ADDR3	0x38
#define AUD_I2C_ADDR4	0x39
#define SDMODE_PIN	GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config = {
		.bus = {
			.i2s.address = SSP_I2S1_START_ADDRESS,
			.i2s.enable_gpio = {
				.pad = SDMODE_PIN,
				.active_low = true
			},
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
			.i2c.i2c_addr[2] = AUD_I2C_ADDR3,
			.i2c.i2c_addr[3] = AUD_I2C_ADDR4,
		},
	};

	return &config;
}

const struct tcss_map typec_map[] = {
	{ .usb2_port = 1, .usb3_port = 0, .ec_port = 0 },
	{ .usb2_port = 3, .usb3_port = 2, .ec_port = 1 },
};

const struct tcss_map *variant_get_tcss_map(size_t *count)
{
	*count = ARRAY_SIZE(typec_map);
	return typec_map;
}
