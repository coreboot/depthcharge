// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>

#include "board/brya/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/i2s/intel_common/max98390.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/soc/alderlake.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98390.h"
#include "drivers/sound/route.h"

#define AUD_VOLUME		4000
#define AUD_BITDEPTH		16
#define AUD_SAMPLE_RATE		48000
#define AUD_NUM_CHANNELS	2

#define I2C_FS_HZ		400000

struct audio_data {
	SoundOps *ops;
	SoundRoute *route;
};

static void setup_max98390(const struct audio_codec *codec, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_MAX98390))
		return;

	DesignwareI2c *i2c = new_pci_designware_i2c(codec->i2c.ctrlr, I2C_FS_HZ,
						    ALDERLAKE_DW_I2C_MHZ);

	for (int i = 0; i < MAX_CODEC; i++) {
		if (codec->i2c.i2c_addr[i]) {
			Max98390Codec *max = new_max98390_codec(
				&i2c->ops, codec->i2c.i2c_addr[i]);
			list_insert_after(&max->component.list_node,
					  &route->components);
		}
	}
}

static void setup_gpio_amp(const struct audio_amp *amp, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_GPIO_AMP))
		return;

	GpioCfg *gpio = new_alderlake_gpio_output(amp->gpio.enable_gpio, 0);
	GpioAmpCodec *speaker_amp = new_gpio_amp_codec(&gpio->ops);
	list_insert_after(&speaker_amp->component.list_node,
			  &route->components);
}

static I2sSource *setup_i2s(const struct audio_bus *bus)
{
	GpioCfg *gpio = new_alderlake_gpio_output(bus->i2s.enable_gpio.pad, 
						  bus->i2s.enable_gpio.active_low);
	I2s *i2s = new_i2s_structure(bus->i2s.settings, AUD_BITDEPTH,
				     &gpio->ops, bus->i2s.address);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
					       AUD_NUM_CHANNELS, AUD_VOLUME);
	return i2s_source;
}

static void configure_audio_bus(const struct audio_bus *bus,
				struct audio_data *data)
{
	data->route = new_sound_route(&setup_i2s(bus)->ops);
}

static void configure_audio_codec(const struct audio_codec *codec,
				  struct audio_data *data)
{
	switch (codec->type) {
	case AUDIO_MAX98390:
		setup_max98390(codec, data->route);
		data->ops = &data->route->ops;
		break;

	case AUDIO_MAX98357:
		if (data->route)
			data->ops = &data->route->ops;
		break;
	default:
		break;
	}
}

static void configure_audio_amp(const struct audio_amp *amp,
				struct audio_data *data)
{
	switch (amp->type) {
	case AUDIO_GPIO_AMP:
		setup_gpio_amp(amp, data->route);
		break;
	default:
		break;
	}
}

void brya_configure_audio(const struct audio_config *config)
{
	struct audio_data data = {};

	configure_audio_bus(&config->bus, &data);
	configure_audio_codec(&config->codec, &data);
	configure_audio_amp(&config->amp, &data);

	if (data.ops)
		sound_set_ops(data.ops);
}
