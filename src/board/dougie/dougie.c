// SPDX-License-Identifier: GPL-2.0-only

#include <libpayload.h>
#include "drivers/soc/dougie.h"
#include "variant.h"

#define NVME_PCI_DEV PCI_DEV(0x1, 0, 0)

pcidev_t variant_get_nvme_pcidev(void)
{
	return NVME_PCI_DEV;
}

uintptr_t variant_get_ec_spi_base(void)
{
	return 0;
}

uintptr_t variant_get_gsc_i2c_base(void)
{
	return 0;
}
