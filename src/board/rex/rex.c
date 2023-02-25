// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/rex/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/gpio/meteorlake.h"
#include "drivers/soc/meteorlake.h"

#define SDMODE_PIN GPP_D4

static void initialize_i2s_config(struct audio_config *config)
{
	*config = (struct audio_config) {
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
}

static void initialize_soundwire_config(struct audio_config *config)
{
	*config = (struct audio_config) {
		.bus = {
			.type			= AUDIO_SNDW,
			.sndw.link		= AUDIO_SNDW_LINK2,
		},
		.codec = {
			.type			= AUDIO_MAX98363,
		},
	};
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	memset(&config, 0, sizeof(config));

	if (fw_config_probe(FW_CONFIG(AUDIO, MAX98360_ALC5682I_I2S)))
		initialize_i2s_config (&config);
	else if (fw_config_probe(FW_CONFIG(AUDIO, MAX98363_CS42L42_SNDW)))
		initialize_soundwire_config (&config);

	return &config;
}
