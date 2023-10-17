// SPDX-License-Identifier: GPL-2.0

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
#include "board/brox/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/memmapped.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/power/pch.h"
#include "drivers/soc/alderlake.h"
#include "drivers/storage/storage_common.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/switches.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"

#define EC_PCH_INT_ODL	GPP_F17
#define TPM_I2C3	PCI_DEV(0, 0x15, 3)
#define I2C_FS_HZ	400000

__weak const struct storage_config *variant_get_storage_configs(size_t *count)
{
	static const struct storage_config storage_configs[] = {
		{ .media = STORAGE_NVME, .pci_dev = PCH_DEV_PCIE8 },
		{ .media = STORAGE_SDHCI, .pci_dev = PCH_DEV_PCIE7 },
	};

	*count = ARRAY_SIZE(storage_configs);
	return storage_configs;
}

__weak const struct tpm_config *variant_get_tpm_config(void)
{
	static const struct tpm_config config = {
		.pci_dev = PCI_DEV(0, 0x15, 3),
	};

	return &config;
}

__weak const int variant_get_ec_int(void)
{
	return EC_PCH_INT_ODL;
}

__weak int gsc_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

static GpioOps *mkbp_int_ops(void)
{
	GpioCfg *mkbp_int_gpio = new_platform_gpio_input(variant_get_ec_int());
	/* Active-low, has to be inverted */
	return new_gpio_not(&mkbp_int_gpio->ops);
}

static void ec_setup(void)
{
	CrosEcLpcBus *cros_ec_lpc_bus =
		new_cros_ec_lpc_bus(CROS_EC_LPC_BUS_GENERIC);
	CrosEc *cros_ec = new_cros_ec(&cros_ec_lpc_bus->ops, mkbp_int_ops());
	register_vboot_ec(&cros_ec->vboot);
}

static void tpm_setup(void)
{
	const struct tpm_config *config = variant_get_tpm_config();
	DesignwareI2c *i2c = new_pci_designware_i2c(config->pci_dev,
						    I2C_FS_HZ,
						    ALDERLAKE_DW_I2C_MHZ);
	GscI2c *tpm = new_gsc_i2c(&i2c->ops, GSC_I2C_ADDR,
				    &gsc_irq_status);
	tpm_set_ops(&tpm->base.ops);
	if (CONFIG(TPM_GOOGLE_SWITCHES))
		flag_replace(FLAG_PHYS_PRESENCE,
				&new_gsc_rec_switch(&tpm->base.ops)->ops);
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	ec_setup();
	tpm_setup();
	power_set_ops(&alderlake_power_ops);
	flash_set_ops(&new_mmap_flash()->ops);
	common_audio_setup(variant_probe_audio_config());

	return 0;
}

INIT_FUNC(board_setup);
