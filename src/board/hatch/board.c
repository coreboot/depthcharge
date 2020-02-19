/*
 * Copyright 2018 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arch/io.h>
#include <assert.h>
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/cannonlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/gpio/cannonlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/i2s/cavs_1_8-regs.h"
#include "drivers/sound/rt1011.h"

#define AUD_VOLUME              4000
#define SDMODE_PIN              GPP_H3
#define AUD_I2C4                PCI_DEV(0, 0x19, 0)

/* Clock frequencies for eMMC are defined below */
#define EMMC_CLOCK_MIN	400000
#define EMMC_CLOCK_MAX	200000000

/* Clock frequencies for the SD ports are defined below */
#define SD_CLOCK_MIN	400000
#define SD_CLOCK_MAX	52000000

static int cr50_irq_status(void)
{
	return cannonlake_get_gpe(GPE0_DW1_21);
}

static void hatch_setup_tpm(void)
{
	if (CONFIG(DRIVER_TPM_SPI)) {
		/* SPI TPM */
		const IntelGspiSetupParams gspi0_params = {
			.dev = PCI_DEV(0, 0x1e, 2),
			.cs_polarity = SPI_POLARITY_LOW,
			.clk_phase = SPI_CLOCK_PHASE_FIRST,
			.clk_polarity = SPI_POLARITY_LOW,
			.ref_clk_mhz = 120,
			.gspi_clk_mhz = 1,
		};
		tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0_params),
					 cr50_irq_status)->ops);
	}
}

static void hatch_setup_flash(void)
{
	/* Flash is mapped at the end of 4GiB */
	uintptr_t flash_start;
	uint32_t flash_size = lib_sysinfo.spi_flash.size;
	MemMappedFlash *flash;

	assert(flash_size != 0);

	flash_start = 4ULL * GiB - flash_size;
	flash = new_mem_mapped_flash(flash_start, flash_size);
	flash_set_ops(&flash->ops);
}

static int is_board_helios(void)
{
	static const char * const helios_str = "Helios";
	struct cb_mainboard *mainboard = lib_sysinfo.mainboard;

	return strncmp(cb_mb_part_string(mainboard),
			helios_str, strlen(helios_str)) == 0;
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* TPM */
	hatch_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	hatch_setup_flash();

	/* Cannonlake PCH */
	power_set_ops(&cannonlake_power_ops);

	/* eMMC */
	/* Attempt to read from the controller. If we get an invalid
	 * value, then it is not present and we should not attempt to
	 * initialize it. */
	pcidev_t emmc_pci_dev = PCI_DEV(0, 0x1a, 0);
	u32 vid = pci_read_config32(emmc_pci_dev, REG_VENDOR_ID);
	if (vid != 0xffffffff) {
		SdhciHost *emmc = new_pci_sdhci_host(emmc_pci_dev,
				SDHCI_PLATFORM_NO_EMMC_HS200,
				EMMC_CLOCK_MIN, EMMC_CLOCK_MAX);
		list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
				&fixed_block_dev_controllers);
	} else {
		printf("%s: Info: eMMC controller not present; skipping\n",
		       __func__);
	}

	/* Audio Setup (for boot beep) */
	if (is_board_helios()) {
		DesignwareI2c *i2c = new_pci_designware_i2c(AUD_I2C4,
					400000, CANNONLAKE_DW_I2C_MHZ);
		rt1011Codec *codec = new_rt1011_codec(&i2c->ops,
						AUD_RT1011_DEVICE_ADDR);
		SoundRoute *sound_route = new_sound_route(&codec->ops);

		list_insert_after(&codec->component.list_node,
				&sound_route->components);
		sound_set_ops(&sound_route->ops);
	} else {
		GpioOps *sdmode =
			&new_cannonlake_gpio_output(SDMODE_PIN, 0)->ops;
		I2s *i2s = new_i2s_structure(&max98357a_settings, 16, sdmode,
				SSP_I2S1_START_ADDRESS);
		I2sSource *i2s_source = new_i2s_source(&i2s->ops, 48000, 2,
							AUD_VOLUME);
		/* Connect the Codec to the I2S source */
		SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
		max98357aCodec *speaker_amp = new_max98357a_codec(sdmode);

		list_insert_after(&speaker_amp->component.list_node,
			&sound_route->components);
		sound_set_ops(&sound_route->ops);
	}

	/* SATA AHCI */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* NVMe SSD */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	/* SD Card */
	/* Attempt to read from the controller. If we get an invalid
	 * value, then it is not present and we should not attempt to
	 * initialize it. */
	pcidev_t sdhci_dev = PCI_DEV(0, 0x14, 5);
	u32 val = pci_read_config32(sdhci_dev, REG_VENDOR_ID);
	if (val != 0xffffffff) {
		SdhciHost *sd = new_pci_sdhci_host(sdhci_dev, 1,
				SD_CLOCK_MIN, SD_CLOCK_MAX);
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		printf("%s: Info: SDHCI controller not present; skipping\n",
		       __func__);
	}

	return 0;
}

INIT_FUNC(board_setup);
