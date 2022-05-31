// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "board/brya/include/variant.h"
#include "drivers/soc/alderlake.h"

const struct audio_config *variant_probe_audio_config(void)
{
	/* TODO(b/231582187): Implement driver for MAX98396 */
	static const struct audio_config audio_config;

	return &audio_config;
}

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCH_DEV_PCIE8 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
	/* TPM is on I2C port 1 */
	static struct tpm_config config = {
		.pci_dev = PCI_DEV(0, 0x15, 1),
	};

	return &config;
}
