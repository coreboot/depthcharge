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
#include "config.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/soc/apollolake.h"
#include "drivers/storage/sdhci.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

#include "drivers/sound/i2s.h"
#include "drivers/sound/max98357a.h"
#include "drivers/gpio/apollolake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/i2s/cavs_1_5-regs.h"

#define EMMC_SD_CLOCK_MIN       400000
#define EMMC_CLOCK_MAX          200000000

#define  SPIBAR_BIOS_BFPREG	(0x0)
#define  BFPREG_BASE_MASK       (0x7fff)
#define  BFPREG_LIMIT_SHIFT     (16)
#define  BFPREG_LIMIT_MASK      (0x7fff << BFPREG_LIMIT_SHIFT)

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

#define EC_USB_PD_PORT_PS8751	1
#define EC_I2C_PORT_PS8751	2

static void update_ps8751_firmware(CrosEc * const cros_ec)
{
	CrosECTunnelI2c *cros_ec_i2c_tunnel =
		new_cros_ec_tunnel_i2c(cros_ec, EC_I2C_PORT_PS8751);
	Ps8751 *ps8751 = new_ps8751(cros_ec_i2c_tunnel, EC_USB_PD_PORT_PS8751);

	register_vboot_aux_fw(&ps8751->fw_ops);
}

static int board_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus;
	CrosEc *cros_ec;

	sysinfo_install_flags(NULL);

	board_flash_init();

	/* TPM */
	octopus_setup_tpm();

	SdhciHost *emmc;
	emmc = new_pci_sdhci_host(PCI_DEV(0, 0x1c, 0), SDHCI_PLATFORM_NO_EMMC_HS200,
			EMMC_SD_CLOCK_MIN, EMMC_CLOCK_MAX);
	list_insert_after(&emmc->mmc_ctrlr.ctrlr.list_node,
			&fixed_block_dev_controllers);

	/* EC */
	cros_ec_lpc_bus = new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, 0, NULL);
	register_vboot_ec(&cros_ec->vboot, 0);

	/* Peripherals connected to EC */
	update_ps8751_firmware(cros_ec);

	/* PCH Power */
	power_set_ops(&apollolake_power_ops);

	/* Audio Setup (for boot beep) */
	GpioOps *sdmode = &new_apollolake_gpio_output(SDMODE_PIN, 0)->ops;

	I2s *i2s = new_i2s_structure(&max98357a_settings, 16, sdmode,
			SSP_I2S1_START_ADDRESS);
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
