// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>

#include "base/fw_config.h"
#include "board/brya/include/variant.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/storage_common.h"

const struct audio_config *variant_probe_audio_config(void)
{
        static struct audio_config config;
        return &config;
}

static const struct storage_config storage_configs[] = {
        { .media = STORAGE_NVME, .pci_dev = PCH_DEV_PCIE10 },
        { .media = STORAGE_UFS, .pci_dev = PCH_DEV_UFS1 },
};

const struct storage_config *variant_get_storage_configs(size_t *count)
{
        *count = ARRAY_SIZE(storage_configs);
        return storage_configs;
}

const struct tpm_config *variant_get_tpm_config(void)
{
        static const struct tpm_config config = {
                .pci_dev = PCI_DEV(0, 0x15, 1),
        };

        return &config;
}
