// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020 Intel Corporation */

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/cavs-regs.h"
#include "drivers/bus/i2s/intel_common/max98357a.h"
#include "drivers/bus/usb/intel_tcss.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/pch.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/gpio_amp.h"
#include "drivers/sound/max98373.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include <libpayload.h>
#include <sysinfo.h>

#define TPM_I2C3	PCI_DEV(0, 0x15, 3)
#define TPM_I2C_ADDR	0x50

#define I2C_FS_HZ	400000

#define SSP_PORT_SPKR	2
#define AUD_VOLUME	4000
#define AUD_BITDEPTH	16
#define AUD_SAMPLE_RATE	48000
#define AUD_NUM_CHANNELS 2
#define SDMODE_PIN	GPP_A11

/*
 * Each USB Type-C port consists of a TCP (USB3) and a USB2 port from
 * the SoC. This table captures the mapping.
 *
 * SoC USB2 ports are numbered 1..10
 * SoC TCP (USB3) ports are numbered 0..3
 */
#define USBC_PORT_0_USB2_NUM    1
#define USBC_PORT_0_USB3_NUM    0

#define USBC_PORT_1_USB2_NUM    2
#define USBC_PORT_1_USB3_NUM    1

#define USBC_PORT_2_USB2_NUM    3
#define USBC_PORT_2_USB3_NUM    2

static const struct tcss_port_map typec_map[] = {
	[0] = { USBC_PORT_0_USB3_NUM, USBC_PORT_0_USB2_NUM },
	[1] = { USBC_PORT_1_USB3_NUM, USBC_PORT_1_USB2_NUM },
	[2] = { USBC_PORT_2_USB3_NUM, USBC_PORT_2_USB2_NUM },
};

int board_tcss_get_port_mapping(const struct tcss_port_map **map)
{
	if (map) {
		*map = typec_map;
		return ARRAY_SIZE(typec_map);
	}

	return 0;
}

static int cr50_irq_status(void)
{
	/* FIX ME: Confirm GSC_PCH_INT_ODL PIN GPE assume route to DW0 */
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

int board_get_ssp_port_index(void)
{
	return SSP_PORT_SPKR;
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);

	/* Chrome EC (eSPI) */
	if (CONFIG(DRIVER_EC_CROS_LPC)) {
		CrosEcLpcBus *cros_ec_lpc_bus =
				new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
		CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
		register_vboot_ec(&cros_ec->vboot);
	}

	if (CONFIG(DRIVER_TPM_CR50_I2C)) {
		DesignwareI2c *i2c3 = new_pci_designware_i2c(
			TPM_I2C3,
			I2C_FS_HZ, ALDERLAKE_DW_I2C_MHZ);
		tpm_set_ops(&new_cr50_i2c(&i2c3->ops, TPM_I2C_ADDR,
			&cr50_irq_status)->base.ops);
		printf("Using experimental I2C TPM\n");
	}

	/* Audio Setup (for boot beep) */
#if CONFIG_DRIVER_SOUND_GPIO_AMP
	GpioOps *sdmode = &new_alderlake_gpio_output(SDMODE_PIN, 0)->ops;
	I2s *i2s = new_i2s_structure(&max98357a_settings, AUD_BITDEPTH,
			sdmode, SSP_I2S2_START_ADDRESS);
	I2sSource *i2s_source = new_i2s_source(&i2s->ops, AUD_SAMPLE_RATE,
			AUD_NUM_CHANNELS, AUD_VOLUME);
	/* Connect the Audio codec to the I2s source */
	SoundRoute *sound_route = new_sound_route(&i2s_source->ops);
	GpioAmpCodec *speaker_amp = new_gpio_amp_codec(sdmode);
	list_insert_after(&speaker_amp->component.list_node,
			&sound_route->components);
	sound_set_ops(&sound_route->ops);
#endif
	/* PCH Power */
	power_set_ops(&alderlake_power_ops);

	/* SATA AHCI TODO: confirm supported or not */
	AhciCtrlr *ahci = new_ahci_ctrlr(PCI_DEV(0, 0x17, 0));
	list_insert_after(&ahci->ctrlr.list_node, &fixed_block_dev_controllers);

	/* NVME SSD */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x1d, 0));
	list_insert_after(&nvme->ctrlr.list_node, &fixed_block_dev_controllers);

	return 0;
}

INIT_FUNC(board_setup);
