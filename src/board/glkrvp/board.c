/*
 * Copyright (C) 2017 Intel Corporation.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/flash/memmapped.h"
#include "drivers/tpm/tpm.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/power/pch.h"
#include "drivers/tpm/lpc.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/ahci.h"

#define EMMC_SD_CLOCK_MIN       400000
//GLK A0 PO: eMMC clock must be less than 50MHz for now
#define EMMC_CLOCK_MAX          25000000
#define SD_CLOCK_MAX            52000000

#define SPIBAR_BIOS_BFPREG	(0x0)
#define  BFPREG_BASE_MASK       (0x7fff)
#define  BFPREG_LIMIT_SHIFT     (16)
#define  BFPREG_LIMIT_MASK      (0x7fff << BFPREG_LIMIT_SHIFT)

static void board_flash_init(void)
{
	uintptr_t mmio_base = pci_read_config32(PCI_DEV(0, 0xd, 2),
						PCI_BASE_ADDRESS_0);
	mmio_base &= PCI_BASE_ADDRESS_MEM_MASK;

	uint32_t val = read32((void *)(mmio_base + SPIBAR_BIOS_BFPREG));

	uintptr_t mmap_start;
	size_t bios_base, bios_end, mmap_size;

	bios_base = (val & BFPREG_BASE_MASK) * 4 * KiB;
	bios_end  = (((val & BFPREG_LIMIT_MASK) >> BFPREG_LIMIT_SHIFT) + 1) *
		4 * KiB;
	mmap_size = bios_end - bios_base;

	/* BIOS region is mapped directly below 4GiB. */
	mmap_start = 4ULL * GiB - mmap_size;

	printf("BIOS MMAP details:\n");
	printf("IFD Base Offset  : 0x%zx\n", bios_base);
	printf("IFD End Offset   : 0x%zx\n", bios_end);
	printf("MMAP Size        : 0x%zx\n", mmap_size);
	printf("MMAP Start       : 0x%lx\n", mmap_start);

	/* W25Q128FV SPI Flash */
	flash_set_ops(&new_mem_mapped_flash_with_offset(mmap_start, mmap_size,
							bios_base)->ops);
}

static int board_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus;
	CrosEc *cros_ec;

	sysinfo_install_flags(NULL);

	board_flash_init();

	/* SLB9670 SPI TPM */
	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	uintptr_t UsbMmioBase =
		pci_read_config32(PCI_DEV(0, 0x15, 0), PCI_BASE_ADDRESS_0);
	UsbMmioBase &= 0xFFFF0000; /* 32 bits only */
	UsbHostController *usb_host1 = new_usb_hc(XHCI, UsbMmioBase);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1c, 0), 0,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card (if present) */
	pcidev_t sd_pci_dev = PCI_DEV(0, 0x1b, 0);
	uint16_t sd_vendor_id = pci_read_config32(sd_pci_dev, REG_VENDOR_ID);
	if (sd_vendor_id == PCI_VENDOR_ID_INTEL) {
		SdhciHost *sd = new_pci_sdhci_host(sd_pci_dev, 1,
					EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
					&removable_block_dev_controllers);
	}

	/* EC */
	cros_ec_lpc_bus = new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* PCH Power */
	power_set_ops(&apollolake_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
