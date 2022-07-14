// SPDX-License-Identifier: GPL-2.0
/* Copyright 2022 Google LLC.  */

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/skyrim/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/kern.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/power/fch.h"
#include "drivers/soc/mendocino.h"
#include "drivers/storage/nvme.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/tpm.h"
#include "pci.h"
#include "vboot/util/flag.h"

#define CR50_INT_18	18
#define EC_SOC_INT_ODL	84

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		/* Coreboot devicetree.cb: gpp_bridge_2 = 02.3 */
		{ .media = STORAGE_NVME, .pci_dev = PCI_DEV(0, 0x02, 0x3)},
		/* Coreboot devicetree.cb: gpp_bridge_1 = 02.2 */
		{ .media = STORAGE_RTKMMC, .pci_dev = PCI_DEV(0, 0x02, 0x02)},
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

static int cr50_irq_status(void)
{
	static KernGpio *tpm_gpio;

	if (!tpm_gpio)
		tpm_gpio = new_kern_fch_gpio_latched(CR50_INT_18);

	return gpio_get(&tpm_gpio->ops);
}

static GpioOps *mkbp_int_ops(void)
{
	KernGpio *mkbp_int_gpio = new_kern_fch_gpio_input(EC_SOC_INT_ODL);
	/* Active-low, has to be inverted */
	return new_gpio_not(&mkbp_int_gpio->ops);
}

static int board_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, mkbp_int_ops());
	register_vboot_ec(&cros_ec->vboot);

	sysinfo_install_flags(NULL);
	flag_replace(FLAG_LIDSW, cros_ec_lid_switch_flag());
	flag_replace(FLAG_PWRSW, cros_ec_power_btn_flag());

	/* Set up Dauntless on I2C3 */
	DesignwareI2c *i2c_ti50 = new_designware_i2c(
		AP_I2C3_ADDR, 400000, AP_I2C_CLK_MHZ);
	tpm_set_ops(&new_cr50_i2c(&i2c_ti50->ops, 0x50,
				  &cr50_irq_status)->base.ops);

	power_set_ops(&kern_power_ops);

	return 0;
}

INIT_FUNC(board_setup);
