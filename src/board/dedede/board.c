// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * Board file for Dedede.
 */

#include <assert.h>
#include <pci.h>
#include <pci/pci.h>
#include <stdbool.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/jasperlake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/jasperlake.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/sound/route.h"
#include "drivers/sound/rt1015.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"

/*
 * Clock frequencies for the eMMC and SD ports are defined below. The minimum
 * frequency is the same for both interfaces, the firmware does not run any
 * interface faster than 52 MHz, but defines maximum eMMC frequency as 200 MHz
 * for proper divider settings.
 */
#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000
#define SD_CLOCK_MAX		52000000

#define AUD_VOLUME		4000
#define AUD_BITDEPTH		16
#define AUD_SAMPLE_RATE		48000
#define EN_SPK_PIN		GPP_D17
#define AUD_NUM_CHANNELS	2
#define AUD_I2C4		PCI_DEV(0, 0x19, 0)
#define SPEED_HZ		400000

static int cr50_irq_status(void)
{
	return jasperlake_get_gpe(GPE0_DW0_04); /* GPP_B4 */
}

static void dedede_setup_tpm(void)
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

static void dedede_setup_flash(void)
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

static bool is_rt1015_codec(void)
{
	static const char * const board_str[] = {"Boten",
						 "Magolor",
						 "Waddledee"};
	struct cb_mainboard *mainboard =
		phys_to_virt(lib_sysinfo.cb_mainboard);
	int i;

	for (i = 0; i < ARRAY_SIZE(board_str); i++) {
		if (!strncmp(cb_mb_part_string(mainboard), board_str[i],
							strlen(board_str[i])))
			return true;
	}
	return false;
}

static int board_setup(void)
{
	sysinfo_install_flags(new_jasperlake_gpio_input_from_coreboot);

	/* 16MB/32MB SPI Flash */
	dedede_setup_flash();

	/* TPM */
	dedede_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	/* PCH Power */
	power_set_ops(&jasperlake_power_ops);

	/* eMMC */
	SdhciHost *emmc = new_pci_sdhci_host((PCH_DEV_EMMC),
			SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* SD Card */
	SdhciHost *sd = new_pci_sdhci_host(PCH_DEV_SDCARD, 1,
			EMMC_SD_CLOCK_MIN, SD_CLOCK_MAX);
	list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
			&removable_block_dev_controllers);

	/* Audio Beep Support */
	if (is_rt1015_codec()) {
		/*
		 * ALC1015 codec uses internal control to shut down, so we can
		 * leave EN_SPK_PIN/SDB gpio pad alone.
		 * max98357a_settings is the common I2sSettings for cAVS i2s.
		 */
		I2s *i2s = new_i2s_structure(&max98357a_settings, AUD_BITDEPTH, 0,
					     SSP_I2S1_START_ADDRESS);
		I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
						       AUD_NUM_CHANNELS, AUD_VOLUME);
		SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
		/* Setup i2c */
		DesignwareI2c *i2c = new_pci_designware_i2c(AUD_I2C4, SPEED_HZ,
							    JASPERLAKE_DW_I2C_MHZ);
		rt1015Codec *speaker_amp = new_rt1015_codec(&i2c->ops,
							    AUD_RT1015_DEVICE_ADDR);

		list_insert_after(&speaker_amp->component.list_node,
				  &sound_route->components);
		sound_set_ops(&sound_route->ops);
	} else {
		GpioOps *spk_en = &new_jasperlake_gpio_output(EN_SPK_PIN, 0)->ops;
		/* Use common i2s settings */
		I2s *i2s = new_i2s_structure(&max98357a_settings, AUD_BITDEPTH, spk_en,
					     SSP_I2S1_START_ADDRESS);
		I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
						       AUD_NUM_CHANNELS, AUD_VOLUME);

		/* Connect the Codec to the I2S source */
		SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
		/* Re-use max98357aCodec for max98360a */
		max98357aCodec *speaker_amp = new_max98357a_codec(spk_en);

		list_insert_after(&speaker_amp->component.list_node,
				  &sound_route->components);
		sound_set_ops(&sound_route->ops);
	}

	return 0;
}

INIT_FUNC(board_setup);
