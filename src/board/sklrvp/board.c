/*
 * Copyright 2013 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
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

#include <pci.h>

#include <pci/pci.h>
#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/lynxpoint_lp.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"
#include "drivers/bus/usb/usb.h"

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	LpPchGpio *ec_in_rw = new_lp_pch_gpio_input(25);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

	CrosEcLpcBus *cros_ec_lpc_bus = new_cros_ec_lpc_bus();
	cros_ec_set_bus(&cros_ec_lpc_bus->ops);

	flash_set_ops(&new_mem_mapped_flash(0xff000000, 0x1000000)->ops);

	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 23, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	uintptr_t UsbMmioBase =
		pci_read_config32(PCI_DEV(0, 0x14, 0), PCI_BASE_ADDRESS_0);
	UsbMmioBase &= 0xFFFF0000; /* 32 bits only */
	UsbHostController *usb_host1 = new_usb_hc(XHCI, UsbMmioBase);
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	power_set_ops(&pch_power_ops);

	/* Enable TPM here when present on board. */

	return 0;
}

INIT_FUNC(board_setup);
