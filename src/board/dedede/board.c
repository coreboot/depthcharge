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
#include "base/fw_config.h"
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
#include "drivers/sound/gpio_amp.h"
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
#define EC_AP_MKBP_INT_L	GPP_C15
#define EN_SPK_PIN		GPP_D17
#define AUD_NUM_CHANNELS	2
#define AUD_I2C4		PCI_DEV(0, 0x19, 0)
#define SPEED_HZ		400000

#define MAX98360A_AMP_SRC	1
#define ALC1015_AMP_SRC		2

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

static void setup_rt1015_amp(void)
{
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
}

static void setup_gpio_amp(void)
{
	GpioOps *spk_en = &new_jasperlake_gpio_output(EN_SPK_PIN, 0)->ops;
	/* Use common i2s settings */
	I2s *i2s = new_i2s_structure(&max98357a_settings, AUD_BITDEPTH, spk_en,
				     SSP_I2S1_START_ADDRESS);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
					       AUD_NUM_CHANNELS, AUD_VOLUME);

	/* Connect the Codec to the I2S source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	/* MAX98360A is a GPIO based amplifier */
	GpioAmpCodec *speaker_amp = new_gpio_amp_codec(spk_en);

	list_insert_after(&speaker_amp->component.list_node,
			  &sound_route->components);
	sound_set_ops(&sound_route->ops);
}

static uint8_t get_amp_source(void)
{
	if (!fw_config_is_provisioned() ||
	    fw_config_probe(FW_CONFIG(AUDIO_AMP, UNPROVISIONED)))
		return CONFIG_DEFAULT_AMP_SOURCE;

	if (fw_config_probe(FW_CONFIG(AUDIO_AMP, MAX98360)) ||
	    fw_config_probe(FW_CONFIG(AUDIO_AMP, RT1015P_AUTO)))
		return MAX98360A_AMP_SRC;

	if (fw_config_probe(FW_CONFIG(AUDIO_AMP, RT1015_I2C)))
		return ALC1015_AMP_SRC;

	return CONFIG_DEFAULT_AMP_SOURCE;
}

static void setup_audio_amp(void)
{
	uint8_t amp_source = get_amp_source();

	switch (amp_source) {
	case MAX98360A_AMP_SRC:
		setup_gpio_amp();
		break;
	case ALC1015_AMP_SRC:
		setup_rt1015_amp();
		break;
	default:
		printf("%s: Unsupported amp id %d\n", __func__, amp_source);
	}
}

static GpioOps *mkbp_int_ops(void)
{
	GpioCfg *mkbp_int_gpio = new_jasperlake_gpio_input(EC_AP_MKBP_INT_L);
	/* Active-low, has to be inverted */
	return new_gpio_not(&mkbp_int_gpio->ops);
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
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, mkbp_int_ops());
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
	setup_audio_amp();

	return 0;
}

INIT_FUNC(board_setup);
