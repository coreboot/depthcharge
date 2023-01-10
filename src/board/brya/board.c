// SPDX-License-Identifier: GPL-2.0

/*
 * These needs to be included first.
 * Some of the driver headers would be dependent on these.
 */
#include <pci.h>
#include <pci/pci.h>

#include "base/init_funcs.h"
#include "base/list.h"
#include "board/brya/include/variant.h"
#include "drivers/bus/i2c/designware.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/ec/cros/lpc.h"
#include "drivers/flash/flash.h"
#include "drivers/gpio/sysinfo.h"
#include "drivers/gpio/alderlake.h"
#include "drivers/gpio/gpio.h"
#include "drivers/power/pch.h"
#include "drivers/soc/alderlake.h"
#include "drivers/sound/cs35l53.h"
#include "drivers/tpm/google/i2c.h"
#include "drivers/tpm/google/switches.h"
#include "drivers/tpm/tpm.h"
#include "vboot/util/flag.h"
#include <libpayload.h>
#include <sysinfo.h>

#define EC_PCH_INT_ODL	GPP_F17
#define TPM_I2C3	PCI_DEV(0, 0x15, 3)
#define TPM_I2C_ADDR	0x50
#define I2C_FS_HZ	400000

#define SSP_PORT_SPKR	2

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

static int cr50_irq_status(void)
{
	return alderlake_get_gpe(GPE0_DW0_13); /* GPP_A13 */
}

static GpioOps *mkbp_int_ops(void)
{
	GpioCfg *mkbp_int_gpio = new_alderlake_gpio_input(variant_get_ec_int());
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
	Cr50I2c *tpm = new_cr50_i2c(&i2c->ops, TPM_I2C_ADDR,
				    &cr50_irq_status);
	tpm_set_ops(&tpm->base.ops);
	if (CONFIG(DRIVER_TPM_CR50_SWITCHES))
		flag_replace(FLAG_PHYS_PRESENCE,
				&new_cr50_rec_switch(&tpm->base.ops)->ops);
}

static void audio_setup(void)
{
	brya_configure_audio(variant_probe_audio_config());
}

static int board_setup(void)
{
	sysinfo_install_flags(NULL);
	ec_setup();
	tpm_setup();
	power_set_ops(&alderlake_power_ops);
	audio_setup();

	return 0;
}

INIT_FUNC(board_setup);
