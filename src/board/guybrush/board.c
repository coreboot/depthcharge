// SPDX-License-Identifier: GPL-2.0
/* Copyright 2021 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/cros_ec_tunnel.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/ec/anx3429/anx3429.h"
#include "drivers/ec/ps8751/ps8751.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/cezanne.h"
#include "drivers/sound/rt1019.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/bayhub.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/display.h"
#include "drivers/bus/usb/usb.h"
#include "pci.h"
#include "vboot/util/flag.h"

/* SD Controllers */
#define BH720_PCI_VID		0x1217
#define BH720_PCI_DID		0x8621

#define GENESYS_PCI_VID		0x17a0
#define GL9755S_PCI_DID		0x9755
#define GL9750S_PCI_DID		0x9750

/* Clock frequencies */
#define MCLK			4800000
#define LRCLK			8000

/* Core boost register */
#define HW_CONFIG_REG		0xc0010015
#define   HW_CONFIG_CPBDIS	(1 << 25)

/* cr50 / Ti50 interrupt is attached to GPIO_3 */
#define CR50_INT		3

/* eDP backlight */
#define GPIO_BACKLIGHT		129

/* Audio Configuration */
#define AUDIO_I2C_MMIO_ADDR	0xfedc4000
#define AUDIO_I2C_SPEED		400000
#define I2C_DESIGNWARE_CLK_MHZ	150

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT);

	return gpio_get(&tpm_gpio->ops);
}

static int guybrush_backlight_update(DisplayOps *me, uint8_t enable)
{
	/* Backlight signal is active low */
	static KernGpio *backlight;

	if (!backlight)
		backlight = new_kern_fch_gpio_output(GPIO_BACKLIGHT, !enable);
	else
		backlight->ops.set(&backlight->ops, !enable);

	return 0;
}

static void setup_rt1019_amp(void)
{
	DesignwareI2c *i2c = new_designware_i2c((uintptr_t)AUDIO_I2C_MMIO_ADDR,
				       AUDIO_I2C_SPEED, I2C_DESIGNWARE_CLK_MHZ);
	rt1019Codec *speaker_amp = new_rt1019_codec(&i2c->ops,
							AUD_RT1019_DEVICE_ADDR);
	SoundRoute *sound_route = new_sound_route(&speaker_amp->ops);

	list_insert_after(&speaker_amp->component.list_node,
						&sound_route->components);
	sound_set_ops(&sound_route->ops);
}

static DisplayOps guybrush_display_ops = {
	.backlight_update = &guybrush_backlight_update,
};

static void setup_ec_in_rw_gpio(void)
{
	static const char * const mb = "Guybrush";
	struct cb_mainboard *mainboard = phys_to_virt(lib_sysinfo.cb_mainboard);

	/*
	 * Board versions 1 & 2 support H1 DB, but the EC_IN_RW signal is not
	 * routed. So add a dummy EC_IN_RW GPIO to emulate EC is trusted.
	 */
	if (!strcmp(cb_mb_part_string(mainboard), mb) &&
	    (lib_sysinfo.board_id == UNDEFINED_STRAPPING_ID ||
	     lib_sysinfo.board_id < 3))
		flag_install(FLAG_ECINRW, new_gpio_low());
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());
	setup_ec_in_rw_gpio();

	setup_rt1019_amp();

	SdhciHost *sd = NULL;
	pcidev_t pci_dev;
	if (pci_find_device(BH720_PCI_VID, BH720_PCI_DID, &pci_dev)) {
		sd = new_bayhub_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	} else if (pci_find_device(GENESYS_PCI_VID, GL9755S_PCI_DID,
				&pci_dev)) {
		sd = new_pci_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	} else if (pci_find_device(GENESYS_PCI_VID, GL9750S_PCI_DID,
				&pci_dev)) {
		sd = new_pci_sdhci_host(pci_dev,
				SDHCI_PLATFORM_REMOVABLE,
				0, 0);
	}

	if (sd) {
		sd->name = "SD";
		list_insert_after(&sd->mmc_ctrlr.ctrlr.list_node,
				&removable_block_dev_controllers);
	} else {
		printf("Failed to find SD card reader\n");
	}

	/* PCI Bridge for NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x02, 0x04));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	/* Set up H1 / Dauntless on I2C3 */
	DesignwareI2c *i2c_h1 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_h1->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);
	display_set_ops(&guybrush_display_ops);

	return 0;
}

INIT_FUNC(board_setup);
