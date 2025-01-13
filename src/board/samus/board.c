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

#include <pci.h>
#include <pci/pci.h>
#include <libpayload.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/samus/device_nvs.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/broadwell/broadwell.h"
#include "drivers/bus/i2s/broadwell/broadwell-alc5677.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/lynxpoint_lp.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/route.h"
#include "drivers/sound/rt5677.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/tpm/lpc.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/video/intel_i915.h"
#include "vboot/util/acpi.h"
#include "vboot/util/flag.h"

#define DEVICE_NVS_OFFSET		48

enum {
	/* Power Control and Status to enable/disable D3Hot */
	PCH_REG_PCS = 0x84,
	PCH_VAL_PCS_D3HOT = 3,

	/* Power register to enable and disable clock */
	SIO_REG_PPR_CLOCK = 0x800,
	SIO_BIT_PPR_CLOCK_EN = (1 << 0),

	/* 400 KHz bus speed */
	I2C_BUS_SPEED = 400000,
};

/* Put device in D0 state */
static void device_enable(int sio_index)
{
	device_nvs_t *nvs = lib_sysinfo.acpi_gnvs + DEVICE_NVS_OFFSET;
	void *reg_pcs = (void *)nvs->bar1[sio_index] + PCH_REG_PCS;

	/* Put device in D0 state */
	write32(reg_pcs, read32(reg_pcs) & ~PCH_VAL_PCS_D3HOT);
	(void)read32(reg_pcs);
}

static DesignwareI2c *i2c_enable(int sio_index)
{
	device_nvs_t *nvs = lib_sysinfo.acpi_gnvs + DEVICE_NVS_OFFSET;
	void *reg_ppr = (void *)nvs->bar0[sio_index] + SIO_REG_PPR_CLOCK;

	device_enable(sio_index);

	/* Enable clock to the device */
	write32(reg_ppr, read32(reg_ppr) | SIO_BIT_PPR_CLOCK_EN);
	(void)read32(reg_ppr);

	return new_designware_i2c(nvs->bar0[sio_index], I2C_BUS_SPEED, 166);
}

static BdwI2s *i2s_enable(int ssp)
{
	device_nvs_t *nvs = lib_sysinfo.acpi_gnvs + DEVICE_NVS_OFFSET;

	device_enable(SIO_NVS_ADSP);

	return new_bdw_i2s(nvs->bar0[SIO_NVS_ADSP], ssp, 16,
		&broadwell_alc5677_settings);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 31, 2));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	power_set_ops(&pch_power_ops);

	tpm_set_ops(&new_lpc_tpm((void *)(uintptr_t)0xfed40000)->ops);

	/* Setup sound route via I2S to RT5677 codec */
	BdwI2s *i2s = i2s_enable(0);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, 48000, 2, 16000);
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);

	DesignwareI2c *i2c0 = i2c_enable(SIO_NVS_I2C0);
	rt5677Codec *codec = new_rt5677_codec(&i2c0->ops, 0x2c, 16,
					      48000, 256, 0, 0);

	list_insert_after(&codec->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);

	if (display_init_required()) {
		uintptr_t i915_base = pci_read_config32(PCI_DEV(0, 2, 0),
						PCI_BASE_ADDRESS_0) & ~0xf;
		display_set_ops(new_intel_i915_display(i915_base));
	}

	return 0;
}

INIT_FUNC(board_setup);
