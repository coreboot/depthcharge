// SPDX-License-Identifier: GPL-2.0

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/i2s.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/alderlake.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/i2s.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/cr50_i2c.h"

#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000

#define AUD_VOLUME		4000
#define AUD_BITDEPTH		16
#define AUD_SAMPLE_RATE		48000
#define AUD_NUM_CHANNELS	2
#define EN_SPK_PIN		GPP_A11

#define TPM_I2C_ADDR	0x50
#define I2C_FS_HZ	400000

#define PCH_DEV_EMMC	PCI_DEV(0, 0x1a, 0)

static int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

static void ec_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);
}

static void tpm_setup(void)
{
	DesignwareI2c *i2c = new_pci_designware_i2c(PCI_DEV(0, 0x15, 0),
						    I2C_FS_HZ,
						    ALDERLAKE_DW_I2C_MHZ);
	Cr50I2c *tpm = new_cr50_i2c(&i2c->ops, TPM_I2C_ADDR,
				    &cr50_irq_status);
	tpm_set_ops(&tpm->base.ops);
}

static void audio_setup(void)
{
	GpioOps *spk_en = &new_alderlake_gpio_output(EN_SPK_PIN, 0)->ops;
	/* Use common i2s settings */
	I2s *i2s = new_i2s_structure(&max98357a_settings, AUD_BITDEPTH, spk_en,
				     SSP_I2S2_START_ADDRESS);
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

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	ec_setup();
	tpm_setup();
	power_set_ops(&alderlake_power_ops);
	audio_setup();

	/* eMMC */
	SdhciHost *emmc =
		new_pci_sdhci_host((PCH_DEV_EMMC),
				   SDHCI_PLATFORM_NO_EMMC_HS200,
				   EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			  &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);
