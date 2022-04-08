// SPDX-License-Identifier: GPL-2.0
/* Copyright 2022 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/sabrina.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/cr50_i2c.h"
#include "drivers/tpm/tpm.h"
#include "pci.h"
#include "vboot/util/flag.h"

#define CR50_INT_18	18

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT_18);

	return gpio_get(&tpm_gpio->ops);
}

static int get_gpio_0(struct GpioOps *me) {
	return 0;
}

static struct GpioOps fake_gpio_0 = {
	.get = get_gpio_0,
};


static int get_gpio_1(struct GpioOps *me) {
	return 1;
}

static struct GpioOps fake_gpio_1 = {
	.get = get_gpio_1,
};

static int board_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, NULL);
	register_vboot_ec(&cros_ec->vboot);

	sysinfo_install_flags(NULL);
	flag_replace(FLAG_LIDSW, &fake_gpio_1);
	flag_replace(FLAG_PWRSW, &fake_gpio_0);

	/* Set up Dauntless on I2C3 */
	DesignwareI2c *i2c_ti50 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_ti50->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	/* PCI Bridge for NVMe */
	NvmeCtrlr *nvme = new_nvme_ctrlr(PCI_DEV(0, 0x02, 0x3));
	list_insert_after(&nvme->ctrlr.list_node,
				&fixed_block_dev_controllers);

	power_set_ops(&kern_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
