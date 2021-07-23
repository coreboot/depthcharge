#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/bus/i2s/intel_common/i2s.h"

#define I2C0	PCI_DEV(0, 0x15, 0)

#define MAX_CODEC	2

enum audio_codec_type {
	AUDIO_CODEC_NONE,
	AUDIO_MAX98357,
	AUDIO_MAX98390,
};

enum audio_amp_type {
	AUDIO_AMP_NONE,
	AUDIO_GPIO_AMP,
};

struct audio_bus {
	struct {
		size_t address;
		unsigned int enable_gpio;
		const I2sSettings *settings;
	} i2s;
};

struct audio_amp {
	enum audio_amp_type type;
	struct {
		unsigned int enable_gpio;
	} gpio;
};

struct audio_codec {
	enum audio_codec_type type;
	struct {
		pcidev_t ctrlr;
		unsigned int i2c_addr[MAX_CODEC];
	} i2c;
};

struct audio_config {
	struct audio_bus bus;
	struct audio_amp amp;
	struct audio_codec codec;
};

const struct tcss_map *variant_get_tcss_map(size_t *count);
const struct audio_config *variant_probe_audio_config(void);

void brya_configure_audio(const struct audio_config *config);

#endif /* _VARIANT_H_ */
