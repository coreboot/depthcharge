// SPDX-License-Identifier: GPL-2.0
/* Copyright 2021 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/cezanne.h"
#include "drivers/sound/gpio_edge_buzzer.h"
#include "drivers/storage/ahci.h"
#include "drivers/storage/blockdev.h"
#include "drivers/storage/sdhci.h"
#include "drivers/storage/bayhub.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "pci.h"
#include "vboot/util/flag.h"

/* SD Controllers */
#define BH720_PCI_VID		0x1217
#define BH720_PCI_DID		0x8621

#define GENESYS_PCI_VID		0x17a0
#define GL9755S_PCI_DID		0x9755
#define GL9750S_PCI_DID		0x9750

/* cr50 / Ti50 interrupt is attached to GPIO_3 */
#define CR50_INT		3

#define BUZZER_GPIO		91

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT);

	return gpio_get(&tpm_gpio->ops);
}

static void setup_buzzer(void)
{
	/* 'PWM_PP3300_BUZZER' */
	GpioOps *sound_gpio = &new_kern_fch_gpio_output(BUZZER_GPIO, 0)->ops;
	GpioEdgeBuzzer *buzzer = new_gpio_edge_buzzer(sound_gpio);
	sound_set_ops(&buzzer->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	flag_replace(FLAG_LIDSW, new_gpio_high());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

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

	/* Bootbeep setup */
	setup_buzzer();

	power_set_ops(&kern_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
