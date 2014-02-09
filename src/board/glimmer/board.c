/*
 * Copyright 2014 Google Inc.
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
#include "board/glimmer/device_nvs.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2s/baytrail/baytrail-max98090.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/baytrail.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98090.h"
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
        if (sysinfo_install_flags())
		return 1;

	/* ECRW GPIO: SCGPIO59 */
	PchGpio *ec_in_rw = new_baytrail_gpio_input(59 / 32,
						    59 % 32);
	if (flag_install(FLAG_ECINRW, &ec_in_rw->ops))
		return 1;

	CrosEcLpcBus *cros_ec_lpc_bus = new_cros_ec_lpc_bus();
	cros_ec_set_bus(&cros_ec_lpc_bus->ops);

	MemMappedFlash *flash = new_mem_mapped_flash(0xff800000, 0x800000);
	if (flash_set_ops(&flash->ops))
		return 1;

	pcidev_t lpe_pcidev = PCI_DEV(0, 0x15, 0);
	BytI2s *i2s = new_byt_i2s(
		lpe_pcidev, &baytrail_max98090_settings, 16, 2, 4800000, 48000);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	DesignwareI2c *i2c = new_designware_i2c(0xd0824000, 400000);
	Max98090Codec *codec = new_max98090_codec(
		&i2c->ops, 0x10, 16, 48000, 400, 1);
	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

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
