/*
 * Copyright 2013 Google LLC
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

#include <pci/pci.h>

#include "base/init_funcs.h"
#include "board/cyan/device_nvs.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2s/braswell/braswell-max98090.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98090.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/tpm/lpc.h"
#include "vboot/util/acpi.h"
#include "vboot/util/flag.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
static const int emmc_sd_clock_min = 400 * 1000;
static const int emmc_clock_max = 200 * 1000 * 1000;
static const int sd_clock_max = 52 * 1000 * 1000;

#define DEVICE_NVS_OFFSET		64

static int board_setup(void)
{
	device_nvs_t *nvs = lib_sysinfo.acpi_gnvs + DEVICE_NVS_OFFSET;
	sysinfo_install_flags(NULL);
#if CONFIG(DRIVER_EC_CROS)
  #if CONFIG(DRIVER_EC_CROS_LPC)
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_MEC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);
  #endif
#endif

	power_set_ops(&braswell_power_ops);

	uintptr_t lpe_mmio = nvs->lpe_bar0;
	if (!nvs->lpe_en) {
		pcidev_t lpe_pcidev = PCI_DEV(0, 0x15, 0);
		lpe_mmio = pci_read_config32(lpe_pcidev, PCI_BASE_ADDRESS_0);
	}
	BswI2s *i2s = new_bsw_i2s(lpe_mmio, &braswell_max98090_settings,
				16, 2,4800000, 48000);

	I2sSource *i2s_source = new_i2s_source(&i2s->ops, 48000, 2, 16000);

	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	die_if(!nvs->lpss_en[LPSS_NVS_I2C2], "Codec I2C misconfigured\n");

	DesignwareI2c *i2c = new_designware_i2c(
		nvs->lpss_bar0[LPSS_NVS_I2C2], 400000, 133);

	Max98090Codec *codec = new_max98090_codec(
		&i2c->ops, 0x10, 16, 48000, 400, 1);

	list_insert_after(&codec->component.list_node,
			  &sound_route->components);

	sound_set_ops(&sound_route->ops);

	tpm_set_ops(&new_lpc_tpm((void *)0xfed40000)->ops);

	SdhciHost *emmc, *sd;

	if (nvs->scc_en[SCC_NVS_MMC])
		emmc = new_mem_sdhci_host(nvs->scc_bar0[SCC_NVS_MMC],
				     0, emmc_sd_clock_min, emmc_clock_max);
	else
		emmc = new_pci_sdhci_host(PCI_DEV(0, 0x10, 0), 0,
				emmc_sd_clock_min, emmc_clock_max);

	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	if (nvs->scc_en[SCC_NVS_SD])
		sd = new_mem_sdhci_host(nvs->scc_bar0[SCC_NVS_SD],
				    1, emmc_sd_clock_min, sd_clock_max);
	else
		sd = new_pci_sdhci_host(PCI_DEV(0, 0x12, 0), 1,
				emmc_sd_clock_min, sd_clock_max);

	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	uintptr_t UsbMmioBase = pci_read_config32( PCI_DEV(0, 0x14, 0),
					PCI_BASE_ADDRESS_0 );
	UsbMmioBase &= 0xFFFF0000;	// 32 bits only?
	UsbHostController *usb_host1 = new_usb_hc(XHCI, UsbMmioBase );
	list_insert_after(&usb_host1->list_node, &usb_host_controllers);

	return 0;
}

INIT_FUNC(board_setup);
