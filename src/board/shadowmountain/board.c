// SPDX-License-Identifier: GPL-2.0
/* Copyright 2021 Intel Corporation. */

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
#include "drivers/bus/spi/intel_gspi.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/power/pch.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/max98373.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci_gli.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/spi.h"
#include "drivers/tpm/tpm.h"

#include "drivers/bus/usb/tigerlake_tcss.h"

#define TPM_I2C1	PCI_DEV(0, 0x15, 1)
#define TPM_I2C_ADDR	0x50

#define EMMC_CLOCK_MIN  400000
#define EMMC_CLOCK_MAX  200000000
#define GENESYS_PCI_VID 0x17a0
#define GL9763E_PCI_DID 0xe763

#define AUD_VOLUME	4000
#define AUD_BITDEPTH	16
#define AUD_SAMPLE_RATE	48000
#define AUD_NUM_CHANNELS 2
#define SDMODE_PIN	GPP_A23

#define AUD_I2C2	PCI_DEV(0, 0x15, 2)
#define AUD_I2C_ADDR	0x32
#define I2C_FS_HZ	400000


/*
 * Each USB Type-C port consists of a TCP (USB3) and a USB2 port from
 * the SoC. This table captures the mapping.
 *
 * SoC USB2 ports are numbered 1..10
 * SoC USB3.1 ports are numbered 1..4 (not used here)
 * SoC TCP (USB3) ports are numbered 0..3
 */
#define USBC_PORT_0_USB2_NUM	CONFIG_SHADOWMOUNTAIN_USBC_PORT_0_USB2_NUM
#define USBC_PORT_0_USB3_NUM	CONFIG_SHADOWMOUNTAIN_USBC_PORT_0_USB3_NUM
#define USBC_PORT_1_USB2_NUM	CONFIG_SHADOWMOUNTAIN_USBC_PORT_1_USB2_NUM
#define USBC_PORT_1_USB3_NUM	CONFIG_SHADOWMOUNTAIN_USBC_PORT_1_USB3_NUM

static const struct tcss_map typec_map[] = {
	{ USBC_PORT_0_USB2_NUM, USBC_PORT_0_USB3_NUM, 0 },
	{ USBC_PORT_1_USB2_NUM, USBC_PORT_1_USB3_NUM, 1 },
};

static int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_03); /* GPP_C3 */
}

static void shadowmountain_setup_tpm(void)
{
	if (CONFIG(DRIVER_TPM_SPI)) {
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
}

static uintptr_t get_ssp_base_address(void)
{
	switch (CONFIG(SHADOWMOUNTAIN_MAX98373_I2S_PORT)) {
	case 1:
		return SSP_I2S1_START_ADDRESS;
	case 2:
	default:
		return SSP_I2S2_START_ADDRESS;
	}
}

/*
 * get ssp port index for this particular config
 */
int board_get_ssp_port_index(void)
{
	return CONFIG_SHADOWMOUNTAIN_MAX98373_I2S_PORT;
}

static void shadowmountain_setup_max98373(void)
{
	I2s *i2s = new_i2s_structure(&max98373_settings, AUD_BITDEPTH, 0,
			get_ssp_base_address());
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
			AUD_NUM_CHANNELS, AUD_VOLUME);
	/* Connect Audio codec to the I2S source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	/* Speaker amp is Maxim 98373 codec on I2C0 */
	DesignwareI2c *i2c0 = new_pci_designware_i2c(AUD_I2C2, I2C_FS_HZ,
			ALDERLAKE_DW_I2C_MHZ);
	Max98373Codec *speaker_amp = new_max98373_codec(&i2c0->ops, AUD_I2C_ADDR);
	list_insert_after(&speaker_amp->component.list_node,
			&sound_route->components);
	sound_set_ops(&sound_route->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* TPM */
	shadowmountain_setup_tpm();

	/* Chrome EC (eSPI) */
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	/* PCH Power */
	power_set_ops(&alderlake_power_ops);

	/* Audio Setup (for boot beep) */
	if(CONFIG_DRIVER_SOUND_MAX98373)
		shadowmountain_setup_max98373();

	/* NVME SSD */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	/* SATA AHCI */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* TCSS ports */
	register_tcss_ports(typec_map, ARRAY_SIZE(typec_map));

	return 0;
}

INIT_FUNC(board_setup);
