// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "drivers/ec/tps6699x/tps6699x.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "base/fw_config.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/bus/soundwire/soundwire.h"

#define EC_SOC_INT_ODL		GPP_E08

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE1 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW2_14); /* GPP_E14 */
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	if (CONFIG(DRIVER_SOUND_RT1321_SNDW)&& CONFIG(DRIVER_SOUND_RT721_SNDW)) {
		unsigned int link = AUDIO_SNDW_LINK3;
		enum audio_codec_type audio_type = audio_get_type(link);

		if (audio_type == AUDIO_CODEC_NONE)
			return NULL;

		config = (struct audio_config){
			.bus = {
				.type = AUDIO_SNDW,
				.sndw.link = link,
			},
			.codec = {
				.type = audio_type,
			},
		};
	}

	return &config;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV_I2C1,
	};

	return &config;
}

const int variant_get_ec_int(void)
{
	return EC_SOC_INT_ODL;
}

const CrosEcLpcBusVariant variant_get_ec_lpc_bus(void)
{
	return CROS_EC_LPC_BUS_RTK;
}

/* Override of func in src/drivers/ec/tps6699x/tps6699x.c */
void board_tps6699x_get_image_paths(const char **image_path, const char **hash_path,
				    int ec_pd_id, struct ec_response_pd_chip_info_v2 *r)
{
	switch (ec_pd_id) {
	case 0:
	case 1:
		*image_path = "tps6699x_GOOG0I00.bin";
		*hash_path = "tps6699x_GOOG0I00.hash";
		break;
	default:
		printf("Unknown ec_pd_id %d\n", ec_pd_id);
	}
}
