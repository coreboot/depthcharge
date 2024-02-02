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

const struct audio_config *variant_probe_audio_config(void)
{
	static struct audio_config config;

	memset(&config, 0, sizeof(config));

	return &config;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	static struct tpm_config config = {
		.pci_dev = PCI_DEV(0, 0x15, 1),
	};

	if (lib_sysinfo.board_id < 4)
		config.pci_dev = PCI_DEV(0, 0x15, 3);

	return &config;
}
