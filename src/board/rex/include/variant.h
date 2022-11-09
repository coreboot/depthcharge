#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>

#include "drivers/bus/i2s/intel_common/i2s.h"

#define MAX_CODEC	4
#define MAX_CTRL	2

enum audio_bus_type {
	AUDIO_I2S,
	AUDIO_SNDW,
	AUDIO_PWM,
};

enum audio_codec_type {
	AUDIO_CODEC_NONE,
	AUDIO_MAX98357,
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
	} gpio;
};

typedef struct {
	pcidev_t ctrlr;
	unsigned int i2c_addr[MAX_CODEC];
} i2c_t;

struct audio_codec {
	enum audio_codec_type type;
	i2c_t i2c[MAX_CTRL];
};

struct audio_config {
	struct audio_bus bus;
	struct audio_amp amp;
	struct audio_codec codec;
};

struct tpm_config {
	pcidev_t pci_dev;
};

const struct audio_config *variant_probe_audio_config(void);
const struct storage_config *variant_get_storage_configs(size_t *count);
void rex_configure_audio(const struct audio_config *config);

#endif /* _VARIANT_H_ */
