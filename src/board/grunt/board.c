/*
 * Copyright 2017 Google Inc.
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
 */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/stoneyridge.h"
#include "drivers/sound/sound.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/bus/usb/usb.h"
#include "vboot/util/flag.h"

#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

#define FLASH_SIZE		0x1000000
#define FLASH_START		( 0xffffffff - FLASH_SIZE + 1 )

static int cr50_irq_status(void)
{
	return stoneyridge_get_gpe(22);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	flash_set_ops(&new_mem_mapped_flash(FLASH_START, FLASH_SIZE)->ops);

	/* TODO: Add Audio information (b:69407112) */

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x14, 7),
			SDHCI_PLATFORM_NO_EMMC_HS200 |
			SDHCI_PLATFORM_CLEAR_TRANSFER_BEFORE_CMD,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* Setup h1 on I2C1 */
	DesignwareI2c *i2c_h1 = new_designware_i2c(
		AP_I2C1_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_h1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	uintptr_t usb_base;
	UsbHostController *usb_ehci;
	UsbHostController *usb_xchi;
	usb_base = (uintptr_t)pci_read_config32(PCI_DEV(0, 18, 0),
			PCI_BASE_ADDRESS_0) & PCI_BASE_ADDRESS_MEM_MASK;
	if (usb_base){
		usb_ehci = new_usb_hc(EHCI, usb_base);
		list_insert_after(&usb_ehci->list_node, &usb_host_controllers);
	}

	usb_base = (uintptr_t)pci_read_config32(PCI_DEV(0, 16, 0),
			PCI_BASE_ADDRESS_0) & PCI_BASE_ADDRESS_MEM_MASK;
	if (usb_base){
		usb_xchi = new_usb_hc(XHCI, usb_base);
		list_insert_after(&usb_xchi->list_node, &usb_host_controllers);
	}

	power_set_ops(&kern_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
