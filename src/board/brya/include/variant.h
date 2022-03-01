#ifndef _VARIANT_H_
#define _VARIANT_H_

#include <libpayload.h>
#include <stdint.h>
#include <stddef.h>

#include "drivers/bus/i2s/intel_common/i2s.h"

#define I2C0	PCI_DEV(0, 0x15, 0)

#define MAX_CODEC	4

enum audio_bus_type {
	AUDIO_I2S,
	AUDIO_SNDW,
	AUDIO_PWM,
};

enum audio_codec_type {
	AUDIO_CODEC_NONE,
	AUDIO_MAX98357,
	AUDIO_MAX98373,
	AUDIO_MAX98390,
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

enum storage_media {
	STORAGE_NVME,
	STORAGE_SDHCI,
	STORAGE_EMMC,
	STORAGE_RTKMMC,
};

struct emmc_config {
	unsigned int platform_flags;
	unsigned int clock_min;
	unsigned int clock_max;
};

struct storage_config {
	enum storage_media media;
	pcidev_t pci_dev;
	struct emmc_config emmc;
};

struct tpm_config {
	pcidev_t pci_dev;
};

const struct audio_config *variant_probe_audio_config(void);
const struct storage_config *variant_get_storage_configs(size_t *count);
const struct tpm_config *variant_get_tpm_config(void);

void brya_configure_audio(const struct audio_config *config);

#endif /* _VARIANT_H_ */
