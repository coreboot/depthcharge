/*
 * Copyright (C) 2016 Intel Corporation.
 * Copyright 2016 Google Inc.
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
 */

#include <arch/io.h>
#include <libpayload.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/soc/apollolake.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/power/pch.h"
#include "drivers/storage/sdhci.h"

#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/gpio/apollolake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/bus/i2s/apollolake/apollolake-max98357a.h"

#define EMMC_SD_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX		200000000
#define SD_CLOCK_MAX		52000000

#define SPIBAR_BIOS_BFPREG     (0x0)
#define  SPIBAR_BIOS_BFPREG	(0x0)
#define  BFPREG_BASE_MASK       (0x7fff)
#define  BFPREG_LIMIT_SHIFT     (16)
#define  BFPREG_LIMIT_MASK      (0x7fff << BFPREG_LIMIT_SHIFT)

#define AUD_VOLUME		4000
#define SDMODE_PIN		GPIO_76

#ifdef PD_SYNC
#error "PD_SYNC configuration incompatible with ec_tunnel"
#endif

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

static int cr50_irq_status(void)
{
	return apollolake_get_gpe(GPE0_DW1_28);
}

static int board_setup(void)
{
	CrosECTunnelI2c *cros_ec_i2c_tunnel;

	sysinfo_install_flags(NULL);

	/* EC */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* programmables downstream from the EC */
	cros_ec_i2c_tunnel = new_cros_ec_tunnel_i2c(cros_ec, /* i2c bus */ 0);
	Anx3429 *anx3429 = new_anx3429(cros_ec_i2c_tunnel, /* ec pd# */ 0);
	register_vboot_aux_fw(&anx3429->fw_ops);

	cros_ec_i2c_tunnel = new_cros_ec_tunnel_i2c(cros_ec, /* i2c bus */ 1);
	Ps8751 *ps8751 = new_ps8751(cros_ec_i2c_tunnel, /* ec pd# */ 1);
	register_vboot_aux_fw(&ps8751->fw_ops);

	board_flash_init();

	/* H1 TPM on I2C bus 2 */
	DesignwareI2c *i2c2 =
		new_pci_designware_i2c(PCI_DEV(0, 0x16, 2), 400000, 133);
	tpm_set_ops(&new_cr50_i2c(&i2c2->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1c, 0),
			SDHCI_PLATFORM_NO_EMMC_HS200,
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

	/* PCH Power */
	power_set_ops(&apollolake_power_ops);

	/* Audio Setup (for boot beep) */
	GpioOps *sdmode = &new_apollolake_gpio_output(SDMODE_PIN, 0)->ops;

	AplI2s *i2s = new_apl_i2s(&apollolake_max98357a_settings, 16, sdmode);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, 48000, 2, AUD_VOLUME);
	/* Connect the Codec to the I2S source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	max98357aCodec *speaker_amp = new_max98357a_codec(sdmode);

	list_insert_after(&speaker_amp->component.list_node,
			&sound_route->components);
	sound_set_ops(&sound_route->ops);

	return 0;
}

INIT_FUNC(board_setup);
