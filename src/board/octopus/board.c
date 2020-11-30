/*
 * Copyright (C) 2018 Intel Corporation.
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

#include <libpayload.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/apollolake.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

#include "drivers/sound/i2s.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/gpio/apollolake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/i2s/cavs-regs.h"

#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000

#define AUD_VOLUME              4000
#define SDMODE_PIN              GPIO_91

static int cr50_irq_status(void)
{
	return apollolake_get_gpe(GPE0_DW1_31); /* GPIO_63 */
}

static void octopus_setup_tpm(void)
{
	/* SPI TPM */
	const IntelGspiSetupParams gspi0_params = {
		.dev = PCI_DEV(0, 0x19, 0),
		.cs_polarity = SPI_POLARITY_LOW,
		.clk_phase = SPI_CLOCK_PHASE_FIRST,
		.clk_polarity = SPI_POLARITY_LOW,
		.ref_clk_mhz = 100,
		.gspi_clk_mhz = 1,
	};
	tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0_params),
				cr50_irq_status)->ops);
}

static int board_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus;
	CrosEc *cros_ec;

	sysinfo_install_flags(NULL);

	/* TPM */
	octopus_setup_tpm();

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1c, 0), SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* EC */
	cros_ec_lpc_bus = new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	/* PCH Power */
	power_set_ops(&apollolake_power_ops);

	/* Audio Setup (for boot beep) */
	GpioOps *sdmode = &new_apollolake_gpio_output(SDMODE_PIN, 0)->ops;

	I2s *i2s = new_i2s_structure(&max98357a_settings, 16, sdmode,
			SSP_I2S1_START_ADDRESS);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, 48000, 2, AUD_VOLUME);
	/* Connect the Codec to the I2S source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	GpioAmpCodec *speaker_amp = new_gpio_amp_codec(sdmode);

	list_insert_after(&speaker_amp->component.list_node,
		&sound_route->components);
	sound_set_ops(&sound_route->ops);

	return 0;
}

INIT_FUNC(board_setup);
