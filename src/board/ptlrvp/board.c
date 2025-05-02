// SPDX-License-Identifier: GPL-2.0

#include "base/init_funcs.h"
#include "board/ptlrvp/include/variant.h"
#include "drivers/flash/fast_spi.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/pantherlake.h"
#include "drivers/power/pch.h"
#include "drivers/soc/pantherlake.h"
#include "drivers/storage/storage_common.h"
#include "vboot/util/flag.h"

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV_PCIE9 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

static void flash_setup(void)
{
	/* 32MB SPI Flash */
	uintptr_t mmio_base = pci_read_config32(PCI_DEV_SPI,
						PCI_BASE_ADDRESS_0);
	mmio_base &= PCI_BASE_ADDRESS_MEM_MASK;
	FastSpiFlash *spi = new_fast_spi_flash(mmio_base);
	MmapFlash *flash = new_mmap_backed_flash(&spi->ops);
	flash_set_ops(&flash->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	power_set_ops(&pantherlake_power_ops);
	flash_setup();
	return 0;
}

INIT_FUNC(board_setup);
