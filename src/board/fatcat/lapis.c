// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "drivers/ec/rts5453/rts5453.h"
#include "board/fatcat/include/variant.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "base/fw_config.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/bus/soundwire/soundwire.h"

#define EC_SOC_INT_ODL		GPP_E07

static const struct storage_config storage_configs[] = {
	{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE9 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

int gsc_irq_status(void)
{
	return pantherlake_get_gpe(GPE0_DW2_02); /* GPP_E02 */
}

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;
	if (CONFIG(DRIVER_SOUND_CS35L56_SNDW)) {
	config = (struct audio_config){
		.bus = {
			.type = AUDIO_SNDW,
			.sndw.link = AUDIO_SNDW_LINK2
		},
		.codec = {
			.type = AUDIO_CS35L56,
		},
	};
	}
	return &config;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV_I2C3,
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
