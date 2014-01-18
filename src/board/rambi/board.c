/*
 * Copyright 2013 Google Inc.
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

#include "base/init_funcs.h"
#include "board/rambi/device_nvs.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/baytrail.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/lpc.h"
#include "vboot/util/flag.h"

/*
 * Clock frequencies for the eMMC port are 400 KHz min (used during
 * initialization) and 200 MHz max. Firmware does not run at 200 MHz
 * (aka HS mode), it is limited to 52 MHz.
 */
static const int emmc_clock_min = 400 * 1000;
static const int emmc_clock_max = 200 * 1000 * 1000;

static int board_setup(void)
{
        sysinfo_install_flags();

	/* ECRW GPIO: SCGPIO59 */
	PchGpio *ec_in_rw = new_baytrail_gpio_input(59 / 32,
						    59 % 32);
	flag_install(FLAG_ECINRW, &ec_in_rw->ops);

	CrosEcLpcBus *cros_ec_lpc_bus = new_cros_ec_lpc_bus();
	cros_ec_set_bus(&cros_ec_lpc_bus->ops);

	MemMappedFlash *flash = new_mem_mapped_flash(0xff800000, 0x800000);
	if (flash_set_ops(&flash->ops))
		return 1;

	/* TODO(shawnn): Init I2S audio codec here for FW beep */

	if (power_set_ops(&baytrail_power_ops))
		return 1;

	LpcTpm *tpm = new_lpc_tpm((void *)0xfed40000);
	if (tpm_set_ops(&tpm->ops))
		return 1;

	/* Initialize eMMC port in ACPI or PCI mode */
	SdhciHost *emmc;
	device_nvs_t *nvs = lib_sysinfo.acpi_gnvs + DEVICE_NVS_OFFSET;
	if (nvs->scc_en[SCC_NVS_MMC])
		emmc = new_mem_sdhci_host((void *)nvs->scc_bar0[SCC_NVS_MMC],
					  0, emmc_clock_min, emmc_clock_max);
	else
		emmc = new_pci_sdhci_host(PCI_DEV(0, 23, 0), 0,
					  emmc_clock_min, emmc_clock_max);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);
