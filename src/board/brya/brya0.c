// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/soundwire/cavs_2_5-sndwregs.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"

#define SDMODE_PIN		GPP_A11

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	memset(&config, 0, sizeof(config));

	if (fw_config_probe(FW_CONFIG(AUDIO, MAX98357_ALC5682I_I2S))) {
		config = (struct audio_config){
			.bus = {
				.type			= AUDIO_I2S,
				.i2s.address		= SSP_I2S2_START_ADDRESS,
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
	} else if (fw_config_probe(FW_CONFIG(AUDIO, MAX98373_ALC5682_SNDW))) {
		config = (struct audio_config){
			.bus = {
				.type			= AUDIO_SNDW,
				.sndw.link		= AUDIO_SNDW_LINK2,
			},
			.codec = {
				.type			= AUDIO_MAX98373,
			},
		};
	}

	return &config;
}
