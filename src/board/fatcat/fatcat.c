// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"

/* TODO: Implement override for `variant_probe_audio_config` */
/* TODO: b/357011633 - add FW_CONFIG for storage selection */
static const struct storage_config storage_configs[] = {
        { .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE5 },
        { .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE1 },
};
const struct storage_config *variant_get_storage_configs(size_t *count)
{
        *count = ARRAY_SIZE(storage_configs);
        return storage_configs;
}
