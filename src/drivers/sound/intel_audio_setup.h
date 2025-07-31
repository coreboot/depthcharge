#ifndef _AUDIO_SETUP_H_
#define _AUDIO_SETUP_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/bus/soundwire/mipi-sndwregs.h"

#define MAX_CODEC	4
#define MAX_CTRL	2

enum audio_bus_type {
	AUDIO_I2S,
	AUDIO_SNDW,
	AUDIO_PWM,
	AUDIO_HDA,
};

enum audio_codec_type {
	AUDIO_CODEC_NONE,
	AUDIO_MAX98357,
	AUDIO_MAX98373,
	AUDIO_MAX98363,
	AUDIO_MAX98390,
	AUDIO_CS35L53,
	AUDIO_MAX98396,
	AUDIO_RT1019,
	AUDIO_RT1320,
	AUDIO_RT5650,
	AUDIO_ALC256,
	AUDIO_ALC257,
	AUDIO_RT712,
	AUDIO_RT721,
	AUDIO_RT722,
};

enum audio_amp_type {
	AUDIO_AMP_NONE,
	AUDIO_GPIO_AMP,
};

struct audio_bus {
	enum audio_bus_type type;
	union {
		struct {
			size_t address;
			struct {
				unsigned int pad;
				bool active_low;
			} enable_gpio;
			const I2sSettings *settings;
			char ch;
			char depth;
			int rate;
			int volume;
		} i2s;
		struct {
			unsigned int link;
		} sndw;
		struct {
			unsigned int pad;
		} pwm;
	};
};

struct audio_amp {
	enum audio_amp_type type;
	struct {
		unsigned int enable_gpio;
		unsigned int beep_gpio;
	} gpio;
};

typedef struct {
	pcidev_t ctrlr;
	unsigned int i2c_addr[MAX_CODEC];
} i2c_t;

struct audio_codec {
	enum audio_codec_type type;
	i2c_t i2c[MAX_CTRL];
	int speed;
};

struct audio_config {
	struct audio_bus bus;
	struct audio_amp amp;
	struct audio_codec codec;
};

const struct audio_config *variant_probe_audio_config(void);
void common_audio_setup(const struct audio_config *config);

/* Detects and returns the type of the SoundWire audio codec. */
enum audio_codec_type audio_get_type(unsigned int link);

/* Compare a codec info ID with a reference ID. */
bool audio_compare_codec_id(const sndw_codec_info *info, const sndw_codec_id *ref_id);

#endif /* _AUDIO_SETUP_H_ */
