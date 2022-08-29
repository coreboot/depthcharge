// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "board/rex/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/gpio/meteorlake.h"
#include "drivers/soc/meteorlake.h"

#define SDMODE_PIN GPP_D4

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config = {
		.bus = {
			.type			= AUDIO_I2S,
			.i2s.address		= SSP_I2S0_START_ADDRESS,
			.i2s.enable_gpio	= { .pad = SDMODE_PIN },
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
