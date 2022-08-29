// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/skyrim/include/variant.h"

const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		/* Coreboot devicetree.cb: gpp_bridge_2 = 02.3 */
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x3)},
		/* Coreboot devicetree.cb: gpp_bridge_1 = 02.2 */
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x02)},
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

enum audio_amp variant_get_audio_amp(void)
{
	return AUDIO_AMP_RT1019;
}
