// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"

#define SDMODE_PIN		GPP_A11
#define SDMODE_ENABLE		0

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	config = (struct audio_config){
		.bus = {
			.i2s.address		= SSP_I2S1_START_ADDRESS,
			.i2s.enable_gpio	= { .pad = SDMODE_PIN,
						    .active_low = SDMODE_ENABLE },
			.i2s.settings		= &max98357a_settings,
		},
		.amp = {
			.type			= AUDIO_GPIO_AMP,
			.gpio.enable_gpio	= SDMODE_PIN,
		},
		.codec = {
			.type			= AUDIO_MAX98357,
		},
	};
	return &config;
}

static const struct tcss_map typec_map[] = {
	{ .usb2_port = 2, .usb3_port = 1, .ec_port = 1 },
	{ .usb2_port = 3, .usb3_port = 2, .ec_port = 2 },
};

const struct tcss_map *variant_get_tcss_map(size_t *count)
{
	*count = ARRAY_SIZE(typec_map);
	return typec_map;
}
