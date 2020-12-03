// SPDX-License-Identifier: GPL-2.0
/* Copyright 2020 Google LLC.  */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <libpayload.h>
#include <pci.h>
#include <pci/pci.h>
#include <sysinfo.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/tigerlake.h"
#include "drivers/power/pch.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/max98373.h"
#include "drivers/soc/tigerlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

#include "drivers/bus/usb/tigerlake_tcss.h"

#define AUD_VOLUME	4000
#define AUD_BITDEPTH	16
#define AUD_SAMPLE_RATE	48000
#define SDMODE_PIN	GPP_A10

#define AUD_I2C0	PCI_DEV(0, 0x15, 0)
#define AUD_I2C_ADDR	0x32
#define I2C_FS_HZ	400000

#if CONFIG_VOLTEER_MAX98373_I2S_PORT == 1
	#define AUD_GPIO_BCLK	GPP_A23
	#define AUD_GPIO_LRCLK	GPP_R7
#elif CONFIG_VOLTEER_MAX98373_I2S_PORT == 2
	#define AUD_GPIO_BCLK   GPP_A7
	#define AUD_GPIO_LRCLK  GPP_A8
#endif

/*
 * Each USB Type-C port consists of a TCP (USB3) and a USB2 port from
 * the SoC. This table captures the mapping.
 *
 * SoC USB2 ports are numbered 1..10
 * SoC USB3.1 ports are numbered 1..4 (not used here)
 * SoC TCP (USB3) ports are numbered 0..3
 */
#define USBC_PORT_0_USB2_NUM	CONFIG_VOLTEER_USBC_PORT_0_USB2_NUM
#define USBC_PORT_0_USB3_NUM	CONFIG_VOLTEER_USBC_PORT_0_USB3_NUM
#define USBC_PORT_1_USB2_NUM	CONFIG_VOLTEER_USBC_PORT_1_USB2_NUM
#define USBC_PORT_1_USB3_NUM	CONFIG_VOLTEER_USBC_PORT_1_USB3_NUM

static const struct tcss_port_map typec_map[] = {
	[0] = { USBC_PORT_0_USB3_NUM, USBC_PORT_0_USB2_NUM },
	[1] = { USBC_PORT_1_USB3_NUM, USBC_PORT_1_USB2_NUM },
};

int board_tcss_get_port_mapping(const struct tcss_port_map **map)
{
	if (map)
		*map = typec_map;

	return ARRAY_SIZE(typec_map);
}

static int cr50_irq_status(void)
{
	return tigerlake_get_gpe(GPE0_DW0_21); /* GPP_C21 */
}

static void volteer_setup_tpm(void)
{
	/* SPI TPM */
	const IntelGspiSetupParams gspi0_params = {
		.dev = PCI_DEV(0, 0x1e, 2),
		.cs_polarity = SPI_POLARITY_LOW,
		.clk_phase = SPI_CLOCK_PHASE_FIRST,
		.clk_polarity = SPI_POLARITY_LOW,
		.ref_clk_mhz = 100,
		.gspi_clk_mhz = 1,
	};
	tpm_set_ops(&new_tpm_spi(new_intel_gspi(&gspi0_params),
		cr50_irq_status)->ops);
}

#if CONFIG_DRIVER_SOUND_MAX98373
static void volteer_setup_max98373(void)
{
	/* Use GPIOs to provide a BCLK+LRCLK to the codec */
	GpioCfg *boot_beep_bclk = new_tigerlake_gpio_output(AUD_GPIO_BCLK, 0);
	GpioCfg *boot_beep_lrclk = new_tigerlake_gpio_output(AUD_GPIO_LRCLK, 0);

	/* Speaker amp is Maxim 98373 codec on I2C0 */
	DesignwareI2c *i2c0 = new_pci_designware_i2c(
			AUD_I2C0,
			I2C_FS_HZ, TIGERLAKE_DW_I2C_MHZ);

	Max98373Codec *tone_generator =
		new_max98373_tone_generator(&i2c0->ops, AUD_I2C_ADDR,
				AUD_SAMPLE_RATE,
				&boot_beep_bclk->ops,
				&boot_beep_lrclk->ops);
	sound_set_ops(&tone_generator->ops);
}
#endif

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* TPM */
	volteer_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	/* 32MB SPI Flash */
	flash_set_ops(&new_mem_mapped_flash(0xfe000000, 0x2000000)->ops);

	/* PCH Power */
	power_set_ops(&tigerlake_power_ops);

	/* Audio Setup (for boot beep) */
#if CONFIG_DRIVER_SOUND_GPIO_AMP
	GpioOps *sdmode = &new_tigerlake_gpio_output(SDMODE_PIN, 0)->ops;
	I2s *i2s = new_i2s_structure(&max98357a_settings, AUD_BITDEPTH,
			sdmode, SSP_I2S1_START_ADDRESS);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
			2, AUD_VOLUME);
	/* Connect the Audio codec to the I2s source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	GpioAmpCodec *speaker_amp = new_gpio_amp_codec(sdmode);
	list_insert_after(&speaker_amp->component.list_node,
			&sound_route->components);
	sound_set_ops(&sound_route->ops);
#endif
#if CONFIG_DRIVER_SOUND_MAX98373
	volteer_setup_max98373();
#endif

	/* NVME SSD */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	/* SATA AHCI */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);
