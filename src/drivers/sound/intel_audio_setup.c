// SPDX-License-Identifier: GPL-2.0

#include <pci.h>
#include <pci/pci.h>

#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/i2s/intel_common/max98390.h"
#include "drivers/bus/i2s/intel_common/i2s-dma.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/soundwire/soundwire.h"
#include "drivers/soc/intel_common.h"
#include "drivers/soc/intel_gpio.h"
#include "drivers/sound/cs35l53.h"
#include "drivers/sound/cs35l56_sndw.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/gpio_edge_buzzer.h"
#include "drivers/sound/hda_codec.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/intel_audio_setup.h"
#include "drivers/sound/max98373.h"
#include "drivers/sound/max98373_sndw.h"
#include "drivers/sound/max98363_sndw.h"
#include "drivers/sound/max98390.h"
#include "drivers/sound/max98396.h"
#include "drivers/sound/nau8318.h"
#include "drivers/sound/route.h"
#include "drivers/sound/rt5645.h"
#include "drivers/sound/rt7xx_sndw.h"
#include "drivers/sound/rt1320_sndw.h"
#include "drivers/sound/sndw_common.h"

/* Default I2S setting 4000 volume, 16bit, 2ch, 48k */
#define AUD_VOLUME		4000
#define AUD_BITDEPTH		16
#define AUD_SAMPLE_RATE		48000
#define AUD_NUM_CHANNELS	2
#define BEEP_DURATION		120
#define I2C_FS_HZ		400000

#define SOC_DW_I2C_MHZ	133

struct audio_data {
	enum audio_bus_type type;
	SoundOps *ops;
	union {
		SoundRoute *route; /* I2S */
		Soundwire *sndw;   /* Soundwire */
		HdaCodec *codec;   /* HDA */
	};
};

/* return default value if value is 0 */
static int default_if_zero(int value, int def)
{
	return value ? : def;
}

static void setup_max98390(const struct audio_codec *codec, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_MAX98390))
		return;

	DesignwareI2c *i2c = new_pci_designware_i2c(codec->i2c[0].ctrlr,
		default_if_zero(codec->speed, I2C_FS_HZ), SOC_DW_I2C_MHZ);

	for (int i = 0; i < MAX_CODEC; i++) {
		if (codec->i2c[0].i2c_addr[i]) {
			Max98390Codec *max = new_max98390_codec(
				&i2c->ops, codec->i2c[0].i2c_addr[i]);
			list_insert_after(&max->component.list_node,
					  &route->components);
		}
	}
}

static void setup_max98396(const struct audio_codec *codec, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_MAX98396))
		return;

	DesignwareI2c *i2c = new_pci_designware_i2c(codec->i2c[0].ctrlr,
		default_if_zero(codec->speed, I2C_FS_HZ), SOC_DW_I2C_MHZ);

	for (int i = 0; i < MAX_CODEC; i++) {
		if (codec->i2c[0].i2c_addr[i]) {
			Max98396Codec *max = new_max98396_codec(
				&i2c->ops, codec->i2c[0].i2c_addr[i]);
			list_insert_after(&max->component.list_node,
					  &route->components);
		}
	}
}

static void setup_max98373(const struct audio_codec *codec, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_MAX98373))
		return;

	DesignwareI2c *i2c = new_pci_designware_i2c(codec->i2c[0].ctrlr,
		default_if_zero(codec->speed, I2C_FS_HZ), SOC_DW_I2C_MHZ);

	Max98373Codec *max = new_max98373_codec(&i2c->ops,
						codec->i2c[0].i2c_addr[0]);
	list_insert_after(&max->component.list_node, &route->components);
}

static void setup_cs35l53(const struct audio_codec *codec, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_CS35L53))
		return;

	DesignwareI2c *i2c1 = new_pci_designware_i2c(codec->i2c[0].ctrlr,
		default_if_zero(codec->speed, I2C_FS_HZ), SOC_DW_I2C_MHZ);

	DesignwareI2c *i2c2 = new_pci_designware_i2c(codec->i2c[1].ctrlr,
		default_if_zero(codec->speed, I2C_FS_HZ), SOC_DW_I2C_MHZ);

	for (int i = 0; i < MAX_CODEC; i++) {
		if (codec->i2c[0].i2c_addr[i]) {
			cs35l53Codec *max1 = new_cs35l53_codec(&i2c1->ops,
						codec->i2c[0].i2c_addr[i]);
			list_insert_after(&max1->component.list_node,
						&route->components);
		}
	}

	for (int i = 0; i < MAX_CODEC; i++) {
		if (codec->i2c[1].i2c_addr[i]) {
			cs35l53Codec *max2 = new_cs35l53_codec(&i2c2->ops,
						codec->i2c[1].i2c_addr[i]);
			list_insert_after(&max2->component.list_node,
						&route->components);
		}
	}
}

static SoundOps *setup_cs35l56_sndw(SndwOps *ops, unsigned int beep_ms)
{
	if (!CONFIG(DRIVER_SOUND_CS35L56_SNDW))
		return NULL;

	return &new_cs35l56_sndw(ops, beep_ms)->ops;
}

static void setup_rt5650(const struct audio_codec *codec, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_RT5645))
		return;

	DesignwareI2c *i2c = new_pci_designware_i2c(codec->i2c[0].ctrlr,
		default_if_zero(codec->speed, I2C_FS_HZ), SOC_DW_I2C_MHZ);

	rt5645Codec *amp = new_rt5645_codec(&i2c->ops,
						codec->i2c[0].i2c_addr[0]);
	list_insert_after(&amp->component.list_node, &route->components);
}

static SoundOps *setup_nau8318(const struct audio_amp *amp, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_NAU8318_BEEP))
		return NULL;

	GpioCfg *gpio_en = new_platform_gpio_output(amp->gpio.enable_gpio, 0);
	GpioCfg *gpio_beep_en = new_platform_gpio_output(amp->gpio.beep_gpio, 0);

	nau8318Codec *nau8318 = new_nau8318_codec(&gpio_en->ops, &gpio_beep_en->ops);
	return &nau8318->ops;
}

static void setup_gpio_amp(const struct audio_amp *amp, SoundRoute *route)
{
	if (!CONFIG(DRIVER_SOUND_GPIO_AMP))
		return;

	GpioCfg *gpio = new_platform_gpio_output(amp->gpio.enable_gpio, 0);
	GpioAmpCodec *speaker_amp = new_gpio_amp_codec(&gpio->ops);
	list_insert_after(&speaker_amp->component.list_node,
			  &route->components);
}

static GpioCfg *cfg_i2s_gpio(const struct audio_bus *bus)
{
	if (!CONFIG(DRIVER_SOUND_I2S_USE_GPIO))
		return NULL;

	GpioCfg *gpio_cfg = new_platform_gpio_output(bus->i2s.enable_gpio.pad,
						  bus->i2s.enable_gpio.active_low);
	return gpio_cfg;
}

static I2sSource *setup_i2s(const struct audio_bus *bus)
{
	GpioCfg *gpio_cfg = cfg_i2s_gpio(bus);
#if CONFIG(INTEL_COMMON_I2S_ACE_3_x)
	I2s *i2s = new_i2s_dma_structure(bus->i2s.settings,
		default_if_zero(bus->i2s.depth, AUD_BITDEPTH),
		gpio_cfg ? &gpio_cfg->ops : NULL, bus->i2s.address);
#else
	I2s *i2s = new_i2s_structure(bus->i2s.settings,
		default_if_zero(bus->i2s.depth, AUD_BITDEPTH),
		gpio_cfg ? &gpio_cfg->ops : NULL, bus->i2s.address);
#endif
	I2sSource *i2s_source = new_i2s_source(&i2s->ops,
		default_if_zero(bus->i2s.rate, AUD_SAMPLE_RATE),
		default_if_zero(bus->i2s.ch, AUD_NUM_CHANNELS),
		default_if_zero(bus->i2s.volume, AUD_VOLUME));
	return i2s_source;
}

static SoundRoute *setup_i2s_route(const struct audio_bus *bus)
{
	if (!CONFIG(DRIVER_SOUND_ROUTE))
		return NULL;

	return new_sound_route(&setup_i2s(bus)->ops);
}

static Soundwire *setup_sndw(unsigned int link)
{
	if (!CONFIG(DRIVER_BUS_SOUNDWIRE))
		return NULL;

	return new_soundwire(link);
}

static SoundOps *setup_gpio_pwm(unsigned int pad)
{
	if (!CONFIG(DRIVER_SOUND_GPIO_EDGE_BUZZER))
		return NULL;

	GpioOps *sound_gpio = &new_platform_gpio_output(pad, 0)->ops;
	GpioEdgeBuzzer *buzzer = new_gpio_edge_buzzer(sound_gpio);
	return &buzzer->ops;
}

static void configure_audio_bus(const struct audio_bus *bus,
				struct audio_data *data)
{
	data->type = bus->type;
	switch (bus->type) {
	case AUDIO_I2S:
		data->route = setup_i2s_route(bus);
		break;

	case AUDIO_SNDW:
		data->sndw = setup_sndw(bus->sndw.link);
		break;

	case AUDIO_PWM:
		data->ops = setup_gpio_pwm(bus->pwm.pad);
		break;

	default:
		break;
	}
}

static SoundOps *setup_max98373_sndw(SndwOps *ops, unsigned int beep_ms)
{
	if (!CONFIG(DRIVER_SOUND_MAX98373_SNDW))
		return NULL;

	return &new_max98373_sndw(ops, beep_ms)->ops;
}

static SoundOps *setup_max98363_sndw(SndwOps *ops, unsigned int beep_ms)
{
	if (!CONFIG(DRIVER_SOUND_MAX98363_SNDW))
		return NULL;

	return &new_max98363_sndw(ops, beep_ms)->ops;
}

static SoundOps *setup_rt7xx_sndw(SndwOps *ops, unsigned int beep_ms)
{
	if (!CONFIG(DRIVER_SOUND_RT722_SNDW) &&
	    !CONFIG(DRIVER_SOUND_RT721_SNDW) &&
	    !CONFIG(DRIVER_SOUND_RT712_SNDW))
		return NULL;

	return &new_rt7xx_sndw(ops, beep_ms)->ops;
}

static SoundOps *setup_rt1320_sndw(SndwOps *ops, unsigned int beep_ms)
{
	if (!CONFIG(DRIVER_SOUND_RT1320_SNDW))
		return NULL;

	return &new_rt1320_sndw(ops, beep_ms)->ops;
}

static void configure_audio_codec(const struct audio_codec *codec,
				  struct audio_data *data)
{
	switch (codec->type) {
	case AUDIO_MAX98390:
		if (data->route) {
			setup_max98390(codec, data->route);
			data->ops = &data->route->ops;
		}
		break;

	case AUDIO_MAX98396:
		if (data->route) {
			setup_max98396(codec, data->route);
			data->ops = &data->route->ops;
		}
		break;

	case AUDIO_MAX98357:
		if (data->route)
			data->ops = &data->route->ops;
		break;

	case AUDIO_MAX98373:
		if (data->type == AUDIO_I2S) {
			setup_max98373(codec, data->route);
			data->ops = &data->route->ops;
		} else if (data->type == AUDIO_SNDW) {
			data->ops = setup_max98373_sndw(&data->sndw->ops, BEEP_DURATION);
		}
		break;

	case AUDIO_MAX98363:
		if (data->type == AUDIO_SNDW)
			data->ops = setup_max98363_sndw(&data->sndw->ops, BEEP_DURATION);
		break;

	case AUDIO_RT712:
	case AUDIO_RT721:
	case AUDIO_RT722:
		if (data->type == AUDIO_SNDW)
			data->ops = setup_rt7xx_sndw(&data->sndw->ops, BEEP_DURATION);
		break;

	case AUDIO_CS35L53:
		if (data->type == AUDIO_I2S) {
			setup_cs35l53(codec, data->route);
			data->ops = &data->route->ops;
		}
		break;

	case AUDIO_CS35L56:
		if (data->type == AUDIO_SNDW)
			data->ops = setup_cs35l56_sndw(&data->sndw->ops, BEEP_DURATION);
		break;

	case AUDIO_RT1019:
		if (data->route)
			data->ops = &data->route->ops;
		break;

	case AUDIO_RT1320:
		if (data->type == AUDIO_SNDW)
			data->ops = setup_rt1320_sndw(&data->sndw->ops, BEEP_DURATION);
		break;

	case AUDIO_RT5650:
		if (data->route) {
			setup_rt5650(codec, data->route);
			data->ops = &data->route->ops;
		}
		break;
	case AUDIO_ALC256:
		if (CONFIG(DRIVER_SOUND_HDA)) {
			if (data->type == AUDIO_HDA) {
				data->codec = new_hda_codec();
				sound_set_ops(&data->codec->ops);
				set_hda_beep_nid_override(data->codec, 1);
			}
		}
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
		if (amp->gpio.beep_gpio)
			data->ops = setup_nau8318(amp, data->route);
		else
			setup_gpio_amp(amp, data->route);
		break;
	default:
		break;
	}
}

void common_audio_setup(const struct audio_config *config)
{
	struct audio_data data = {};

	if (!config)
		return;

	configure_audio_bus(&config->bus, &data);
	configure_audio_codec(&config->codec, &data);
	configure_audio_amp(&config->amp, &data);

	if (data.ops)
		sound_set_ops(data.ops);
}


/*
 * audio_compare_codec_id - Compare a codec info ID with a reference ID.
 * @info: Pointer to the sndw_codec_info containing the ID to compare.
 * @ref_id: Pointer to the reference sndw_codec_id to compare against.
 *
 * This function iterates through the ID fields and returns true if they all match,
 * false otherwise.
 */
bool audio_compare_codec_id(const sndw_codec_info *info, const sndw_codec_id *ref_id)
{
	for (int i = 0; i < SNDW_DEV_ID_NUM; i++) {
		if (ref_id->id[i] != info->codecid.id[i])
			return false;
	}
	return true;
}

/*
 * sndw_get_audio_type() - Detects and returns the type of the SoundWire audio codec.
 * @link: The SoundWire link number to probe.
 *
 * This function attempts to identify the audio codec connected to the specified SoundWire
 * link. It first initializes the SoundWire bus, then probes and enables the codec.
 * If successful, it compares the codec's ID against a list of known SoundWire codec IDs
 * (e.g., RT712, RT721, MAX98363) to determine its type.
 *
 * Return: An enum audio_codec_type value indicating the detected codec, or AUDIO_CODEC_NONE
 * if no known codec is found or if enabling the codec fails.
 */
enum audio_codec_type audio_get_type(unsigned int link)
{
	Soundwire *bus = new_soundwire(link);
	sndw_codec_info info;

	if (sndw_probe_and_enable_codec(bus, &info)) {
		printf("Failed to enable codec.\n");
		return AUDIO_CODEC_NONE;
	}

	/* Check for all Soundwire Codec ID */
	if (CONFIG(DRIVER_SOUND_RT712_SNDW) && audio_compare_codec_id(&info, &rt712_id))
		return AUDIO_RT712;
	else if (CONFIG(DRIVER_SOUND_RT721_SNDW) && audio_compare_codec_id(&info, &rt721_id))
		return AUDIO_RT721;
	else if (CONFIG(DRIVER_SOUND_RT722_SNDW) && audio_compare_codec_id(&info, &rt722_id))
		return AUDIO_RT722;
	else if (CONFIG(DRIVER_SOUND_RT1320_SNDW) && audio_compare_codec_id(&info, &rt1320_id))
		return AUDIO_RT1320;
	else if (CONFIG(DRIVER_SOUND_MAX98363_SNDW) && audio_compare_codec_id(&info, &max98363_id))
		return AUDIO_MAX98363;
	else if (CONFIG(DRIVER_SOUND_MAX98373_SNDW) && audio_compare_codec_id(&info, &max98373_id))
		return AUDIO_MAX98373;
	else
		return AUDIO_CODEC_NONE;
}
